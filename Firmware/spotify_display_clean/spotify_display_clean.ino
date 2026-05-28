#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>
#include <math.h>

const char* SSID          = "YOUR_WIFI_SSID";
const char* PASSWORD      = "YOUR_WIFI_PASSWORD";
const char* CLIENT_ID     = "YOUR_CLIENT_ID";
const char* CLIENT_SECRET = "YOUR_CLIENT_SECRET";

#define TFT_CS   2
#define TFT_RST  1
#define TFT_DC   3
#define TFT_SCLK 4
#define TFT_MOSI 6

#define BTN_PREV 5
#define BTN_PLAY 7
#define BTN_NEXT 21

#define ENC_CLK  0
#define ENC_DT   10
#define ENC_SW   20

#define C_BG       0x0000
#define C_SURFACE  0x0841
#define C_BORDER   0x2945
#define C_GREEN    0x1DCA
#define C_GREEN_DK 0x0E44
#define C_WHITE    0xFFFF
#define C_MUTED    0x7BEF
#define C_DIM      0x39E7
#define C_RED      0xF800

#define SW        160
#define SH        128

#define HDR_H      20
#define FTR_Y     108
#define TRK_Y      34
#define ART_Y      58
#define BAR_Y      82
#define BAR_H       3
#define TIME_Y     88
#define BAR_PAD    10

#define T_API        2500
#define T_SCROLL       28
#define T_EQ           55
#define T_VOL        1800
#define SCROLL_PAUSE 1200
#define SCROLL_GAP     48
#define D_BTN          50
#define D_CLK           4
#define D_SW          300

const unsigned char icon_play[] PROGMEM = {
    0x00,0x00, 0x03,0x00, 0x07,0x00, 0x0f,0x00, 0x1f,0x00, 0x3f,0x00,
    0x7f,0x00, 0xff,0x00, 0xff,0x00, 0x7f,0x00, 0x3f,0x00, 0x1f,0x00,
    0x0f,0x00, 0x07,0x00, 0x03,0x00, 0x00,0x00
};
const unsigned char icon_pause[] PROGMEM = {
    0x00,0x00, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c,
    0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c,
    0x3c,0x3c, 0x3c,0x3c, 0x3c,0x3c, 0x00,0x00
};
const unsigned char icon_next[] PROGMEM = {
    0x00,0x00, 0x0c,0x30, 0x1c,0x30, 0x3c,0x30, 0x7c,0x30, 0xfc,0x30,
    0xfc,0x30, 0xfc,0x30, 0xfc,0x30, 0xfc,0x30, 0xfc,0x30, 0x7c,0x30,
    0x3c,0x30, 0x1c,0x30, 0x0c,0x30, 0x00,0x00
};
const unsigned char icon_prev[] PROGMEM = {
    0x00,0x00, 0x0c,0x30, 0x0c,0x38, 0x0c,0x3c, 0x0c,0x7c, 0x0c,0xfc,
    0x0c,0xfc, 0x0c,0xfc, 0x0c,0xfc, 0x0c,0xfc, 0x0c,0xfc, 0x0c,0x7c,
    0x0c,0x3c, 0x0c,0x38, 0x0c,0x30, 0x00,0x00
};
const unsigned char icon_shuffle[] PROGMEM = {
    0x00,0x00, 0x81,0x81, 0x42,0x42, 0x24,0x24, 0x18,0x18, 0x18,0x18,
    0x24,0x24, 0x42,0x42, 0x42,0x42, 0x24,0x24, 0x18,0x18, 0x18,0x18,
    0x24,0x24, 0x42,0x42, 0x81,0x81, 0x00,0x00
};
const unsigned char icon_speaker[] PROGMEM = {
    0x01,0x80, 0x03,0x80, 0x07,0x00, 0x3f,0x00, 0x3f,0x44, 0x3f,0x88,
    0x3f,0x10, 0x3f,0x10, 0x3f,0x10, 0x3f,0x88, 0x3f,0x44, 0x07,0x00,
    0x03,0x80, 0x01,0x80, 0x00,0x00, 0x00,0x00
};

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
Spotify         sp(CLIENT_ID, CLIENT_SECRET);

GFXcanvas16 trackCanvas(SW, 20);

struct TrackState {
    String name, artist;
    bool   playing  = false;
    bool   shuffle  = false;
    int    progress = 0;
    int    duration = 0;
};
TrackState cur;

int  vol              = 50;
bool volVisible       = false;
bool pendingVolUpdate = false;

float        scrollX    = 0.0f;
int          textW      = 0;
bool         scrolling  = false;
bool         scrollHeld = true;
unsigned long tScrollHeld = 0;

#define EQ_BARS   7
#define EQ_BAR_W  5
#define EQ_GAP    2
#define EQ_MAX_H  13
#define EQ_BASE_Y (HDR_H - 4)

float        eqPos[EQ_BARS];
float        eqVel[EQ_BARS];
float        eqTgt[EQ_BARS];
unsigned long eqNextTgt[EQ_BARS];

void initEQ() {
    for (int i = 0; i < EQ_BARS; i++) {
        eqPos[i] = 2.0f; eqVel[i] = 0.0f;
        eqTgt[i] = 2.0f; eqNextTgt[i] = 0;
    }
}

unsigned long tApi = 0, tScroll = 0, tEq = 0, tVol = 0, tWifiCheck = 0;
unsigned long tBtn[3] = {0, 0, 0};
unsigned long tClk = 0, tSw = 0, tVolTurn = 0;
bool          lastBtn[3]  = {HIGH, HIGH, HIGH};
int           lastClk     = HIGH;
bool          lastSw      = HIGH;

bool valid(const String& s) {
    return s.length() > 0 && s != "null" &&
           s != "error" && s != "Something went wrong";
}

String fmtTime(int ms) {
    if (ms <= 0) return "0:00";
    int s = ms / 1000;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return String(buf);
}

uint16_t wifiColor() {
    return (WiFi.status() == WL_CONNECTED) ? C_GREEN : C_RED;
}

void wifiKeepAlive() {
    if (WiFi.status() == WL_CONNECTED) return;
    unsigned long t = millis();
    if (t - tWifiCheck >= 10000) {
        tWifiCheck = t;
        WiFi.disconnect();
        WiFi.begin(SSID, PASSWORD);
    }
}

void animBoot() {
    tft.fillScreen(C_BG);


    for (int x = 0; x < SW; x++) {
        int h = 3 + (int)(sin(x * 0.18f) * 11.0f);
        tft.drawFastVLine(x, SH / 2 - h / 2, h, C_GREEN);
        if (x > 6) tft.drawFastVLine(x - 6, SH / 2 - 14, 28, C_BG);
        delay(3);
    }
    tft.fillScreen(C_BG);


    const char* logo  = "SPOTIFY";
    const int   nChar = 7;
    int logoW  = nChar * 12;
    int logoX  = (SW - logoW) / 2;
    int logoY  = SH / 2 - 14;

    tft.setTextSize(2);

    for (int i = 0; i < nChar; i++) {

        for (int dy = -14; dy <= 0; dy += 2) {
            tft.fillRect(logoX + i * 12, logoY - 2, 12, 18, C_BG);
            tft.setCursor(logoX + i * 12, logoY + dy);
            tft.setTextColor(C_GREEN);
            tft.print(logo[i]);
            delay(4);
        }

        tft.fillRect(logoX + i * 12, logoY - 2, 12, 18, C_BG);
        tft.setCursor(logoX + i * 12, logoY);
        tft.setTextColor(C_WHITE);
        tft.print(logo[i]);
        delay(30);
    }


    for (int x = logoX; x < logoX + logoW; x++) {
        tft.drawPixel(x, logoY + 17, C_GREEN);
        delay(2);
    }


    delay(200);
    tft.setTextSize(1);
    tft.setTextColor(C_MUTED);
    int tagW = 9 * 6;
    tft.setCursor((SW - tagW) / 2, logoY + 22);
    tft.print("BEAT VIEW");

    delay(700);


    for (int h = 1; h <= SH / 2 + 4; h += 4) {
        tft.fillRect(0, SH / 2 - h, SW, h * 2, C_BG);
        delay(6);
    }
}

void statusScreen(const char* line1, const char* line2 = nullptr) {
    tft.fillScreen(C_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_MUTED);
    int y = SH / 2 - (line2 ? 8 : 4);
    tft.setCursor((SW - strlen(line1) * 6) / 2, y);
    tft.print(line1);
    if (line2) {
        tft.setTextColor(C_DIM);
        tft.setCursor((SW - strlen(line2) * 6) / 2, y + 12);
        tft.print(line2);
    }
}

void updateEQ(bool playing) {

    const int totalW = EQ_BARS * (EQ_BAR_W + EQ_GAP) - EQ_GAP;
    const int startX = SW - totalW - 14;
    unsigned long now = millis();

    for (int i = 0; i < EQ_BARS; i++) {

        tft.fillRect(startX + i * (EQ_BAR_W + EQ_GAP),
                     EQ_BASE_Y - EQ_MAX_H, EQ_BAR_W, EQ_MAX_H, C_BG);

        if (playing) {

            if (now >= eqNextTgt[i]) {
                eqTgt[i]     = random(2, EQ_MAX_H + 1);
                eqNextTgt[i] = now + random(70, 260);
            }


            float disp  = eqTgt[i] - eqPos[i];
            eqVel[i]   += disp * 0.32f;
            eqVel[i]   *= 0.68f;
            eqPos[i]   += eqVel[i];
            eqPos[i]    = constrain(eqPos[i], 1.0f, (float)EQ_MAX_H);

            int bh   = max(1, (int)eqPos[i]);
            int bx   = startX + i * (EQ_BAR_W + EQ_GAP);
            int topY = EQ_BASE_Y - bh;


            int split = bh / 2;
            tft.fillRect(bx, topY,          EQ_BAR_W, split,      C_GREEN_DK);
            tft.fillRect(bx, topY + split,  EQ_BAR_W, bh - split, C_GREEN);


            tft.drawFastHLine(bx, topY, EQ_BAR_W, C_WHITE);

        } else {

            if (eqPos[i] > 2.0f) {
                eqVel[i] -= 0.25f;
                eqPos[i] += eqVel[i];
                if (eqPos[i] <= 2.0f) { eqPos[i] = 2.0f; eqVel[i] = 0.0f; }
            }
            int bh = max(2, (int)eqPos[i]);
            tft.fillRect(startX + i * (EQ_BAR_W + EQ_GAP),
                         EQ_BASE_Y - bh, EQ_BAR_W, bh, C_DIM);
        }
    }
}

void drawHeader() {
    tft.fillRect(0, 0, SW, HDR_H, C_BG);

    tft.setTextSize(1);
    tft.setTextColor(C_GREEN);
    tft.setCursor(6, 6);
    tft.print("BEAT VIEW");


    tft.fillCircle(SW - 7, HDR_H / 2, 3, wifiColor());


    tft.drawFastHLine(0, HDR_H - 2, SW, C_BORDER);
    tft.drawFastHLine(0, HDR_H - 1, SW, C_SURFACE);
}

void drawFooter(bool playing, bool shuffle) {
    tft.fillRect(0, FTR_Y, SW, SH - FTR_Y, C_BG);


    tft.drawFastHLine(0, FTR_Y,     SW, C_SURFACE);
    tft.drawFastHLine(0, FTR_Y + 1, SW, C_BORDER);

    int cy = FTR_Y + (SH - FTR_Y - 16) / 2;


    uint16_t shufCol = shuffle ? C_GREEN : C_DIM;
    tft.drawBitmap(4, cy, icon_shuffle, 16, 16, shufCol);
    if (shuffle) tft.fillRect(8, cy + 14, 8, 2, C_GREEN);


    tft.drawBitmap(26,  cy, icon_prev,                       16, 16, C_WHITE);
    tft.drawBitmap(72,  cy, playing ? icon_pause : icon_play, 16, 16, C_GREEN);
    tft.drawBitmap(118, cy, icon_next,                       16, 16, C_WHITE);
}

void drawProgressBar(int progress, int duration) {
    int pct = (duration > 0)
              ? (int)constrain(map(progress, 0, duration, 0, 100), 0, 100)
              : 0;

    int bx = BAR_PAD;
    int bw = SW - BAR_PAD * 2;


    tft.fillRect(bx, BAR_Y, bw, BAR_H, C_SURFACE);


    int filled = map(pct, 0, 100, 0, bw);
    if (filled > 0) tft.fillRect(bx, BAR_Y, filled, BAR_H, C_GREEN);


    int dotX = bx + filled;
    tft.fillCircle(dotX, BAR_Y + BAR_H / 2, 4, C_WHITE);
    tft.fillCircle(dotX, BAR_Y + BAR_H / 2, 2, C_GREEN);


    tft.fillRect(0,        TIME_Y - 1, 38, 9, C_BG);
    tft.fillRect(SW - 38,  TIME_Y - 1, 38, 9, C_BG);

    tft.setTextSize(1);
    tft.setTextColor(C_DIM);
    tft.setCursor(BAR_PAD, TIME_Y);
    tft.print(fmtTime(progress));

    String total = fmtTime(duration);
    tft.setCursor(SW - BAR_PAD - (int)total.length() * 6, TIME_Y);
    tft.print(total);
}

void showVolPopup(int v) {
    const int pw = SW - 24, ph = 24;
    const int px = 12,      py = HDR_H + 6;

    tft.fillRect(px, py, pw, ph, C_SURFACE);
    tft.drawRect(px,     py,     pw,     ph,     C_BORDER);
    tft.drawRect(px + 1, py + 1, pw - 2, ph - 2, C_GREEN);

    tft.drawBitmap(px + 4, py + 4, icon_speaker, 16, 16, C_WHITE);


    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    char buf[5];
    snprintf(buf, sizeof(buf), "%3d%%", v);
    tft.setCursor(px + pw - 26, py + 8);
    tft.print(buf);


    const int bx = px + 24, bw = pw - 52;
    tft.fillRect(bx, py + 11, bw, 3, C_DIM);
    tft.fillRect(bx, py + 11, map(v, 0, 100, 0, bw), 3, C_GREEN);

    volVisible = true;
    tVol = millis();
}

void clearVolPopup() {
    const int pw = SW - 24, ph = 24;
    const int px = 12,      py = HDR_H + 6;

    tft.fillRect(px - 2, py - 2, pw + 4, ph + 4, C_BG);


    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(1);
    tft.getTextBounds(cur.artist.c_str(), 0, 0, &x1, &y1, &w, &h);
    tft.setTextColor(C_MUTED);
    tft.setCursor((SW - w) / 2, ART_Y);
    tft.print(cur.artist);

    drawProgressBar(cur.progress, cur.duration);
    volVisible = false;
}

void renderTrack(const String& artist, const String& track) {
    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(2);
    tft.getTextBounds(track.c_str(), 0, 0, &x1, &y1, &w, &h);
    textW = (int)w;

    if (textW > SW - 4) {

        scrolling  = true;
        scrollX    = 0.0f;
        scrollHeld = true;
        tScrollHeld = millis();
    } else {
        scrolling = false;
        tft.fillRect(0, TRK_Y - 2, SW, 20, C_BG);
        tft.setCursor((SW - textW) / 2, TRK_Y);
        tft.setTextColor(C_WHITE);
        tft.print(track);
    }

    tft.setTextSize(1);
    tft.getTextBounds(artist.c_str(), 0, 0, &x1, &y1, &w, &h);
    tft.fillRect(0, ART_Y - 2, SW, 11, C_BG);
    tft.setCursor((SW - w) / 2, ART_Y);
    tft.setTextColor(C_MUTED);
    tft.print(artist);

    drawProgressBar(cur.progress, cur.duration);
}

void animTrackTransition(const String& incoming) {
    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(2);
    tft.getTextBounds(incoming.c_str(), 0, 0, &x1, &y1, &w, &h);
    int inW = (int)w;


    for (int r = 1; r <= 12; r++) {
        tft.fillRect(0, TRK_Y - 2 + (10 - r), SW, r * 2, C_SURFACE);
        delay(4);
    }
    tft.fillRect(0, TRK_Y - 2, SW, 20, C_SURFACE);


    tft.setCursor((SW - inW) / 2, TRK_Y);
    tft.setTextColor(C_GREEN);
    tft.print(incoming);
    delay(110);


    tft.fillRect(0, TRK_Y - 2, SW, 20, C_BG);
    tft.setCursor((SW - inW) / 2, TRK_Y);
    tft.setTextColor(C_WHITE);
    tft.print(incoming);
}

void handleButtons() {
    unsigned long t = millis();

    struct { int pin; int idx; } btns[] = {
        {BTN_PLAY, 0}, {BTN_NEXT, 1}, {BTN_PREV, 2}
    };

    for (auto& b : btns) {
        bool state = digitalRead(b.pin);
        if (state == LOW && lastBtn[b.idx] == HIGH && t - tBtn[b.idx] > D_BTN) {
            tBtn[b.idx] = t;
            switch (b.idx) {
                case 0:
                    cur.playing ? sp.pause_playback() : sp.start_resume_playback();
                    cur.playing = !cur.playing;
                    drawFooter(cur.playing, cur.shuffle);
                    break;
                case 1:
                    sp.skip();    tApi = 0;  break;
                case 2:
                    sp.previous(); tApi = 0; break;
            }
        }
        lastBtn[b.idx] = state;
    }
}

void handleEncoder() {
    unsigned long t = millis();

    int clk = digitalRead(ENC_CLK);
    if (clk != lastClk && clk == LOW && t - tClk > D_CLK) {
        tClk = t;
        int delta = (digitalRead(ENC_DT) != clk) ? -4 : 4;
        vol = constrain(vol + delta, 0, 100);
        showVolPopup(vol);
        pendingVolUpdate = true;
        tVolTurn = t;
    }
    lastClk = clk;


    if (pendingVolUpdate && (t - tVolTurn > 300)) {
        sp.set_volume(vol);
        pendingVolUpdate = false;
    }

    bool sw = digitalRead(ENC_SW);
    if (lastSw == HIGH && sw == LOW && t - tSw > D_SW) {
        tSw = t;
        cur.shuffle = !cur.shuffle;
        sp.set_shuffle(cur.shuffle);
        drawFooter(cur.playing, cur.shuffle);
    }
    lastSw = sw;
}

void setup() {
    Serial.begin(115200);

    pinMode(BTN_PREV, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    pinMode(ENC_CLK,  INPUT_PULLUP);
    pinMode(ENC_DT,   INPUT_PULLUP);
    pinMode(ENC_SW,   INPUT_PULLUP);

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(C_BG);

    initEQ();
    animBoot();


    statusScreen("CONNECTING...", SSID);
    WiFi.begin(SSID, PASSWORD);

    int dots = 0;
    unsigned long tDot = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(150);
        if (millis() - tDot > 450) {
            tDot = millis();
            tft.setTextColor(C_GREEN);
            tft.setCursor((SW - 4 * 6) / 2, SH / 2 + 8);
            char buf[5];
            snprintf(buf, sizeof(buf), "%-4s", (dots % 4 == 0 ? "." : dots % 4 == 1 ? ".." : dots % 4 == 2 ? "..." : "...."));
            tft.print(buf);
            dots++;
        }
    }


    tft.fillScreen(C_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_GREEN);
    tft.setCursor((SW - 10 * 6) / 2, SH / 2 - 4);
    tft.print("CONNECTED!");
    delay(450);


    statusScreen("SPOTIFY AUTH...", "open browser to authorise");
    sp.begin();
    while (!sp.is_auth()) sp.handle_client();


    tft.fillScreen(C_BG);
    drawHeader();
    drawFooter(false, false);
}

void loop() {
    unsigned long t = millis();

    sp.handle_client();
    wifiKeepAlive();
    handleButtons();
    handleEncoder();


    static bool lastWifi = false;
    bool nowWifi = (WiFi.status() == WL_CONNECTED);
    if (nowWifi != lastWifi) {
        lastWifi = nowWifi;
        tft.fillCircle(SW - 7, HDR_H / 2, 3, wifiColor());
    }


    if (volVisible && t - tVol >= T_VOL) clearVolPopup();


    if (t - tEq >= T_EQ) { tEq = t; updateEQ(cur.playing); }


    if (scrolling && !volVisible) {
        if (scrollHeld) {
            if (t - tScrollHeld >= SCROLL_PAUSE) scrollHeld = false;
        } else if (t - tScroll >= T_SCROLL) {
            tScroll = t;

            trackCanvas.fillScreen(C_BG);
            trackCanvas.setTextSize(2);
            trackCanvas.setTextColor(C_WHITE);

            int sx = (int)scrollX;
            trackCanvas.setCursor(sx, 2);
            trackCanvas.print(cur.name);


            if (sx < 0) {
                trackCanvas.setCursor(sx + textW + SCROLL_GAP, 2);
                trackCanvas.print(cur.name);
            }

            tft.drawRGBBitmap(0, TRK_Y - 2, trackCanvas.getBuffer(), SW, 20);

            scrollX -= 1.0f;
            if (scrollX <= -(float)(textW + SCROLL_GAP)) scrollX = 0.0f;
        }
    }


    if (t - tApi >= T_API) {
        tApi = t;

        String newTrack  = sp.current_track_name();
        String newArtist = sp.current_artist_names();
        bool   newPlay   = sp.is_playing();
        int    newProg   = sp.current_playback_progress();
        int    newDur    = sp.current_track_duration();

        bool trackChanged = valid(newTrack) && (newTrack != cur.name);

        if (trackChanged) {
            animTrackTransition(newTrack);
            cur.name     = newTrack;
            cur.artist   = newArtist;
            cur.duration = newDur;
            cur.progress = newProg;
            renderTrack(cur.artist, cur.name);
        }

        if (newDur > 0 && !volVisible) {
            cur.progress = newProg;
            cur.duration = newDur;
            drawProgressBar(cur.progress, cur.duration);
        }

        if (newPlay != cur.playing || trackChanged) {
            cur.playing = newPlay;
            drawFooter(cur.playing, cur.shuffle);
        }
    }
}
