#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

const char* SSID          = "YOUR_WIFI_SSID";
const char* PASSWORD      = "YOUR_WIFI_PASSWORD";
const char* CLIENT_ID     = "YOUR_CLIENT_ID";
const char* CLIENT_SECRET = "YOUR_CLIENT_SECRET";

#define TFT_CS   1
#define TFT_RST  2
#define TFT_DC   3
#define TFT_SCLK 4
#define TFT_MOSI 5
#define BTN_PREV 6
#define BTN_PLAY 7
#define BTN_NEXT 8
#define ENC_CLK  0
#define ENC_DT   10
#define ENC_SW   20

#define SW      160
#define SH      128
#define HDR_H   18
#define FTR_Y   108
#define CTN_Y   19
#define CTN_H   (FTR_Y - CTN_Y)
#define BAR_Y   92
#define BAR_H   3
#define BAR_PAD 8
#define TRK_Y   42
#define ART_Y   68

#define GREEN  0x1DCA
#define BLACK  0x0000
#define DGRAY  0x18C3
#define MGRAY  0x632C
#define LGRAY  0xAD55
#define WHITE  0xFFFF
#define DGREEN 0x0E65

#define T_API    2500
#define T_SCROLL   55
#define T_EQ      120
#define T_VOL    1800
#define D_BTN     200
#define D_CLK       5
#define D_SW      300
#define SCROLL_PX   2
#define SCROLL_GAP 28

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Spotify sp(CLIENT_ID, CLIENT_SECRET);

struct Track {
    String name, artist;
    bool   playing  = false;
    int    duration = 0, progress = 0;
};

Track cur;

int  vol      = 50;
bool shuf     = false;
int  lastClk  = HIGH;
bool lastSw   = HIGH;
int  scrollX  = SW;
int  textW    = 0;
bool scrolling  = false;
bool volVisible = false;

int eqH[4]   = {4, 8, 6, 5};
int eqDir[4] = {1, -1, 1, -1};

unsigned long tApi = 0, tScroll = 0, tEq = 0, tVol = 0;
unsigned long tBtn[3] = {}, tClk = 0, tSw = 0;

bool valid(const String& s) {
    return s.length() > 0 && s != "null" && s != "error" && s != "Something went wrong";
}

void wifiReconnect() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.disconnect();
    WiFi.begin(SSID, PASSWORD);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 8000) delay(200);
}

void animBoot() {
    tft.fillScreen(BLACK);

    for (int x = SW / 2; x >= 0; x -= 3) {
        tft.drawFastHLine(x, SH / 2, SW - 2 * x, GREEN);
        delay(6);
    }
    delay(80);
    tft.fillScreen(BLACK);

    const char* logo = "BEAT VIEW";
    tft.setTextSize(2);
    tft.setTextColor(GREEN);
    int logoX = (SW - 6 * 12) / 2;
    for (int i = 0; i < 6; i++) {
        tft.setCursor(logoX + i * 12, SH / 2 - 8);
        tft.print(logo[i]);
        delay(70);
    }

    delay(250);

    int bx = 20, by = SH / 2 + 18, bw = SW - 40;
    tft.drawRect(bx - 1, by - 1, bw + 2, 5, DGRAY);
    for (int i = 0; i <= bw; i += 2) {
        tft.fillRect(bx, by, i, 3, GREEN);
        delay(5);
    }
    delay(180);
    tft.fillScreen(BLACK);
}

void animWipe() {
    for (int x = 0; x < SW + 6; x += 4) {
        if (x > 8) tft.fillRect(x - 8, CTN_Y, 8, CTN_H, BLACK);
        tft.fillRect(x, CTN_Y, 4, CTN_H, GREEN);
    }
    tft.fillRect(SW - 4, CTN_Y, 4, CTN_H, BLACK);
}

void animProgressReveal(int toPct) {
    int bx = BAR_PAD, bw = SW - BAR_PAD * 2;
    tft.fillRect(bx, BAR_Y, bw, BAR_H, DGRAY);
    int target = map(constrain(toPct, 0, 100), 0, 100, 0, bw);
    for (int f = 0; f <= target; f += 2) {
        tft.fillRect(bx, BAR_Y, f, BAR_H, GREEN);
        tft.fillCircle(bx + f, BAR_Y + BAR_H / 2, 3, GREEN);
        if (f > 2) tft.fillRect(bx + f - 2, BAR_Y - 1, 3, BAR_H + 2, BLACK);
        tft.fillRect(bx, BAR_Y, f, BAR_H, GREEN);
        tft.fillCircle(bx + f, BAR_Y + BAR_H / 2, 3, GREEN);
        delay(4);
    }
}

void updateEQ(bool playing) {
    const int xs[4]  = {SW - 36, SW - 28, SW - 20, SW - 12};
    const int maxH   = 11;
    const int baseY  = HDR_H - 3;

    for (int i = 0; i < 4; i++) {
        tft.fillRect(xs[i], baseY - maxH, 5, maxH, BLACK);
        if (playing) {
            eqH[i] += eqDir[i] * random(1, 3);
            if (eqH[i] >= maxH) { eqH[i] = maxH; eqDir[i] = -1; }
            if (eqH[i] <= 2)    { eqH[i] = 2;    eqDir[i] =  1; }
            tft.fillRect(xs[i], baseY - eqH[i], 5, eqH[i], GREEN);
        } else {
            tft.fillRect(xs[i], baseY - 2, 5, 2, DGRAY);
        }
    }
}

void drawHeader() {
    tft.fillRect(0, 0, SW, HDR_H, BLACK);
    tft.setTextSize(1);
    tft.setTextColor(GREEN);
    tft.setCursor(8, 6);
    tft.print("NOW PLAYING");
    tft.drawFastHLine(0, HDR_H, SW, GREEN);
}

void drawFooter(bool playing) {
    tft.fillRect(0, FTR_Y, SW, SH - FTR_Y, BLACK);
    tft.drawFastHLine(0, FTR_Y, SW, DGRAY);

    tft.fillTriangle(30, 113, 30, 124, 20, 118, GREEN);
    tft.fillRect(17, 113, 2, 12, GREEN);

    if (playing) {
        tft.fillRect(76, 113, 4, 12, GREEN);
        tft.fillRect(84, 113, 4, 12, GREEN);
    } else {
        tft.fillTriangle(76, 113, 76, 124, 88, 118, GREEN);
    }

    tft.fillTriangle(130, 113, 130, 124, 140, 118, GREEN);
    tft.fillRect(141, 113, 2, 12, GREEN);

    if (shuf) tft.fillCircle(153, 113, 3, GREEN);
}

void drawProgressBar(int pct) {
    pct = constrain(pct, 0, 100);
    int bx = BAR_PAD, bw = SW - BAR_PAD * 2;
    tft.fillRect(bx, BAR_Y, bw, BAR_H, DGRAY);
    int filled = map(pct, 0, 100, 0, bw);
    if (filled > 0) tft.fillRect(bx, BAR_Y, filled, BAR_H, GREEN);
    tft.fillCircle(bx + filled, BAR_Y + BAR_H / 2, 3, GREEN);
}

void showVolPopup(int v) {
    tft.fillRect(35, 72, 90, 16, DGRAY);
    tft.drawRect(35, 72, 90, 16, GREEN);
    tft.setTextSize(1);
    tft.setCursor(40, 77);
    tft.setTextColor(WHITE);
    tft.print("Vol: ");
    tft.print(v);
    tft.print("%");
    int ix = 76, iw = 44;
    tft.fillRect(ix, 76, iw, 4, MGRAY);
    tft.fillRect(ix, 76, map(v, 0, 100, 0, iw), 4, GREEN);
    volVisible = true;
    tVol = millis();
}

void clearVolPopup() {
    tft.fillRect(0, 70, SW, 20, BLACK);
    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(1);
    tft.getTextBounds(cur.artist, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SW - w) / 2, ART_Y);
    tft.setTextColor(LGRAY);
    tft.print(cur.artist);
    drawProgressBar(cur.duration > 0 ? map(cur.progress, 0, cur.duration, 0, 100) : 0);
    volVisible = false;
}

void renderTrack(const String& artist, const String& track) {
    tft.fillRect(0, CTN_Y, SW, CTN_H, BLACK);

    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(2);
    tft.getTextBounds(track, 0, 0, &x1, &y1, &w, &h);
    textW = (int)w;

    if (textW > SW - 10) {
        scrolling = true;
        scrollX   = SW;
    } else {
        scrolling = false;
        tft.setCursor((SW - textW) / 2, TRK_Y);
        tft.setTextColor(WHITE);
        tft.print(track);
    }

    tft.setTextSize(1);
    tft.getTextBounds(artist, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SW - w) / 2, ART_Y);
    tft.setTextColor(LGRAY);
    tft.print(artist);

    int pct = (cur.duration > 0) ? map(cur.progress, 0, cur.duration, 0, 100) : 0;
    animProgressReveal(pct);
}

void handleButtons() {
    unsigned long t = millis();
    if (digitalRead(BTN_PLAY) == LOW && t - tBtn[0] > D_BTN) {
        tBtn[0] = t;
        cur.playing ? sp.pause_playback() : sp.start_resume_playback();
        cur.playing = !cur.playing;
        drawFooter(cur.playing);
    }
    if (digitalRead(BTN_NEXT) == LOW && t - tBtn[1] > D_BTN) {
        tBtn[1] = t; sp.skip(); tApi = 0;
    }
    if (digitalRead(BTN_PREV) == LOW && t - tBtn[2] > D_BTN) {
        tBtn[2] = t; sp.previous(); tApi = 0;
    }
}

void handleEncoder() {
    unsigned long t = millis();
    int clk = digitalRead(ENC_CLK);
    if (clk != lastClk && clk == LOW && t - tClk > D_CLK) {
        tClk = t;
        vol = constrain(vol + (digitalRead(ENC_DT) != clk ? -5 : 5), 0, 100);
        sp.set_volume(vol);
        showVolPopup(vol);
    }
    lastClk = clk;

    bool sw = digitalRead(ENC_SW);
    if (lastSw == HIGH && !sw && t - tSw > D_SW) {
        tSw = t;
        shuf = !shuf;
        sp.set_shuffle(shuf);
        drawFooter(cur.playing);
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
    lastClk = digitalRead(ENC_CLK);
    lastSw  = digitalRead(ENC_SW);

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(BLACK);

    animBoot();

    tft.setTextSize(1);
    tft.setTextColor(MGRAY);
    tft.setCursor(30, SH / 2 - 4);
    tft.print("CONNECTING...");
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    tft.fillRect(0, SH / 2 - 8, SW, 16, BLACK);
    tft.setCursor(22, SH / 2 - 4);
    tft.print("AUTHENTICATING...");
    sp.begin();
    while (!sp.is_auth()) sp.handle_client();

    tft.fillScreen(BLACK);
    drawHeader();
    drawFooter(false);
}

void loop() {
    unsigned long t = millis();

    sp.handle_client();
    wifiReconnect();
    handleButtons();
    handleEncoder();

    if (volVisible && t - tVol >= T_VOL)    clearVolPopup();
    if (t - tEq >= T_EQ) { tEq = t; updateEQ(cur.playing); }

    if (scrolling && !volVisible && t - tScroll >= T_SCROLL) {
        tScroll = t;
        tft.fillRect(0, TRK_Y - 2, SW, 20, BLACK);
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        tft.setCursor(scrollX, TRK_Y);
        tft.print(cur.name);
        scrollX -= SCROLL_PX;
        if (scrollX < -(textW + SCROLL_GAP)) scrollX = SW;
    }

    if (t - tApi >= T_API) {
        tApi = t;

        String newTrack  = sp.current_track_name();
        String newArtist = sp.current_artist_names();
        bool   newPlay   = sp.is_playing();
        int    newProg   = sp.current_playback_progress();
        int    newDur    = sp.current_track_duration();

        bool changed = valid(newTrack) && newTrack != cur.name;

        if (changed) {
            animWipe();
            cur.name = newTrack; cur.artist = newArtist;
            cur.duration = newDur; cur.progress = newProg;
            renderTrack(cur.artist, cur.name);
        }

        if (newDur > 0 && !volVisible) {
            cur.progress = newProg; cur.duration = newDur;
            drawProgressBar(map(cur.progress, 0, cur.duration, 0, 100));
        }

        if (newPlay != cur.playing || changed) {
            cur.playing = newPlay;
            drawFooter(cur.playing);
        }
    }
}
