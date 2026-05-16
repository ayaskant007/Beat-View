#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

char* SSID = "YOUR_WIFI_SSID";
char* PASSWORD: "YOUR_WIFI_PASSWORD";
const char* CLIENT_ID = "YOUR_CLIENT_ID";
const char* CLIENT_SECRET = "YOUR_CLIENT_SECRET";

#define TFT_CS 1
#define TFT_RST 2
#define TFT_DC 3
#define TFT_SCLK 4
#define TFT_MOSI 5

#define BTN_PREV 6
#define BTN_PLAY 7
#define BTN_NEXT 8

#define SPOTIFY_GREEN 0x1ED5
#define DARK_GRAY 0x2104
#define LIGHT_GRAY 0xAD55

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Spotify sp(CLIENT_ID, CLIENT_SECRET);

String lastTrack = "";
String lastArtist = "";
bool lastPlayState = false;

unsigned long lastApiCheck = 0;
const long apiInterval = 2500;

unsigned long lastScrollUpdate = 0;
const int scrollSpeed = 60;

int scrollCursorX = 10;
int trackTextWidth = 0;
bool isScrolling = false;


void drawHeader() {
  tft.fillRect(0, 0, 160, 20, ST77X_BLACK);
  tft.setTextSize(1);
  tft.setCursor(45, 6);
  tft.setTextColor(SPOTIFY_GREEN);
  tft.print("NOW PLAYING");
  tft.drawFastHLine(0, 20, 160, SPOTIFY_GREEN);
}

void drawFooter(bool isPlaying) {
  tft.fillRect(0, 105, 160, 23, ST77XX_BLACK);
  tft.drawFastHLine(0, 105, 160, DARK_GRAY);

  tft.fillTriangle(40, 110, 40, 122, 30, 116, SPOTIFY_GREEN);
  tft.fillRect(27, 110, 2, 12, SPOTIFY_GREEN);
} 

  if (isPlaying) {
    tft.fillRect(75, 110, 4, 12, SPOTIFY_GREEN);
    tft.fillRect(83, 110, 4, 12, SPOTIFY_GREEN);
  } else {
    tft.fillTriangle(75, 110, 75, 122, 87, 116, SPOTIFY_GREEN);
  }

tft.fillTriangle(120, 110, 120, 122, 130, 116, SPOTIFY_GREEN);
tft.fillRect(131, 110, 2, 12, SPOTIFY_GREEN);
}

void updateStaticData(String artist, String track) {
  tft.fillRect(0, 21, 160, 83, ST7XX_BLACK);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextSize(2);
  tft.getTextBounds(track, 0, 0, &x1, &y1, &w, &h);
  trackTextWidth = w;

  if (trackTextWidth > 150) {
    isScrolling = True;
    scrollCursorX = 10;
  } else {
    isScrolling = false;
    scrollCursorX = ( 160 - trackTextWidth) / 2;

    tft.setCursor(scrollCursorX, 40);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(track);
  }

  tft.setTextSize(1);
  tft.getTextBounds(artist, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((160 - w) / 2,75);
  tft.setTextColor(LIGHT_GRAY);
  tft.print(artist);
}


void handleButtons() {
  if (digitalRead(BTN_PLAY) == LOW) {
    sp.start_resume_playback();
    delay(250);
  }
  if (digitalRead(BTN_NEXT) == LOW){
    sp.skip();
    lastApiCheck = 0;
    delay(250);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(20, 60);
  tft.setTextColor(SPOTIFY_GREEN);
  tft.print("Booting System...");

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  sp.begin();
  while (!sp.is_auth()) { sp.handle_client(); }

  tft.fillScreen(ST77XX_BLACK);
  drawHeader();
}

void loop() {
  unsigned long currentMillis = millis();

  handleButtons();

  if (isScrolling && (currentMillis - lastScrollUpdate >= scrollSpeed)) {
    lastScrollUpdate = CurrentMillis;

    tft.fillRect(0, 38, 160, 18, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(scrollCursorX, 40);
    tft.print(lastTrack);

    scrollCursorX -= 2;

    if (scrollCursorX < -trackTextWidth) {
      scrollCursorX = 160;
    }
  }

  if (currentMillis - lastApiCheck >= apiInterval) {
    lastApiCheck = currentMillis;

    String currentArtist = sp.current_artist_names();
    String currentTrack = sp.current_track_name();
    bool currentPlayState = sp.is_playing();

    bool trackChanged = (currentTrack != lastTrack && currentTrack != "null" && currentTrack != "");
    bool stateChanged = (currentPlayState != lastPlayState);

    if (trackChanged) {
      lastTrack = currentTrack;
      lastArtist = currentArtist;
      updateStaticData(lastArtist, lastTrack);
    }
    
    if (stateChanged || trackChanged) {
      lastPlayState = currentPlayState;
      drawFooter(lastPlayState);
    }
  }
}