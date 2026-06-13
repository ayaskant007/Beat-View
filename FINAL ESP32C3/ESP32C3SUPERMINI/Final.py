import asyncio
import io
import time
import os
import sys
from dataclasses import dataclass, field
from typing import Optional, Tuple, List

import serial
from serial.serialutil import SerialException
from serial.tools import list_ports

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow")

try:
    from winsdk.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    )
    from winsdk.windows.storage.streams import DataReader, InputStreamOptions
except ImportError:
    raise SystemExit("pip install winsdk")

BAUD = 115200
SERIAL_PORT = "COM6"   # e.g. "COM6"
POLL_SECONDS = 0.4
ALBUM_SIZE = 60


# ---------------- Volume controller ----------------

class VolumeController:
    def __init__(self):
        self.endpoint = None
        self._initialized = False
        self._warned = False

    def _ensure_endpoint(self) -> bool:
        if self._initialized:
            return self.endpoint is not None

        self._initialized = True
        try:
            from ctypes import cast, POINTER
            from comtypes import CLSCTX_ALL
            from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume

            devices = AudioUtilities.GetSpeakers()
            try:
                interface = devices._dev.Activate(
                    IAudioEndpointVolume._iid_, CLSCTX_ALL, None
                )
                self.endpoint = cast(interface, POINTER(IAudioEndpointVolume))
            except Exception:
                interface = devices.Activate(
                    IAudioEndpointVolume._iid_, CLSCTX_ALL, None
                )
                self.endpoint = cast(interface, POINTER(IAudioEndpointVolume))
        except Exception as e:
            self.endpoint = None
            if not self._warned:
                self._warned = True
                print(f"[warn] volume init failed: {e}")

        return self.endpoint is not None

    def get(self) -> int:
        if not self._ensure_endpoint():
            return 50
        try:
            v = self.endpoint.GetMasterVolumeLevelScalar()
            return int(round(v * 100))
        except Exception:
            return 50

    def set(self, pct: int):
        if not self._ensure_endpoint():
            return
        try:
            self.endpoint.SetMasterVolumeLevelScalar(
                max(0, min(100, pct)) / 100.0, None)
        except Exception:
            pass

    def mute(self, state: bool):
        if not self._ensure_endpoint():
            return
        try:
            self.endpoint.SetMute(1 if state else 0, None)
        except Exception:
            pass


# ---------------- Track data ----------------

@dataclass
class TrackInfo:
    title: str = ""
    artist: str = ""
    album: str = ""
    playing: bool = False
    position_ms: int = 0
    duration_ms: int = 0
    app: str = ""
    theme_rgb: Tuple[int, int, int] = (0, 200, 255)
    art_bytes: bytes = b""
    art_w: int = 0
    art_h: int = 0


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def esc(text: str) -> str:
    return (text or "").replace("|", "/").replace("\n", " ").replace("\r", " ").strip()


def find_port() -> Optional[str]:
    if SERIAL_PORT:
        return SERIAL_PORT
    ports = list(list_ports.comports())
    preferred = []
    for p in ports:
        desc = f"{p.device} {p.description}".lower()
        score = 0
        for key in ("esp32", "usb jtag", "cp210", "ch340", "silabs", "cdc"):
            if key in desc:
                score += 1
        if score:
            preferred.append((score, p.device))
    if preferred:
        preferred.sort(reverse=True)
        return preferred[0][1]
    return ports[0].device if ports else None


# ---------------- Windows media reading ----------------

async def run_async_with_timeout(coro, timeout=1.5, default=None):
    try:
        return await asyncio.wait_for(coro, timeout=timeout)
    except Exception as e:
        print(f"[warn] winsdk operation timed out or failed: {e}")
        return default


async def get_session(manager):
    if not manager:
        return None
    try:
        current = manager.get_current_session()
    except Exception:
        current = None

    def is_playing(session) -> bool:
        try:
            playback = session.get_playback_info()
            status = str(getattr(playback, "playback_status", ""))
            return "PLAYING" in status.upper() or "4" in status
        except Exception:
            return False

    if current is not None and is_playing(current):
        return current

    try:
        sessions = list(manager.get_sessions())
    except Exception:
        sessions = []

    for session in sessions:
        if is_playing(session):
            return session

    if current is not None:
        return current
    return sessions[0] if sessions else None


async def read_thumbnail_bytes(media) -> bytes:
    # Not directly used since we read inline with timeout, but kept for compatibility.
    thumb = getattr(media, "thumbnail", None)
    if thumb is None:
        return b""
    try:
        stream = await run_async_with_timeout(thumb.open_read_async(), timeout=2.0)
        if not stream or stream.size <= 0:
            return b""
        reader = DataReader(stream.get_input_stream_at(0))
        reader.input_stream_options = InputStreamOptions.READ_AHEAD
        loaded = await run_async_with_timeout(reader.load_async(stream.size), timeout=2.0)
        if not loaded:
            return b""
        data = bytearray(reader.unconsumed_buffer_length)
        reader.read_bytes(data)
        return bytes(data)
    except Exception:
        return b""


def extract_theme_color(img: Image.Image) -> Tuple[int, int, int]:
    """Pick a vibrant dominant color suitable for UI accent."""
    small = img.convert("RGB").resize((32, 32), Image.Resampling.LANCZOS)
    pixels = list(small.getdata())

    # Score pixels: prefer saturated, mid-bright colors
    best_score = -1
    best_color = (0, 200, 255)

    # Bucket pixels by quantization to find dominant clusters
    buckets = {}
    for r, g, b in pixels:
        key = (r // 32, g // 32, b // 32)
        if key not in buckets:
            buckets[key] = [0, 0, 0, 0]  # r,g,b,count
        buckets[key][0] += r
        buckets[key][1] += g
        buckets[key][2] += b
        buckets[key][3] += 1

    for key, (rs, gs, bs, cnt) in buckets.items():
        r = rs // cnt
        g = gs // cnt
        b = bs // cnt
        mx = max(r, g, b)
        mn = min(r, g, b)
        sat = (mx - mn) / 255.0
        brightness = mx / 255.0

        # Skip too dark or too white
        if brightness < 0.25 or brightness > 0.95:
            continue
        if sat < 0.25:
            continue

        # Score: saturation * count * brightness factor
        score = sat * cnt * (1.0 - abs(brightness - 0.6))
        if score > best_score:
            best_score = score
            best_color = (r, g, b)

    # Boost saturation a bit for nicer UI
    r, g, b = best_color
    # Ensure not too dark
    mx = max(r, g, b)
    if mx < 120 and mx > 0:
        scale = 160 / mx
        r = min(255, int(r * scale))
        g = min(255, int(g * scale))
        b = min(255, int(b * scale))
    return (r, g, b)


def image_to_rgb565_bytes(img: Image.Image, size: int = ALBUM_SIZE) -> bytes:
    img = img.convert("RGB").resize((size, size), Image.Resampling.LANCZOS)
    out = bytearray(size * size * 2)
    i = 0
    for y in range(size):
        for x in range(size):
            r, g, b = img.getpixel((x, y))
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            out[i] = (v >> 8) & 0xFF
            out[i + 1] = v & 0xFF
            i += 2
    return bytes(out)


def short_app_name(app_id: str) -> str:
    if not app_id:
        return ""
    a = app_id.lower()
    if "spotify" in a:
        return "Spotify"
    if "chrome" in a:
        return "Chrome"
    if "firefox" in a:
        return "Firefox"
    if "edge" in a:
        return "Edge"
    if "vlc" in a:
        return "VLC"
    if "youtube" in a:
        return "YouTube"
    if "music" in a:
        return "Music"
    if "media" in a:
        return "Media"
    # Strip trailing parts
    base = app_id.split("!")[0].split(".")[-1]
    return base[:12] if base else "Player"


async def read_media(manager, include_art: bool = True) -> TrackInfo:
    info = TrackInfo()
    session = await get_session(manager)

    if session is None:
        info.title = "No active session"
        info.artist = "Open music on Windows"
        info.app = ""
        return info

    try:
        media = await run_async_with_timeout(session.try_get_media_properties_async(), timeout=1.5)
        timeline = session.get_timeline_properties()
        playback = session.get_playback_info()

        if media:
            info.title = str(getattr(media, "title", "") or "")
            info.artist = str(getattr(media, "artist", "") or "")
            info.album = str(getattr(media, "album_title", "") or "")
        info.app = short_app_name(
            str(getattr(session, "source_app_user_model_id", "") or ""))

        try:
            status = str(getattr(playback, "playback_status", ""))
            info.playing = "PLAYING" in status.upper() or "4" in status
        except Exception:
            info.playing = False

        try:
            import datetime
            now_utc = datetime.datetime.now(datetime.timezone.utc)
            elapsed = (now_utc - timeline.last_updated_time).total_seconds()
            pos_s = timeline.position.total_seconds()
            if info.playing:
                pos_s += elapsed
            info.position_ms = int(pos_s * 1000)
        except Exception:
            pass
        try:
            info.duration_ms = int(timeline.end_time.total_seconds() * 1000)
        except Exception:
            pass

        if include_art and media:
            try:
                thumb = getattr(media, "thumbnail", None)
                if thumb is not None:
                    stream = await run_async_with_timeout(thumb.open_read_async(), timeout=2.0)
                    if stream and stream.size > 0:
                        reader = DataReader(stream.get_input_stream_at(0))
                        reader.input_stream_options = InputStreamOptions.READ_AHEAD
                        loaded = await run_async_with_timeout(reader.load_async(stream.size), timeout=2.0)
                        if loaded:
                            thumb_bytes = bytearray(reader.unconsumed_buffer_length)
                            reader.read_bytes(thumb_bytes)
                            if thumb_bytes:
                                img = Image.open(io.BytesIO(bytes(thumb_bytes)))
                                img.load()
                                info.theme_rgb = extract_theme_color(img)
                                info.art_bytes = image_to_rgb565_bytes(img, ALBUM_SIZE)
                                info.art_w = ALBUM_SIZE
                                info.art_h = ALBUM_SIZE
            except Exception as e:
                # Album art is optional; never let it block track metadata updates.
                print(f"[warn] Failed to load album art: {e}")
    except Exception as e:
        info.title = "Media error"
        info.artist = str(e)[:36]

    if not info.title:
        info.title = "No track playing"
    if not info.artist:
        info.artist = "Open music on Windows"

    return info


# ---------------- Commands ----------------

async def send_command(cmd: str, vol_ctrl: VolumeController, manager) -> bool:
    cmd = cmd.strip().upper()

    # Local volume commands first
    if cmd.startswith("VOL:"):
        try:
            v = int(cmd.split(":", 1)[1])
            vol_ctrl.set(v)
            return True
        except Exception:
            return False
    if cmd == "MUTE":
        vol_ctrl.mute(True)
        return True
    if cmd == "UNMUTE":
        vol_ctrl.mute(False)
        return True

    # Media commands
    try:
        session = await get_session(manager)
        if session is None:
            return False
        if cmd == "TOGGLE":
            await run_async_with_timeout(session.try_toggle_play_pause_async(), timeout=1.5)
            return True
        if cmd == "PLAY":
            await run_async_with_timeout(session.try_play_async(), timeout=1.5)
            return True
        if cmd == "PAUSE":
            await run_async_with_timeout(session.try_pause_async(), timeout=1.5)
            return True
        if cmd == "NEXT":
            await run_async_with_timeout(session.try_skip_next_async(), timeout=1.5)
            return True
        if cmd == "PREV":
            await run_async_with_timeout(session.try_skip_previous_async(), timeout=1.5)
            return True
    except Exception:
        return False
    return False



# ---------------- Serial framing ----------------

def make_track_line(info: TrackInfo, vol: int) -> str:
    r, g, b = info.theme_rgb
    return (
        f"TRACK|{esc(info.artist)}|{esc(info.title)}|{1 if info.playing else 0}|"
        f"{max(0, info.position_ms)}|{max(0, info.duration_ms)}|"
        f"{clamp(vol, 0, 100)}|{esc(info.app)}|"
        f"{clamp(r, 0, 255)}|{clamp(g, 0, 255)}|{clamp(b, 0, 255)}\n"
    )


def send_album(ser: serial.Serial, info: TrackInfo):
    if not info.art_bytes:
        print("[info] No album art available, sending empty ART packet.")
        ser.write(b"ART|0|0|0\n")
        ser.flush()
        return
    print(f"[info] Sending album art ({len(info.art_bytes)} bytes) in 64-byte chunks...")
    header = f"ART|{info.art_w}|{info.art_h}|{len(info.art_bytes)}\n".encode()
    ser.write(header)
    ser.flush()
    
    # Send art in 64-byte chunks with 3ms delay to prevent buffer overflow
    chunk_size = 64
    for i in range(0, len(info.art_bytes), chunk_size):
        chunk = info.art_bytes[i:i + chunk_size]
        ser.write(chunk)
        ser.flush()
        time.sleep(0.003)
        
    ser.write(b"\n")
    ser.flush()
    print("[info] Album art sent successfully.")


# ---------------- Main ----------------

def restart_program(ser):
    print("[info] Closing serial port and restarting script...")
    if ser:
        try:
            ser.close()
        except Exception:
            pass
    time.sleep(0.5)
    import subprocess
    script = os.path.abspath(sys.argv[0])
    args = [sys.executable, script] + sys.argv[1:]
    subprocess.Popen(args)
    os._exit(0)


async def main():
    # Set Windows timer resolution to 1ms for precise sleeps
    winmm = None
    try:
        import ctypes
        winmm = ctypes.WinDLL('winmm')
        winmm.timeBeginPeriod(1)
    except Exception:
        pass

    port = find_port()
    if not port:
        raise SystemExit("No serial port found. Plug in the ESP32-C3.")

    print(f"[info] Opening {port} @ {BAUD}")
    ser = None
    while ser is None:
        try:
            # timeout=0 makes reads non-blocking
            ser = serial.Serial(port, BAUD, timeout=0)
        except SerialException as e:
            print(f"[warn] Could not open {port}: {e}")
            print("[info] Close Arduino Serial Monitor or any app using the port; retrying in 2s...")
            await asyncio.sleep(2)
    time.sleep(2.5)  # let ESP32 boot and enter loop()
    ser.write(b"\n\n\n")
    ser.flush()
    await asyncio.sleep(0.1)
    startup_time = time.time()

    # Instantiate the MediaManager once
    manager = None
    while manager is None:
        try:
            manager = await run_async_with_timeout(MediaManager.request_async(), timeout=3.0)
            if manager is None:
                print("[warn] MediaManager creation timed out. Retrying in 2s...")
                await asyncio.sleep(2)
        except Exception as e:
            print(f"[warn] Could not create MediaManager: {e}. Retrying in 2s...")
            await asyncio.sleep(2)

    vol_ctrl = VolumeController()

    last_track_key = ""
    last_art_key = ""
    last_send = 0.0
    song_end_time = None       # wall-clock time when current song is expected to finish
    song_end_track = ""        # track identity that the timer is set for

    async def poll_media_loop():
        nonlocal last_track_key, last_art_key, last_send
        nonlocal song_end_time, song_end_track
        
        current_active_track = ""
        active_theme_rgb = (0, 200, 255)
        active_art_bytes = b""
        
        while True:
            try:
                # 1. Fetch metadata without art
                info = await read_media(manager, include_art=False)
                
                track_id = f"{info.artist}|{info.title}"
                if track_id != current_active_track:
                    # 2. Track changed, fetch with art
                    info = await read_media(manager, include_art=True)
                    current_active_track = track_id
                    active_theme_rgb = info.theme_rgb
                    active_art_bytes = info.art_bytes
                else:
                    # 3. Carry over active track properties
                    info.theme_rgb = active_theme_rgb
                    info.art_bytes = active_art_bytes

                cur_vol = vol_ctrl.get()

                line = make_track_line(info, cur_vol)
                track_key = f"{info.artist}|{info.title}|{int(info.playing)}|{info.duration_ms}|{info.app}|{info.theme_rgb}|{cur_vol}"

                now = time.time()
                # Send when meta changes OR every ~1s for progress
                if track_key != last_track_key or now - last_send >= 1.0:
                    if track_key != last_track_key:
                        print(f"[info] Sending track: {info.title} - {info.artist} ({info.app})")
                    else:
                        pos_str = f"{info.position_ms // 60000}:{(info.position_ms // 1000) % 60:02d}"
                        dur_str = f"{info.duration_ms // 60000}:{(info.duration_ms // 1000) % 60:02d}"
                        print(f"[progress] {pos_str} / {dur_str} (playing: {info.playing})")
                    ser.write(line.encode("utf-8"))
                    ser.flush()
                    last_send = now
                    last_track_key = track_key

                # Send art only when track changes
                art_key = f"{info.artist}|{info.title}|{len(info.art_bytes)}"
                if art_key != last_art_key:
                    send_album(ser, info)
                    last_art_key = art_key

                # --- Auto-restart and track change detection ---
                cur_track_id = f"{info.artist}|{info.title}"
                is_fallback = info.title in ("No active session", "No track playing", "Media error")
                
                if info.playing and info.duration_ms > 0 and info.position_ms >= 0 and not is_fallback:
                    remaining_ms = info.duration_ms - info.position_ms
                    remaining_s = max(0.0, remaining_ms / 1000.0)
                    
                    if cur_track_id != song_end_track:
                        # Transitioned from one real track to another
                        was_fallback = (
                            "No active session" in song_end_track or 
                            "No track playing" in song_end_track or 
                            "Media error" in song_end_track or
                            song_end_track == ""
                        )
                        if not was_fallback:
                            print(f"[auto] Track changed from '{song_end_track}' to '{cur_track_id}'. Restarting...")
                            restart_program(ser)
                        
                        song_end_track = cur_track_id
                        song_end_time = now + remaining_s + 3.0  # 3s buffer
                        mins = int(remaining_s) // 60
                        secs = int(remaining_s) % 60
                        print(f"[auto] Track initialized: '{cur_track_id}'. Ends in ~{mins}:{secs:02d} + 3s buffer.")
                    else:
                        if song_end_time is None:
                            # Resumed playing after a pause
                            song_end_time = now + remaining_s + 3.0
                            mins = int(remaining_s) // 60
                            secs = int(remaining_s) % 60
                            print(f"[auto] Resumed playback. Restart scheduled in ~{mins}:{secs:02d} + 3s buffer.")
                        else:
                            # Check for significant seek/scrub drift
                            expected_remaining_s = song_end_time - now
                            actual_remaining_s = remaining_s + 3.0
                            if abs(expected_remaining_s - actual_remaining_s) > 2.0:
                                song_end_time = now + remaining_s + 3.0
                                mins = int(remaining_s) // 60
                                secs = int(remaining_s) % 60
                                print(f"[auto] Seek/scrub detected. Restart rescheduled in ~{mins}:{secs:02d} + 3s buffer.")
                else:
                    # Paused, stopped, or fallback track
                    song_end_time = None
                    if cur_track_id != song_end_track:
                        song_end_track = cur_track_id

                # Check if the expected end time has been reached
                if song_end_time is not None and now >= song_end_time:
                    print("[auto] Expected end time reached. Restarting to load next song...")
                    restart_program(ser)

            except Exception as e:
                print(f"[poll error] {e}")

            await asyncio.sleep(POLL_SECONDS)

    async def serial_read_loop():
        buf = b""
        while True:
            try:
                # non-blocking read because timeout=0
                data = ser.read(256)
            except Exception as e:
                print(f"[serial error] {e}")
                await asyncio.sleep(0.5)
                continue
            if data:
                buf += data
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    text = raw.decode("utf-8", errors="ignore").strip()
                    if not text:
                        continue
                    if text.startswith("BTN:"):
                        parts = text[4:].split(":")
                        action = parts[0]
                        if action == "VOL" and len(parts) >= 2:
                            await send_command(f"VOL:{parts[1]}", vol_ctrl, manager)
                        else:
                            if action in ("NEXT", "PREV"):
                                # Ignore startup button bounce to prevent infinite restart loop
                                time_since_startup = time.time() - startup_time
                                if time_since_startup < 3.0:
                                    print(f"[info] Ignored early button transition during boot: {text}")
                                    continue
                                await send_command(action, vol_ctrl, manager)
                                print(f"[info] {action} button pressed. Waiting 1.0s and restarting...")
                                await asyncio.sleep(1.0)
                                restart_program(ser)
                            else:
                                await send_command(action, vol_ctrl, manager)
                        print(f"[esp32] {text}")
                    else:
                        print(f"[esp32] {text}")
                await asyncio.sleep(0.01)
            else:
                # yield to event loop when no serial data is available
                await asyncio.sleep(0.05)

    print("[info] Running. Ctrl+C to exit.")
    try:
        await asyncio.gather(poll_media_loop(), serial_read_loop())
    finally:
        if winmm:
            try:
                winmm.timeEndPeriod(1)
            except Exception:
                pass
        ser.close()


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(line_buffering=True)
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[info] Stopped.")
