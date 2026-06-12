#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>

// =====================================================
// ESP32-C3 SuperMini + ST7735 Media Controller
// Layout: 160x128 landscape
// =====================================================

// ------------------ TFT pins ------------------
#define TFT_CS    2
#define TFT_RST   1
#define TFT_DC    3
#define TFT_SCLK  4
#define TFT_MOSI  6

// ------------------ Controls ------------------
#define BTN_PREV  5
#define BTN_PLAY  7
#define BTN_NEXT  21
#define ENC_CLK   0
#define ENC_DT    10
#define ENC_SW    20

// ------------------ Display geometry ------------------
#define SW          160
#define SH          128

// Layout regions
#define HDR_H       12          // app name strip
#define FTR_H       20          // playback controls
#define BODY_Y      HDR_H
#define FTR_Y       (SH - FTR_H)
#define BODY_H      (FTR_Y - BODY_Y)   // 96px
// Title sub-canvas for proper clipping
#define TITLE_CV_W  (SW - TXT_X - 4)   // 84px wide
#define TITLE_CV_H  16

// Album art
#define ART_SIZE    60
#define ART_X       6
#define ART_Y       (BODY_Y + 4)

// Text area (right of album)
#define TXT_X       72
#define TXT_W       (SW - TXT_X - 4)
#define TITLE_Y     (BODY_Y + 6)
#define TITLE_H     14
#define ARTIST_Y    (BODY_Y + 24)
#define ARTIST_H    10
#define EQ_Y        (BODY_Y + 38)
#define EQ_H        18

// Progress bar (full width below album)
#define PROG_Y      (BODY_Y + 76)
#define PROG_TIME_Y (BODY_Y + 84)

// ------------------ Timing ------------------
#define D_BTN       45
#define D_CLK       3
#define D_SW        300
#define T_UI        40
#define T_VOL       1800
#define T_SCROLL    50
#define SCROLL_GAP  30
#define SCROLL_PAUSE 1000

// ------------------ Colors (RGB565) ------------------
#define C_BG        0x0000
#define C_PANEL     0x18E3
#define C_PANEL_2   0x2965
#define C_TEXT      0xFFFF
#define C_MUTED     0xACD3
#define C_DIM       0x6B4D
#define C_ACCENT    0x07FF
#define C_DANGER    0xF800

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 bodyCanvas(SW, BODY_H);
GFXcanvas16 footerCanvas(SW, FTR_H);
GFXcanvas16 headerCanvas(SW, HDR_H);
GFXcanvas16 titleCanvas(TITLE_CV_W, TITLE_CV_H);

// ------------------ State ------------------
struct Track {
  String artist = "";
  String name = "";
  String app = "";
  bool playing = false;
  int progress = 0;
  int duration = 0;
};

Track cur;
int vol = 50;
bool shuf = false;
bool muted = false;

// Inputs
int lastClk = HIGH;
bool lastSw = HIGH;
bool lastBtnState[3] = {HIGH, HIGH, HIGH};
unsigned long tBtn[3] = {0, 0, 0};
unsigned long tClk = 0;
unsigned long tSw = 0;
unsigned long tUi = 0;
unsigned long tScroll = 0;
unsigned long tVol = 0;
unsigned long tScrollPause = 0;

bool volVisible = false;
bool scrolling = false;
int scrollX = 0;
int textW = 0;
uint32_t frameId = 0;

// EQ bars (8 bars)
#define EQ_BARS 8
float eqH[EQ_BARS]      = {3, 6, 9, 4, 7, 5, 8, 3};
float targetEq[EQ_BARS] = {3, 6, 9, 4, 7, 5, 8, 3};

String rxLine;

// Album art
uint16_t albumArt[ART_SIZE * ART_SIZE];
bool artReady = false;
bool artReceiving = false;
bool artAwaitTerm = false;
int artExpected = 0;
int artIndex = 0;
int artW = ART_SIZE;
int artH = ART_SIZE;

// Theme color from PC (extracted from album art)
uint16_t themeColor = C_ACCENT;
uint8_t themeR = 0, themeG = 200, themeB = 255;

// Visual pulse for play indicator
float pulsePhase = 0;

// ------------------ Helpers ------------------
static bool valid(const String& s) {
  return s.length() > 0 && s != "null" && s != "error";
}

static String safeText(String s, int maxLen = 60) {
  s.replace("\r", " ");
  s.replace("\n", " ");
  s.replace("|", "/");
  s.trim();
  if ((int)s.length() > maxLen) s = s.substring(0, maxLen);
  return s;
}

static String formatTime(int ms) {
  if (ms < 0) ms = 0;
  int total = ms / 1000;
  int m = total / 60;
  int s = total % 60;
  char buf[10];
  snprintf(buf, sizeof(buf), "%d:%02d", m, s);
  return String(buf);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint8_t rr = ar + (((int)br - ar) * t >> 8);
  uint8_t gg = ag + (((int)bg - ag) * t >> 8);
  uint8_t bb2 = ab + (((int)bb - ab) * t >> 8);
  return (rr << 11) | (gg << 5) | bb2;
}

static uint16_t darken(uint16_t c, uint8_t amt) {
  return blend565(c, C_BG, amt);
}

static uint16_t lighten(uint16_t c, uint8_t amt) {
  return blend565(c, C_TEXT, amt);
}

static String initialsFromTitle(const String& s) {
  String out;
  bool take = true;
  for (unsigned i = 0; i < s.length() && out.length() < 2; i++) {
    char c = s[i];
    if (isalnum((unsigned char)c)) {
      if (take) { out += (char)toupper(c); take = false; }
    } else take = true;
  }
  if (out.length() == 0) out = "?";
  return out;
}

// ------------------ Drawing primitives ------------------

// Vertical gradient fill on canvas
static void fillGradV(GFXcanvas16& c, int x, int y, int w, int h, uint16_t top, uint16_t bot) {
  for (int i = 0; i < h; i++) {
    uint8_t t = (h > 1) ? (uint8_t)((i * 255L) / (h - 1)) : 0;
    c.drawFastHLine(x, y + i, w, blend565(top, bot, t));
  }
}

// Draw album art with rounded corners and subtle border
static void drawAlbumArt(GFXcanvas16& c) {
  int ay = ART_Y - BODY_Y;  // translate to canvas-local Y
  
  if (artReady && artW == ART_SIZE && artH == ART_SIZE) {
    c.drawRGBBitmap(ART_X, ay, albumArt, ART_SIZE, ART_SIZE);
  } else {
    // Fallback: gradient square with initials
    uint16_t topC = lighten(themeColor, 40);
    uint16_t botC = darken(themeColor, 60);
    fillGradV(c, ART_X, ay, ART_SIZE, ART_SIZE, topC, botC);
    
    // Decorative circle
    c.drawCircle(ART_X + ART_SIZE/2, ay + ART_SIZE/2, 18, lighten(themeColor, 80));
    c.drawCircle(ART_X + ART_SIZE/2, ay + ART_SIZE/2, 14, lighten(themeColor, 120));
    
    String init = initialsFromTitle(valid(cur.name) ? cur.name : "?");
    c.setTextSize(3);
    c.setTextColor(C_TEXT);
    int16_t x1, y1; uint16_t tw, th;
    c.getTextBounds(init, 0, 0, &x1, &y1, &tw, &th);
    c.setCursor(ART_X + (ART_SIZE - tw)/2, ay + (ART_SIZE - th)/2);
    c.print(init);
  }
  
  // Border
  c.drawRect(ART_X - 1, ay - 1, ART_SIZE + 2, ART_SIZE + 2, darken(themeColor, 100));
  c.drawRect(ART_X, ay, ART_SIZE, ART_SIZE, themeColor);
}

// Animated EQ bars
static void drawEqBars(GFXcanvas16& c) {
  int ey = EQ_Y - BODY_Y;
  int barW = 3;
  int gap = 2;
  int totalW = EQ_BARS * (barW + gap) - gap;
  int startX = TXT_X;
  
  for (int i = 0; i < EQ_BARS; i++) {
    int bx = startX + i * (barW + gap);
    
    // Background slot
    c.fillRect(bx, ey, barW, EQ_H, C_PANEL);
    
    if (cur.playing) {
      // Animate toward target
      if (abs(eqH[i] - targetEq[i]) < 0.5f) {
        targetEq[i] = random(2, EQ_H + 1);
      }
      eqH[i] += (targetEq[i] - eqH[i]) * 0.35f;
      
      int h = (int)eqH[i];
      if (h < 2) h = 2;
      
      // Gradient bar
      for (int j = 0; j < h; j++) {
        uint8_t t = (uint8_t)((j * 255L) / EQ_H);
        uint16_t col = blend565(themeColor, lighten(themeColor, 120), t);
        c.drawFastHLine(bx, ey + EQ_H - h + j, barW, col);
      }
    } else {
      // Paused: flat low bar
      eqH[i] = 2;
      c.fillRect(bx, ey + EQ_H - 2, barW, 2, C_DIM);
    }
  }
}

// Progress bar
static void drawProgress(GFXcanvas16& c) {
  int py = PROG_Y - BODY_Y;
  int pad = 6;
  int barW = SW - pad * 2;
  int barH = 4;
  
  // Background track
  c.fillRoundRect(pad, py, barW, barH, 2, C_PANEL);
  
  int pct = (cur.duration > 0) ? 
            map(constrain(cur.progress, 0, cur.duration), 0, cur.duration, 0, barW) : 0;
  
  if (pct > 0) {
    // Gradient fill
    for (int i = 0; i < pct; i++) {
      uint8_t t = (uint8_t)((i * 255L) / barW);
      uint16_t col = blend565(darken(themeColor, 40), lighten(themeColor, 60), t);
      c.drawFastVLine(pad + i, py, barH, col);
    }
    // Playhead dot
    int dotX = pad + pct;
    if (dotX < pad + barW) {
      c.fillCircle(dotX, py + barH/2, 3, C_TEXT);
      c.drawCircle(dotX, py + barH/2, 3, themeColor);
    }
  }
  
  // Time labels
  int ty = PROG_TIME_Y - BODY_Y;
  c.setTextSize(1);
  c.setTextColor(C_MUTED);
  c.setCursor(pad, ty);
  c.print(formatTime(cur.progress));
  
  String rt = formatTime(cur.duration);
  int16_t x1, y1; uint16_t tw, th;
  c.getTextBounds(rt, 0, 0, &x1, &y1, &tw, &th);
  c.setCursor(SW - pad - tw, ty);
  c.print(rt);
}

// Scrolling title text
static void drawTitle(GFXcanvas16& c) {
  String title = valid(cur.name) ? cur.name : "No track playing";
  title = safeText(title, 80);
  
  // Render onto title sub-canvas (hard-clipped region)
  titleCanvas.fillScreen(C_BG);
  // Match body gradient at title row so it blends
  uint16_t bgTop = darken(themeColor, 220);
  uint16_t bgBot = C_BG;
  int rowY = TITLE_Y - BODY_Y;
  for (int i = 0; i < TITLE_CV_H; i++) {
    int absY = rowY + i;
    uint8_t t = (BODY_H > 1) ? (uint8_t)((absY * 255L) / (BODY_H - 1)) : 0;
    titleCanvas.drawFastHLine(0, i, TITLE_CV_W, blend565(bgTop, bgBot, t));
  }
  
  titleCanvas.setTextSize(2);
  titleCanvas.setTextColor(C_TEXT);
  titleCanvas.setTextWrap(false);
  
  int16_t x1, y1; uint16_t tw, th;
  titleCanvas.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  textW = tw;
  
  if (textW > TITLE_CV_W) {
    scrolling = true;
    titleCanvas.setCursor(scrollX, 1);
    titleCanvas.print(title);
    
    int secondX = scrollX + textW + SCROLL_GAP;
    if (secondX < TITLE_CV_W) {
      titleCanvas.setCursor(secondX, 1);
      titleCanvas.print(title);
    }
  } else {
    scrolling = false;
    scrollX = 0;
    titleCanvas.setCursor(0, 1);
    titleCanvas.print(title);
  }
  
  // Blit title canvas onto body canvas - pixels outside TITLE_CV_W are physically gone
  c.drawRGBBitmap(TXT_X, rowY, titleCanvas.getBuffer(), TITLE_CV_W, TITLE_CV_H);
}

// Artist line
static void drawArtist(GFXcanvas16& c) {
  String artist = valid(cur.artist) ? cur.artist : "Open music on Windows";
  artist = safeText(artist, 30);
  
  c.setTextSize(1);
  c.setTextColor(lighten(themeColor, 80));
  
  // Truncate with ellipsis if too long
  int16_t x1, y1; uint16_t tw, th;
  c.getTextBounds(artist, 0, 0, &x1, &y1, &tw, &th);
  while ((int)tw > TXT_W && artist.length() > 4) {
    artist = artist.substring(0, artist.length() - 2);
    c.getTextBounds(artist + "..", 0, 0, &x1, &y1, &tw, &th);
  }
  if (tw > TXT_W - 12) artist = artist.substring(0, artist.length() - 2) + "..";
  
  c.setCursor(TXT_X, ARTIST_Y - BODY_Y);
  c.print(artist);
}

// Volume popup overlay
static void drawVolumePopup(GFXcanvas16& c) {
  int pw = 130;
  int ph = 32;
  int px = (SW - pw) / 2;
  int py = (BODY_H - ph) / 2;
  
  // Shadow
  c.fillRoundRect(px + 2, py + 2, pw, ph, 6, C_BG);
  // Panel
  c.fillRoundRect(px, py, pw, ph, 6, C_PANEL_2);
  c.drawRoundRect(px, py, pw, ph, 6, themeColor);
  
  // Speaker icon (simple)
  int ix = px + 8;
  int iy = py + ph/2;
  c.fillTriangle(ix, iy - 4, ix + 6, iy - 6, ix + 6, iy + 6, C_TEXT);
  c.fillTriangle(ix, iy + 4, ix + 6, iy + 6, ix + 6, iy - 6, C_TEXT);
  if (vol > 30 && !muted) c.drawCircle(ix + 10, iy, 4, C_TEXT);
  if (vol > 70 && !muted) c.drawCircle(ix + 10, iy, 7, C_TEXT);
  if (muted) {
    c.drawLine(ix, iy - 6, ix + 14, iy + 6, C_DANGER);
    c.drawLine(ix, iy + 6, ix + 14, iy - 6, C_DANGER);
  }
  
  // Bar
  int barX = px + 28;
  int barY = py + ph/2 - 3;
  int barW = pw - 60;
  c.fillRoundRect(barX, barY, barW, 6, 3, C_BG);
  int fill = map(constrain(vol, 0, 100), 0, 100, 0, barW);
  if (fill > 0) {
    for (int i = 0; i < fill; i++) {
      uint8_t t = (uint8_t)((i * 255L) / barW);
      c.drawFastVLine(barX + i, barY, 6, blend565(themeColor, lighten(themeColor, 100), t));
    }
  }
  
  // Percent
  c.setTextSize(1);
  c.setTextColor(C_TEXT);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", vol);
  int16_t x1, y1; uint16_t tw, th;
  c.getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
  c.setCursor(px + pw - tw - 8, py + ph/2 - 3);
  c.print(buf);
}

// ------------------ Region draw functions ------------------

static void drawHeader() {
  headerCanvas.fillScreen(C_BG);
  fillGradV(headerCanvas, 0, 0, SW, HDR_H, darken(themeColor, 180), C_BG);
  
  headerCanvas.setTextSize(1);
  headerCanvas.setTextColor(lighten(themeColor, 60));
  headerCanvas.setCursor(4, 2);
  headerCanvas.print("NOW PLAYING");
  
  // App badge on right
  String app = valid(cur.app) ? safeText(cur.app, 12) : String("---");
  app.toUpperCase();
  int16_t x1, y1; uint16_t tw, th;
  headerCanvas.getTextBounds(app, 0, 0, &x1, &y1, &tw, &th);
  headerCanvas.setTextColor(C_MUTED);
  headerCanvas.setCursor(SW - tw - 4, 2);
  headerCanvas.print(app);
  
  // Status dot
  uint16_t dotC = valid(cur.name) ? (cur.playing ? themeColor : C_DIM) : C_DANGER;
  headerCanvas.fillCircle(SW - tw - 12, 5, 2, dotC);
  
  tft.drawRGBBitmap(0, 0, headerCanvas.getBuffer(), SW, HDR_H);
}

static void drawBody() {
  // Background gradient using theme color
  uint16_t bgTop = darken(themeColor, 220);
  uint16_t bgBot = C_BG;
  fillGradV(bodyCanvas, 0, 0, SW, BODY_H, bgTop, bgBot);
  
  drawAlbumArt(bodyCanvas);
  drawTitle(bodyCanvas);
  drawArtist(bodyCanvas);
  drawEqBars(bodyCanvas);
  drawProgress(bodyCanvas);
  
  if (volVisible) {
    drawVolumePopup(bodyCanvas);
  }
  
  tft.drawRGBBitmap(0, BODY_Y, bodyCanvas.getBuffer(), SW, BODY_H);
}

static void drawFooter() {
  footerCanvas.fillScreen(C_BG);
  fillGradV(footerCanvas, 0, 0, SW, FTR_H, C_BG, darken(themeColor, 200));
  
  // Top separator
  footerCanvas.drawFastHLine(0, 0, SW, darken(themeColor, 100));
  
  int cy = FTR_H / 2;
  
  // Prev button
  int px1 = 28;
  footerCanvas.fillCircle(px1, cy, 8, C_PANEL);
  // Draw prev triangle
  footerCanvas.fillTriangle(px1 + 2, cy - 4, px1 + 2, cy + 4, px1 - 3, cy, C_TEXT);
  footerCanvas.fillRect(px1 - 4, cy - 4, 2, 9, C_TEXT);
  
  // Play/Pause button (center, bigger)
  int px2 = SW / 2;
  pulsePhase += 0.15f;
  int pulse = cur.playing ? (int)(sin(pulsePhase) * 1.5f + 0.5f) : 0;
  footerCanvas.fillCircle(px2, cy, 10 + pulse, cur.playing ? themeColor : C_PANEL_2);
  footerCanvas.drawCircle(px2, cy, 10 + pulse, lighten(themeColor, 80));
  
  if (cur.playing) {
    // Pause icon
    footerCanvas.fillRect(px2 - 4, cy - 5, 3, 10, C_TEXT);
    footerCanvas.fillRect(px2 + 1, cy - 5, 3, 10, C_TEXT);
  } else {
    // Play icon
    footerCanvas.fillTriangle(px2 - 3, cy - 5, px2 - 3, cy + 5, px2 + 4, cy, C_TEXT);
  }
  
  // Next button
  int px3 = SW - 28;
  footerCanvas.fillCircle(px3, cy, 8, C_PANEL);
  footerCanvas.fillTriangle(px3 - 2, cy - 4, px3 - 2, cy + 4, px3 + 3, cy, C_TEXT);
  footerCanvas.fillRect(px3 + 2, cy - 4, 2, 9, C_TEXT);
  
  // Shuffle indicator (small, top-right of footer)
  if (shuf) {
    footerCanvas.fillRoundRect(SW - 18, 2, 16, 6, 2, themeColor);
    footerCanvas.setTextSize(1);
    footerCanvas.setTextColor(C_BG);
    footerCanvas.setCursor(SW - 16, 2);
    footerCanvas.print("SHF");
  }
  
  tft.drawRGBBitmap(0, FTR_Y, footerCanvas.getBuffer(), SW, FTR_H);
}

// ------------------ Serial protocol ------------------

static void parseTrackLine(const String& line) {
  // TRACK|artist|title|playing|pos|dur|vol|app|r|g|b
  int idx[11];
  int found = 0;
  idx[0] = -1;
  for (int i = 0; i < (int)line.length() && found < 10; i++) {
    if (line[i] == '|') idx[++found] = i;
  }
  if (found < 10) return;
  
  auto field = [&](int n) -> String {
    int start = idx[n] + 1;
    int end = (n + 1 <= 10) ? idx[n + 1] : line.length();
    return line.substring(start, end);
  };
  
  String artist = safeText(field(1), 60);
  String title  = safeText(field(2), 80);
  bool playing  = field(3).toInt() == 1;
  int pos       = field(4).toInt();
  int dur       = field(5).toInt();
  int newVol    = field(6).toInt();
  String app    = safeText(field(7), 20);
  int r = field(8).toInt();
  int g = field(9).toInt();
  int b = field(10).toInt();
  
  if (pos < 0) pos = 0;
  if (dur < 0) dur = 0;
  if (dur > 0 && pos > dur) pos = dur;
  
  bool themeChanged = false;
  if (r >= 0 && g >= 0 && b >= 0) {
    uint16_t nt = rgb565(r, g, b);
    if (nt != themeColor) {
      themeColor = nt;
      themeR = r; themeG = g; themeB = b;
      themeChanged = true;
    }
  }
  
  bool metaChanged = (title != cur.name) || (artist != cur.artist) || (app != cur.app);
  
  cur.name = title;
  cur.artist = artist;
  cur.playing = playing;
  cur.progress = pos;
  cur.duration = dur;
  cur.app = app;
  if (newVol >= 0) vol = constrain(newVol, 0, 100);
  
  if (metaChanged) {
    scrollX = 0;
    tScrollPause = millis();
  }
  
  if (metaChanged || themeChanged) {
    drawHeader();
  }
}

static void beginAlbumPacket(int w, int h, int len) {
  artReceiving = true;
  artAwaitTerm = false;
  artExpected = len;
  artIndex = 0;
  artW = w;
  artH = h;
  if (w == 0 || h == 0 || len == 0) {
    artReady = false;
    artReceiving = false;
    artAwaitTerm = false;
  }
}

static void handleSerialByte(uint8_t c) {
  if (artReceiving) {
    if (artIndex < artExpected) {
      int pixIdx = artIndex / 2;
      if (pixIdx < ART_SIZE * ART_SIZE) {
        if ((artIndex & 1) == 0) {
          albumArt[pixIdx] = ((uint16_t)c) << 8;
        } else {
          albumArt[pixIdx] |= c;
        }
      }
      artIndex++;
      if (artIndex >= artExpected) {
        artReceiving = false;
        artAwaitTerm = true;
        artReady = (artW == ART_SIZE && artH == ART_SIZE);
      }
    }
    return;
  }
  if (artAwaitTerm) {
    if (c == '\n') artAwaitTerm = false;
    return;
  }
  
  if (c == '\n') {
    rxLine.trim();
    if (rxLine.startsWith("TRACK|")) {
      parseTrackLine(rxLine);
    } else if (rxLine.startsWith("ART|")) {
      int p1 = rxLine.indexOf('|');
      int p2 = rxLine.indexOf('|', p1 + 1);
      int p3 = rxLine.indexOf('|', p2 + 1);
      if (p1 > 0 && p2 > 0 && p3 > 0) {
        int w = rxLine.substring(p1 + 1, p2).toInt();
        int h = rxLine.substring(p2 + 1, p3).toInt();
        int len = rxLine.substring(p3 + 1).toInt();
        if (len == 0) {
          artReady = false;
        } else if (w == ART_SIZE && h == ART_SIZE && len <= (int)sizeof(albumArt)) {
          beginAlbumPacket(w, h, len);
        }
      }
    } else if (rxLine.startsWith("VOL|")) {
      vol = constrain(rxLine.substring(4).toInt(), 0, 100);
      volVisible = true;
      tVol = millis();
    }
    rxLine = "";
  } else if (c != '\r') {
    rxLine += (char)c;
    if (rxLine.length() > 250) rxLine = "";
  }
}

// ------------------ Inputs ------------------

static void sendCommand(const String& cmd) {
  Serial.println(cmd);
}

void handleButtons() {
  unsigned long t = millis();
  
  bool curPlay = digitalRead(BTN_PLAY);
  if (curPlay == LOW && lastBtnState[0] == HIGH && t - tBtn[0] > D_BTN) {
    tBtn[0] = t;
    sendCommand("BTN:TOGGLE");
    cur.playing = !cur.playing;
  }
  lastBtnState[0] = curPlay;
  
  bool curNext = digitalRead(BTN_NEXT);
  if (curNext == LOW && lastBtnState[1] == HIGH && t - tBtn[1] > D_BTN) {
    tBtn[1] = t;
    sendCommand("BTN:NEXT");
  }
  lastBtnState[1] = curNext;
  
  bool curPrev = digitalRead(BTN_PREV);
  if (curPrev == LOW && lastBtnState[2] == HIGH && t - tBtn[2] > D_BTN) {
    tBtn[2] = t;
    sendCommand("BTN:PREV");
  }
  lastBtnState[2] = curPrev;
}

void handleEncoder() {
  unsigned long t = millis();
  int clk = digitalRead(ENC_CLK);
  bool sw = digitalRead(ENC_SW);
  
  if (clk != lastClk && clk == LOW && t - tClk > D_CLK) {
    tClk = t;
    int delta = (digitalRead(ENC_DT) != clk) ? -4 : 4;
    vol = constrain(vol + delta, 0, 100);
    sendCommand(String("BTN:VOL:") + String(vol));
    volVisible = true;
    tVol = t;
  }
  lastClk = clk;
  
  if (lastSw == HIGH && !sw && t - tSw > D_SW) {
    tSw = t;
    muted = !muted;
    sendCommand(muted ? "BTN:MUTE" : "BTN:UNMUTE");
    volVisible = true;
    tVol = t;
  }
  lastSw = sw;
  
  if (volVisible && (t - tVol >= T_VOL)) {
    volVisible = false;
  }
}

// ------------------ Setup / Loop ------------------

void setup() {
  Serial.begin(115200);
  randomSeed((uint32_t)micros());
  
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(ENC_CLK,  INPUT_PULLUP);
  pinMode(ENC_DT,   INPUT_PULLUP);
  pinMode(ENC_SW,   INPUT_PULLUP);
  
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.setTextWrap(false);
  tft.fillScreen(C_BG);
  
  bodyCanvas.setTextWrap(false);
  footerCanvas.setTextWrap(false);
  headerCanvas.setTextWrap(false);
  titleCanvas.setTextWrap(false);
  
  themeColor = C_ACCENT;
  themeR = 0; themeG = 200; themeB = 255;
  
  // Splash
  tft.setTextSize(2);
  tft.setTextColor(C_ACCENT);
  tft.setCursor(20, 50);
  tft.print("MediaCtrl");
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(38, 75);
  tft.print("Waiting for PC...");
  delay(800);
  tft.fillScreen(C_BG);
  
  drawHeader();
  drawBody();
  drawFooter();
}

void loop() {
  while (Serial.available()) {
    handleSerialByte((uint8_t)Serial.read());
  }
  
  handleButtons();
  handleEncoder();
  
  unsigned long t = millis();
  
  if (t - tUi >= T_UI) {
    tUi = t;
    frameId++;
    
    // Scroll logic with pause at start
    if (scrolling && !volVisible) {
      if (t - tScrollPause > SCROLL_PAUSE && t - tScroll >= T_SCROLL) {
        tScroll = t;
        scrollX -= 1;
        if (scrollX <= -(textW + SCROLL_GAP)) {
          scrollX = 0;
          tScrollPause = t;
        }
      }
    }
    
    drawBody();
    drawFooter();   // redraw for pulse animation
  }
}