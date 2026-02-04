/*
 * =====================================================
 * FRIYAY FOREVER - Protocol 1.0 (v26 - CLEANED UP)
 * For ESP32-8048S043C (4.3" 800x480 RGB Display)
 * =====================================================
 *
 * v26 Changes from v25:
 * - Removed ~100 lines of dead code and unused variables
 * - Removed redundant compatibility aliases (using globals directly)
 * - Fixed ADS1115 crash bug (added adsOK flag)
 * - Fixed morse pattern bounds check
 * - Extracted drawCyberpunkGrid() helper function
 * - Removed unused firstName from Friend struct
 * - Removed unused albumArtReady/spotifyCodeReady flags
 * - Added magic number constants
 * - Cleaner, more maintainable code
 */

#include <Arduino.h>
#include <FastLED.h>  // MUST be before Arduino_GFX_Library to avoid RED macro conflict
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include <Wire.h>
#include <Preferences.h>
#include <TAMC_GT911.h>
#include <JPEGDEC.h>
#include <Adafruit_ADS1X15.h>
#include "qr_code.h"  // Embedded QR code image
#include "ota_updates.h"  // OTA firmware updates

// ============================================================
// CONFIGURATION - CHANGE THESE FOR EACH UNIT
// ============================================================

#define MY_FRIEND_INDEX 0  // 0=NM, 1=ST, 2=GO, 3=TD, 4=MN

#define BOT_TOKEN "8274851974:AAEao868jidxcQEnY8IxPK91ujLmOsA_Alg"

struct Friend {
  const char* initials;
  int64_t telegramId;
  bool committed;
};

Friend friends[] = {
  {"NM", 7612996805LL, false},
  {"ST", 7015581601LL, false},
  {"GO", 8252040084LL, false},
  {"TD", 8293810017LL, false},
  {"MN", 8472668102LL, false}
};
#define NUM_FRIENDS 5

#define LATITUDE 35.9132
#define LONGITUDE -79.0558

// ============================================================
// PIN DEFINITIONS - ESP32-8048S043C
// ============================================================
#define GFX_BL 2
#define MQ135_PIN 12

// GT911 Touch
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38

// WS2812B LED Strip
#define LED_PIN 13
#define LED_COUNT 7
#define LED_BRIGHTNESS 128

// ============================================================
// DISPLAY SETUP - ESP32-8048S043C
// ============================================================
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
    0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 16000000 /* prefer_speed */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel);

// GT911 Touch
TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, 800, 480);

// ============================================================
// COLORS (RGB565)
// ============================================================
#define COL_BLACK       0x0000
#define COL_WHITE       0xFFFF
#define COL_YELLOW      0xFEA0
#define COL_CYAN        0x07FF
#define COL_GREEN       0x3CA4
#define COL_VU_GREEN    0x07E0
#define COL_RED         0xF800
#define COL_ORANGE      0xFC40
#define COL_GRAY        0x52AA
#define COL_DARK_GRAY   0x31A6
#define COL_SPOTIFY_BG  0x1807
#define COL_SCANNER     0x055F
#define COL_GRID        0x2115

// ============================================================
// LAYOUT CONSTANTS (800x480)
// ============================================================
#define SCREEN_W 800
#define SCREEN_H 480
#define MARGIN 15
#define Y_OFFSET 12  // Shift entire UI down for enclosure alignment

// Notification box
#define NOTIF_W 305
#define NOTIF_H 50
#define NOTIF_X (SCREEN_W - NOTIF_W - MARGIN)
#define NOTIF_Y (MARGIN + Y_OFFSET)

// Friend buttons
#define BTN_Y (10 + Y_OFFSET)
#define BTN_H 50
#define BTN_W 60
#define BTN_GAP 6
#define COMMIT_W 80

#define BOTTOM_LINE (SCREEN_H - MARGIN + Y_OFFSET)

// Spotify/Album art area
#define ALBUM_ART_W 225
#define ALBUM_ART_H 280
#define ALBUM_ART_DISPLAY_H 210  // v26: Named constant for actual display height
#define SPOT_HEADER_H 45
#define SPOT_TOTAL_H (SPOT_HEADER_H + ALBUM_ART_H)
#define SPOT_BOTTOM BOTTOM_LINE
#define SPOT_TOP (SPOT_BOTTOM - SPOT_TOTAL_H)
#define ART_X (SCREEN_W - ALBUM_ART_W - MARGIN)
#define ART_AREA_Y (SPOT_TOP + SPOT_HEADER_H)
#define QR_OFFSET_X 22   // v26: Named constant
#define QR_OFFSET_Y 10   // v26: Named constant

// VU meters
#define VU_W 38
#define VU_GAP 10
#define VU_TOTAL_W (VU_W * 2 + VU_GAP)
#define VU_TO_ART_GAP 15
#define VU_TO_PANEL_GAP 8
#define VU_X (ART_X - VU_TO_ART_GAP - VU_TOTAL_W)
#define VU_TOP SPOT_TOP
#define VU_BOTTOM BOTTOM_LINE
#define VU_H (VU_BOTTOM - VU_TOP)

// Timer
#define TIMER_H 140
#define TIMER_BOTTOM BOTTOM_LINE
#define TIMER_Y (TIMER_BOTTOM - TIMER_H)
#define TIMER_X MARGIN
#define TIMER_W (VU_X - VU_TO_PANEL_GAP - MARGIN)

// Weather panel
#define PANEL_X MARGIN
#define PANEL_TOP SPOT_TOP
#define PANEL_BOTTOM (TIMER_Y - 8)
#define PANEL_H (PANEL_BOTTOM - PANEL_TOP)
#define PANEL_W TIMER_W
#define PANEL_Y PANEL_TOP

// Days row
#define DAY_H 28
#define DAYS_Y (PANEL_TOP - 5 - DAY_H)
#define HEADER_Y (DAYS_Y + DAY_H / 2)

// Weather bars
#define WEATHER_START_Y (PANEL_Y + 30)
#define WEATHER_ROW_GAP ((PANEL_H - 25) / 3)
#define BLOCK_SIZE 28
#define BLOCK_GAP 4

// Grid pattern
#define GRID_SPACING 25

// ============================================================
// TIMING CONSTANTS
// ============================================================
#define SPLASH_DURATION_MS 2500
#define MSG_DISPLAY_TIME_MS 34000
#define MSG_HIGHLIGHT_TIME_MS 30000
#define DAY_AUTO_RESET_MS 20000
#define COMMIT_ANIM_DURATION 3000
#define MAX_WIFI_NETWORKS 4
#define MAX_BOUNCES 16
#define SCANNER_SPEED 8

// LED breathing timing
#define BREATH_NORMAL_CYCLE 480   // 8 seconds (4+4)
#define BREATH_FAST_CYCLE 360     // 6 seconds (3+3)
#define BREATH_FASTER_CYCLE 120   // 2 seconds (1+1)

// Morse code pattern: durations in ms (positive=ON, negative=OFF)
const int MORSE_PATTERN[] = {
  600, -200, 200, -200, 600, -600,  // K: -.-
  200, -200, 200, -600,              // I: ..
  600, -200, 200,                    // N: -.
  0                                  // END marker
};
#define MORSE_PATTERN_LENGTH 13

// ============================================================
// GLOBAL STATE
// ============================================================

// WiFi and Setup
Preferences prefs;
String savedSSID = "";
String savedPass = "";
bool wifiOK = false;
bool inSetup = false;
int wifiStrength = 4;

WebServer server(80);
DNSServer dns;
bool kbVisible = false;
String kbInput = "";
bool capsOn = false;
int selNetwork = -1;
String networks[15];
int netCount = 0;

// Weather
float currTemp = 70;
float precipitation = 0;
int wetLvl = 5;
int tmpLvl = 5;
int fukLvl = 5;
bool weatherOK = false;
int selectedDay = -1;
unsigned long lastDaySelectTime = 0;
float forecastHighTemp[7] = {70, 70, 70, 70, 70, 70, 70};
float forecastRain[7] = {0, 0, 0, 0, 0, 0, 0};
bool forecastLoaded = false;

// Sensors
int aqiLvl = 5;
int co2Lvl = 5;
bool adsOK = false;  // v26: Track ADS1115 status

// Time
struct tm tinfo;
int dayOfWeek = 0;
long secToFri = 0;
int hrsLeft = 0;
int minLeft = 0;
int secLeft = 0;

// Messages
String currMsg = "";
bool newMsg = false;
unsigned long msgTime = 0;
bool showingMsg = false;
int msgScrollPos = 0;

// Animations
bool showCommitAnim = false;
unsigned long commitAnimStart = 0;
bool scannerActive = false;

// Debounce
unsigned long lastCommitTime = 0;
#define COMMIT_DEBOUNCE_MS 3000  // 3 second cooldown between commits
int scannerPos = 4;
int scannerDirection = 1;
int scannerBounces = 0;
bool zeroTriggered = false;  // v26: Moved from static local to global

// LED state
enum LedAnimationType { LED_BREATHING, LED_MORSE_PURPLE, LED_MORSE_RED };
LedAnimationType currentLedAnim = LED_BREATHING;
int breathPhase = 0;
bool morseActive = false;
unsigned long morseStepStart = 0;
int morseStep = 0;
int lastCycleFrames = BREATH_NORMAL_CYCLE;

// Spotify
bool hasSpotify = false;
String trackId = "";
String albumArtUrl = "";
String spotifyCodeUrl = "";
String spotifySenderInitials = "";
bool showingQRCode = false;
JPEGDEC jpeg;

// Touch
enum TouchState { TOUCH_IDLE, TOUCH_PRESSED, TOUCH_HELD };
int touchX = 0, touchY = 0;
bool touchOK = false;
TouchState touchState = TOUCH_IDLE;
bool wasTouched = false;
int savedTouchX = 0, savedTouchY = 0;

// Timing trackers
unsigned long lastWeather = 0;
unsigned long lastBot = 0;
unsigned long lastDisp = 0;
unsigned long lastSensor = 0;
unsigned long lastAnim = 0;
unsigned long lastQRCheck = 0;

// Network clients
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Hardware
Adafruit_ADS1115 ads;
CRGB leds[LED_COUNT];

// OTA Updates
OTAUpdater otaUpdater;
unsigned long lastOTACheck = 0;
bool otaInProgress = false;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================
void showSplash();
void initTouch();
bool checkTouch();
void handleTouch();
void handleSetupTouch();
void handleKBTouch();
void toggleCommit();
void drawUI();
void drawButtons();
void drawNotificationBox();
void drawDays();
void drawWeatherPanel();
void drawWeatherBars();
void drawTimer();
void drawVUMeters();
void drawMeter(int x, int y, int w, int h, int level, const char* label);
void drawHeader();
void drawWifiIcon(int x, int y);
void drawProfileIcon(int x, int y);
void drawSpotifyArea();
void drawCyberpunkGrid(int x, int y, int w, int h);  // v26: New helper
void drawSenderBadge();
void startWiFiSetup();
void drawNetList();
void drawKeyboard();
void doConnect();
void tryConnect();
void handleRoot();
void checkTelegram();
void showMessage(String msg);
int getFriendIdx(int64_t id);
void broadcast(String msg);
void parseSpotify(String text);
void getWeather();
void selectDay(int dayIndex);
void calcWeatherForDay(int dayIndex);
void fetchSpotifyArt();
void getSpotifyCode();
uint8_t* downloadImageFromUrl(String url, int* outLen);
void downloadAndDisplayImage();
void downloadAndDisplayCode();
void decodeAndDisplayJpeg(uint8_t *buffer, int size);
void decodeAndDisplayCode(uint8_t *buffer, int size);
void checkQRReminder();
void displayQRPlaceholder();
int jpegDrawCallback(JPEGDRAW *pDraw);
int jpegDrawCallbackCode(JPEGDRAW *pDraw);
int jpegDrawCallbackQR(JPEGDRAW *pDraw);
String sanitizeMessage(String msg);
void calcWeather();
int calculateWifiStrength(int rssi);
void readSensors();
void calcCountdown();
void checkReset();
void updateAnimations();
void triggerScanner();
void triggerMorseLED(LedAnimationType type);
void updateLedAnimations();
void updateBreathingLED();
void updateMorseLED();
uint16_t getGradientColor(int segment, int maxSegments);
void otaProgressCallback(int progress);
void checkForOTAUpdates();

// ============================================================
// TOUCH INITIALIZATION & READING
// ============================================================

void initTouch() {
  Serial.println("   Starting GT911 init...");
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  ts.begin();
  ts.setRotation(ROTATION_NORMAL);
  touchOK = true;
  Serial.println("   GT911 initialized!");
}

bool checkTouch() {
  if (!touchOK) return false;

  ts.read();
  bool currentlyTouched = ts.isTouched;

  switch (touchState) {
    case TOUCH_IDLE:
      if (currentlyTouched && !wasTouched) {
        int rawX = ts.points[0].x;
        int rawY = ts.points[0].y;

        savedTouchX = map(rawX, 792, 325, 0, 800);
        savedTouchY = map(rawY, 471, 209, 0, 480);
        savedTouchX = constrain(savedTouchX, 0, SCREEN_W - 1);
        savedTouchY = constrain(savedTouchY, 0, SCREEN_H - 1);

        touchState = TOUCH_PRESSED;
        wasTouched = true;
      }
      break;

    case TOUCH_PRESSED:
      if (!currentlyTouched) {
        touchX = savedTouchX;
        touchY = savedTouchY;
        touchState = TOUCH_IDLE;
        wasTouched = false;
        Serial.printf("[TOUCH] Tap at (%d, %d)\n", touchX, touchY);
        return true;
      }
      break;

    case TOUCH_HELD:
      if (!currentlyTouched) {
        touchState = TOUCH_IDLE;
        wasTouched = false;
      }
      break;
  }

  return false;
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  FRIYAY FOREVER Protocol 1.0 - v26");
  Serial.println("  CLEANED UP VERSION");
  Serial.println("========================================");
  Serial.printf("Unit owner: %s\n\n", friends[MY_FRIEND_INDEX].initials);

  // Initialize display
  Serial.println("[1/5] Init display...");
  gfx->begin();
  gfx->fillScreen(COL_BLACK);
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  Serial.println("   Display OK");

  showSplash();

  // Initialize touch
  Serial.println("[2/5] Init touch...");
  initTouch();

  // Initialize sensors and hardware
  Serial.println("[3/5] Init sensors & hardware...");
  pinMode(MQ135_PIN, INPUT);
  analogReadResolution(12);

  // ADS1115 with error handling
  adsOK = ads.begin();
  if (!adsOK) {
    Serial.println("   ADS1115 FAILED - using fallback");
  } else {
    Serial.println("   ADS1115 initialized OK");
    ads.setGain(GAIN_ONE);
  }

  // LED strip
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, LED_COUNT, CRGB(0, 255, 255));
  FastLED.show();
  Serial.println("   LED strip OK");

  // WiFi connection
  Serial.println("[4/5] Check WiFi...");
  prefs.begin("friyay", false);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  Serial.printf("   Saved SSID: %s\n", savedSSID.c_str());

  if (savedSSID.length() > 0) {
    tryConnect();
  }

  if (!wifiOK) {
    Serial.println("   Starting WiFi setup...");
    startWiFiSetup();
    return;
  }

  // Time sync
  Serial.println("[5/5] Sync time...");
  configTime(-5 * 3600, 3600, "pool.ntp.org");

  int tries = 0;
  while (!getLocalTime(&tinfo) && tries < 8) {
    delay(500);
    tries++;
    yield();
  }
  Serial.println("   Time synced");

  getLocalTime(&tinfo);
  dayOfWeek = tinfo.tm_wday;

  client.setInsecure();
  client.setTimeout(1500);
  getWeather();

  wifiStrength = calculateWifiStrength(WiFi.RSSI());

  drawUI();
  displayQRPlaceholder();

  // Initialize OTA updater
  otaUpdater.setProgressCallback(otaProgressCallback);
  Serial.printf("[OTA] Firmware version: %s\n", otaUpdater.getCurrentVersion().c_str());

  Serial.println();
  Serial.println("========================================");
  Serial.println("  READY!");
  Serial.println("========================================");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  if (inSetup) {
    dns.processNextRequest();
    server.handleClient();
    if (checkTouch()) handleSetupTouch();
    delay(10);
    return;
  }

  unsigned long now = millis();

  // Animation update (60fps)
  if (now - lastAnim >= 16) {
    lastAnim = now;
    updateAnimations();
  }

  // 1-second update
  if (now - lastDisp >= 1000) {
    lastDisp = now;
    getLocalTime(&tinfo);
    dayOfWeek = tinfo.tm_wday;
    calcCountdown();
    drawTimer();

    if (WiFi.status() == WL_CONNECTED) {
      int newStrength = calculateWifiStrength(WiFi.RSSI());
      if (newStrength != wifiStrength) {
        wifiStrength = newStrength;
        drawHeader();
      }
    }

    checkReset();
    checkQRReminder();
  }

  // Auto-reset day selection
  if (selectedDay >= 0 && lastDaySelectTime > 0 &&
      (now - lastDaySelectTime >= DAY_AUTO_RESET_MS)) {
    selectDay(-1);
  }

  // Telegram check (15 seconds)
  if (now - lastBot >= 15000) {
    lastBot = now;
    checkTelegram();
  }

  // Weather update (1 hour)
  if (now - lastWeather >= 3600000) {
    lastWeather = now;
    getWeather();
    drawWeatherBars();
  }

  // Sensor update (5 seconds)
  if (now - lastSensor >= 5000) {
    lastSensor = now;
    readSensors();
    drawVUMeters();
  }

  // OTA update check (every 24 hours, but staggered by unit to avoid all checking at once)
  // Each unit checks at a different hour based on MY_FRIEND_INDEX
  if (!otaInProgress && now - lastOTACheck >= 86400000) {  // 24 hours
    // Only check if it's the designated hour for this unit (spreads load)
    int checkHour = 3 + MY_FRIEND_INDEX;  // Units check at 3am, 4am, 5am, 6am, 7am
    if (tinfo.tm_hour == checkHour && tinfo.tm_min < 5) {
      lastOTACheck = now;
      checkForOTAUpdates();
    }
  }

  if (checkTouch()) handleTouch();

  // WiFi maintenance
  if (WiFi.status() != WL_CONNECTED) {
    wifiOK = false;
    tryConnect();
    if (!wifiOK) startWiFiSetup();
  }

  delay(10);
}

// ============================================================
// ANIMATIONS
// ============================================================

void updateAnimations() {
  bool needNotifRedraw = false;
  bool needTimerRedraw = false;

  updateLedAnimations();

  // Scanner animation
  if (scannerActive) {
    scannerPos += SCANNER_SPEED * scannerDirection;

    if (scannerDirection == 1 && scannerPos >= NOTIF_W - 10) {
      scannerDirection = -1;
      scannerBounces++;
      scannerPos = NOTIF_W - 10;
    } else if (scannerDirection == -1 && scannerPos <= 4) {
      scannerDirection = 1;
      scannerBounces++;
      scannerPos = 4;
    }

    if (scannerBounces >= MAX_BOUNCES) {
      scannerActive = false;
      scannerPos = 4;
      scannerDirection = 1;
      scannerBounces = 0;
    }

    needNotifRedraw = true;
  }

  // Message scroll
  if (showingMsg && currMsg.length() > 12) {
    msgScrollPos += 2;
    int totalScrollWidth = (int)currMsg.length() * 30 + TIMER_W;
    if (msgScrollPos > totalScrollWidth) {
      msgScrollPos = -TIMER_W / 2;
    }
    needTimerRedraw = true;
  }

  if (needNotifRedraw) drawNotificationBox();
  if (needTimerRedraw) drawTimer();
}

void triggerScanner() {
  scannerActive = true;
  scannerPos = 4;
  scannerDirection = 1;
  scannerBounces = 0;
  triggerMorseLED(LED_MORSE_PURPLE);
}

// ============================================================
// LED ANIMATIONS
// ============================================================

void triggerMorseLED(LedAnimationType type) {
  currentLedAnim = type;
  morseActive = true;
  morseStep = 0;
  morseStepStart = millis();
}

int getBreathingCycleFrames() {
  if (dayOfWeek == 5 && tinfo.tm_hour == 14) {
    if (tinfo.tm_min >= 59) return BREATH_FASTER_CYCLE;
    if (tinfo.tm_min >= 50) return BREATH_FAST_CYCLE;
  }
  return BREATH_NORMAL_CYCLE;
}

void updateBreathingLED() {
  int cycleFrames = getBreathingCycleFrames();
  int dimFrames = cycleFrames / 2;

  if (cycleFrames != lastCycleFrames) {
    breathPhase = 0;
    lastCycleFrames = cycleFrames;
  }

  breathPhase = (breathPhase + 1) % cycleFrames;

  uint8_t brightness;
  if (breathPhase < dimFrames) {
    brightness = map(breathPhase, 0, dimFrames - 1, 255, 20);
  } else {
    brightness = map(breathPhase, dimFrames, cycleFrames - 1, 20, 255);
  }

  fill_solid(leds, LED_COUNT, CRGB(0, brightness, brightness));
  FastLED.show();
}

void updateMorseLED() {
  // v26 FIX: Check bounds BEFORE accessing array
  if (morseStep >= MORSE_PATTERN_LENGTH) {
    morseActive = false;
    breathPhase = 0;
    fill_solid(leds, LED_COUNT, CRGB(0, 255, 255));
    FastLED.show();
    return;
  }

  int currentDuration = MORSE_PATTERN[morseStep];

  // Check for end marker
  if (currentDuration == 0) {
    morseActive = false;
    breathPhase = 0;
    fill_solid(leds, LED_COUNT, CRGB(0, 255, 255));
    FastLED.show();
    return;
  }

  unsigned long elapsed = millis() - morseStepStart;
  bool shouldBeOn = (currentDuration > 0);
  int durationMs = abs(currentDuration);

  if (elapsed >= (unsigned long)durationMs) {
    morseStep++;
    morseStepStart = millis();
    return;
  }

  CRGB color = (currentLedAnim == LED_MORSE_PURPLE) ? CRGB(128, 0, 128) : CRGB(255, 0, 0);
  fill_solid(leds, LED_COUNT, shouldBeOn ? color : CRGB::Black);
  FastLED.show();
}

void updateLedAnimations() {
  if (morseActive) {
    updateMorseLED();
  } else {
    updateBreathingLED();
  }
}

// ============================================================
// TOUCH HANDLERS
// ============================================================

void handleTouch() {
  // Friend buttons
  if (touchY >= BTN_Y && touchY <= BTN_Y + BTN_H) {
    int x = MARGIN;
    for (int i = 0; i < NUM_FRIENDS; i++) {
      if (touchX >= x && touchX <= x + BTN_W) {
        if (i == MY_FRIEND_INDEX) toggleCommit();
        return;
      }
      x += BTN_W + BTN_GAP;
    }

    x += 10;
    if (touchX >= x && touchX <= x + COMMIT_W) {
      toggleCommit();
      return;
    }
  }

  // Days selection
  if (touchY >= DAYS_Y && touchY <= DAYS_Y + DAY_H + 5) {
    int dayW = (PANEL_W - 10) / 7;
    int x = MARGIN + 5;

    for (int i = 0; i < 7; i++) {
      if (touchX >= x && touchX <= x + dayW) {
        const int dayMap[] = {6, 0, 1, 2, 3, 4, 5};
        int actualDay = dayMap[i];
        int daysFromToday = (actualDay - dayOfWeek + 7) % 7;
        selectDay(daysFromToday);
        return;
      }
      x += dayW;
    }
  }
}

void toggleCommit() {
  // Debounce: prevent rapid-fire commits (3 second cooldown)
  unsigned long now = millis();
  if (now - lastCommitTime < COMMIT_DEBOUNCE_MS) {
    Serial.printf("[TOUCH] Commit debounced (too soon, %lums since last)\n", now - lastCommitTime);
    return;
  }
  lastCommitTime = now;

  friends[MY_FRIEND_INDEX].committed = !friends[MY_FRIEND_INDEX].committed;
  drawButtons();

  String msg = friends[MY_FRIEND_INDEX].committed
    ? "🏂 " + String(friends[MY_FRIEND_INDEX].initials) + " is IN!"
    : "😢 " + String(friends[MY_FRIEND_INDEX].initials) + " is OUT";

  broadcast(msg);
  triggerScanner();

  if (friends[MY_FRIEND_INDEX].committed) {
    showCommitAnim = true;
    commitAnimStart = millis();
  }

  drawTimer();
}

// ============================================================
// DRAWING FUNCTIONS
// ============================================================

void showSplash() {
  gfx->fillScreen(COL_BLACK);
  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(4);
  gfx->setCursor(160, 180);
  gfx->print("FRIYAY FOREVER");
  gfx->setTextSize(2);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(290, 250);
  gfx->print("Protocol 1.0 v26");
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(320, 300);
  gfx->print("Unit: ");
  gfx->print(friends[MY_FRIEND_INDEX].initials);
  delay(SPLASH_DURATION_MS);
}

void drawUI() {
  gfx->fillScreen(COL_BLACK);
  drawButtons();
  drawNotificationBox();
  drawDays();
  drawWeatherPanel();
  drawWeatherBars();
  drawTimer();
  drawVUMeters();
  drawHeader();
  drawSpotifyArea();
}

// v26: New helper function to eliminate duplicate grid drawing code
void drawCyberpunkGrid(int x, int y, int w, int h) {
  for (int gx = x; gx <= x + w; gx += GRID_SPACING) {
    gfx->drawFastVLine(gx, y, h, COL_GRID);
  }
  for (int gy = y; gy <= y + h; gy += GRID_SPACING) {
    gfx->drawFastHLine(x, gy, w, COL_GRID);
  }
}

void drawButtons() {
  int x = MARGIN;

  for (int i = 0; i < NUM_FRIENDS; i++) {
    if (friends[i].committed) {
      gfx->fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, COL_YELLOW);
      gfx->setTextColor(COL_BLACK);
    } else {
      gfx->fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, COL_BLACK);
      gfx->drawRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, COL_YELLOW);
      gfx->setTextColor(COL_YELLOW);
    }

    gfx->setTextSize(2);
    int tw = strlen(friends[i].initials) * 12;
    gfx->setCursor(x + (BTN_W - tw) / 2, BTN_Y + 17);
    gfx->print(friends[i].initials);

    x += BTN_W + BTN_GAP;
  }

  x += 10;
  uint16_t commitBgColor = friends[MY_FRIEND_INDEX].committed ? COL_YELLOW : COL_CYAN;
  gfx->fillRoundRect(x, BTN_Y, COMMIT_W, BTN_H, 6, commitBgColor);
  gfx->setTextColor(COL_BLACK);
  gfx->setTextSize(2);
  gfx->setCursor(x + 8, BTN_Y + 17);
  gfx->print("Commit");
}

void drawNotificationBox() {
  gfx->fillRect(NOTIF_X - 2, NOTIF_Y - 2, NOTIF_W + 4, NOTIF_H + 4, COL_BLACK);
  gfx->drawRoundRect(NOTIF_X, NOTIF_Y, NOTIF_W, NOTIF_H, 6, COL_CYAN);
  gfx->fillRect(NOTIF_X + 2, NOTIF_Y + 2, NOTIF_W - 4, NOTIF_H - 4, 0x0011);

  if (scannerActive) {
    for (int g = 0; g < 5; g++) {
      uint16_t glowColor = gfx->color565(0, 80 - g * 15, 200 - g * 35);
      gfx->fillRect(NOTIF_X + scannerPos - 15 + g * 3, NOTIF_Y + 3,
                    12 - g * 2, NOTIF_H - 6, glowColor);
    }
    gfx->fillRect(NOTIF_X + scannerPos, NOTIF_Y + 3, 10, NOTIF_H - 6, COL_WHITE);
  }

  for (int gx = NOTIF_X + 15; gx < NOTIF_X + NOTIF_W - 5; gx += 20) {
    gfx->drawFastVLine(gx, NOTIF_Y + 3, NOTIF_H - 6, 0x0111);
  }
}

void drawDays() {
  const char* days[] = {"SAT", "SUN", "MON", "TUE", "WED", "THU", "FRI"};
  const int dayMap[] = {6, 0, 1, 2, 3, 4, 5};

  int dayW = (PANEL_W - 10) / 7;
  int x = MARGIN + 5;

  for (int i = 0; i < 7; i++) {
    int actualDay = dayMap[i];
    bool isToday = (actualDay == dayOfWeek);
    int daysFromToday = (actualDay - dayOfWeek + 7) % 7;
    bool isSelected = (selectedDay >= 0 && daysFromToday == selectedDay);
    int centerX = x + dayW / 2;

    if (isToday && (selectedDay < 0 || isSelected)) {
      gfx->fillRoundRect(x, DAYS_Y, dayW, DAY_H + 5, 6, COL_YELLOW);
      gfx->setTextColor(COL_BLACK);
    } else if (isSelected) {
      gfx->fillRoundRect(x, DAYS_Y, dayW, DAY_H + 5, 6, COL_CYAN);
      gfx->setTextColor(COL_BLACK);
    } else if (isToday) {
      gfx->drawRoundRect(x, DAYS_Y, dayW, DAY_H + 5, 6, COL_YELLOW);
      gfx->setTextColor(COL_YELLOW);
    } else {
      gfx->setTextColor(COL_WHITE);
    }

    gfx->setTextSize(2);
    gfx->setCursor(centerX - 18, DAYS_Y + 6);
    gfx->print(days[i]);

    x += dayW;
  }
}

void drawWeatherPanel() {
  gfx->drawRoundRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 8, COL_YELLOW);
}

void drawWeatherBars() {
  gfx->fillRect(PANEL_X + 3, PANEL_Y + 3, PANEL_W - 6, PANEL_H - 6, COL_BLACK);
  gfx->drawRoundRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 8, COL_YELLOW);

  const char* labels[] = {"WET", "TMP", "FUK"};
  int values[] = {wetLvl, tmpLvl, fukLvl};
  uint16_t labelColors[] = {COL_YELLOW, COL_CYAN, COL_YELLOW};
  char displays[3][10];

  snprintf(displays[0], sizeof(displays[0]), "%d%%", wetLvl * 10);
  snprintf(displays[1], sizeof(displays[1]), "%dF", (int)currTemp);
  snprintf(displays[2], sizeof(displays[2]), "%dHD", fukLvl * 10);

  int y = WEATHER_START_Y;
  int labelX = PANEL_X + 15;
  int barStartX = PANEL_X + 70;
  int valueX = PANEL_X + PANEL_W - 15;
  int barWidth = valueX - barStartX - 55;
  int blockW = (barWidth - BLOCK_GAP * 9) / 10;

  for (int r = 0; r < 3; r++) {
    gfx->setTextColor(labelColors[r]);
    gfx->setTextSize(2);
    gfx->setCursor(labelX, y + 6);
    gfx->print(labels[r]);

    for (int i = 0; i < 10; i++) {
      int bx = barStartX + i * (blockW + BLOCK_GAP);
      uint16_t col = (i < values[r]) ? COL_CYAN : COL_DARK_GRAY;
      gfx->fillRect(bx, y, blockW, BLOCK_SIZE, col);
    }

    gfx->setTextColor(COL_YELLOW);
    gfx->setTextSize(2);
    int tw = strlen(displays[r]) * 12;
    gfx->setCursor(valueX - tw, y + 6);
    gfx->print(displays[r]);

    y += WEATHER_ROW_GAP;
  }
}

void drawTimer() {
  gfx->fillRect(TIMER_X - 3, TIMER_Y - 3, TIMER_W + 6, TIMER_H + 6, COL_BLACK);

  uint16_t borderCol = (newMsg && (millis() - msgTime < MSG_HIGHLIGHT_TIME_MS)) ? COL_CYAN : COL_YELLOW;
  gfx->drawRoundRect(TIMER_X, TIMER_Y, TIMER_W, TIMER_H, 8, borderCol);
  if (newMsg && (millis() - msgTime < MSG_HIGHLIGHT_TIME_MS)) {
    gfx->drawRoundRect(TIMER_X + 1, TIMER_Y + 1, TIMER_W - 2, TIMER_H - 2, 7, borderCol);
  }

  if (showCommitAnim && (millis() - commitAnimStart > COMMIT_ANIM_DURATION)) {
    showCommitAnim = false;
  }

  int centerY = TIMER_Y + TIMER_H / 2;

  // Priority 1: Commit animation
  if (showCommitAnim) {
    gfx->setTextColor(COL_YELLOW);
    gfx->setTextSize(4);
    gfx->setCursor(TIMER_X + 80, centerY - 32);
    gfx->print("Cha Boi!");
    gfx->setTextSize(3);
    gfx->setTextColor(COL_CYAN);
    gfx->setCursor(TIMER_X + 150, centerY + 10);
    gfx->print("Lets Ride!");
  }
  // Priority 2: Message
  else if (showingMsg && currMsg.length() > 0) {
    if (millis() - msgTime > MSG_DISPLAY_TIME_MS) {
      showingMsg = false;
      currMsg = "";
      msgScrollPos = 0;
      newMsg = false;
      drawTimer();
      return;
    }

    gfx->setTextColor(COL_WHITE);
    gfx->setTextSize(5);
    int charWidth = 30;
    int textY = centerY - 20;
    int clipLeft = TIMER_X + 10;
    int clipRight = TIMER_X + TIMER_W - 10;

    if (currMsg.length() <= 12) {
      int tw = currMsg.length() * charWidth;
      int textX = TIMER_X + (TIMER_W - tw) / 2;
      if (textX < clipLeft) textX = clipLeft;
      gfx->setCursor(textX, textY);
      gfx->print(currMsg);
    } else {
      int textStartX = clipLeft + 10 - msgScrollPos;
      int firstVisibleChar = max(0, (clipLeft - textStartX) / charWidth);
      int visibleStartX = textStartX + firstVisibleChar * charWidth;
      int charsVisible = (clipRight - visibleStartX) / charWidth + 1;
      int lastVisibleChar = min((int)currMsg.length(), firstVisibleChar + charsVisible);

      if (firstVisibleChar < lastVisibleChar) {
        String visibleText = currMsg.substring(firstVisibleChar, lastVisibleChar);
        int drawX = max(clipLeft, visibleStartX);
        gfx->setCursor(drawX, textY);
        gfx->print(visibleText);
      }
    }
  }
  // Priority 3: Shutdown message
  else if (secToFri <= 0) {
    if (!zeroTriggered) {
      triggerMorseLED(LED_MORSE_RED);
      zeroTriggered = true;
    }
    gfx->setTextColor(COL_VU_GREEN);
    gfx->setTextSize(2);
    gfx->setCursor(TIMER_X + 80, TIMER_Y + 40);
    gfx->print("SHUT IT DOWN!");
    gfx->setCursor(TIMER_X + 40, TIMER_Y + 80);
    gfx->print("GO RIDE WITH YOUR BOYS!");
  }
  // Priority 4: Countdown
  else {
    if (secToFri > 60) zeroTriggered = false;

    char timeStr[15];
    snprintf(timeStr, sizeof(timeStr), "%03d:%02d:%02d", hrsLeft, minLeft, secLeft);
    gfx->setTextColor(COL_WHITE);
    gfx->setTextSize(7);
    int tw = strlen(timeStr) * 42;
    gfx->setCursor(TIMER_X + (TIMER_W - tw) / 2, centerY - 28);
    gfx->print(timeStr);
  }
}

void drawVUMeters() {
  drawMeter(VU_X, VU_TOP, VU_W, VU_H, aqiLvl, "AQI");
  drawMeter(VU_X + VU_W + VU_GAP, VU_TOP, VU_W, VU_H, co2Lvl, "CO2");
}

uint16_t getGradientColor(int segment, int maxSegments) {
  float ratio = (float)segment / (float)(maxSegments - 1);
  int r = 31 - (int)(31 * ratio);
  int g = 53 + (int)(10 * ratio);
  int b = (int)(31 * ratio);
  return (constrain(r, 0, 31) << 11) | (constrain(g, 0, 63) << 5) | constrain(b, 0, 31);
}

void drawMeter(int x, int y, int w, int h, int level, const char* label) {
  gfx->fillRect(x - 1, y - 1, w + 2, h + 2, COL_BLACK);

  uint16_t borderCol;
  if (level >= 8) borderCol = COL_CYAN;
  else if (level >= 5) borderCol = getGradientColor(level, 10);
  else borderCol = COL_YELLOW;

  gfx->drawRoundRect(x, y, w, h, 4, borderCol);

  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(1);
  for (size_t i = 0; i < strlen(label) && i < 3; i++) {
    gfx->setCursor(x + w/2 - 3, DAYS_Y + 4 + i * 11);
    char c[2] = {label[i], 0};
    gfx->print(c);
  }

  int pad = 4;
  int segCount = 10;
  int segGap = 3;
  int segH = (h - pad * 2 - (segCount - 1) * segGap) / segCount;

  for (int i = 0; i < segCount; i++) {
    int segY = y + h - pad - (i + 1) * (segH + segGap) + segGap;
    uint16_t segCol = (i < level) ? getGradientColor(i, segCount) : COL_DARK_GRAY;
    gfx->fillRect(x + pad, segY, w - pad * 2, segH, segCol);
  }
}

void drawHeader() {
  int hx = VU_X + VU_TOTAL_W + 20;
  gfx->fillRect(hx - 5, HEADER_Y - 15, 250, 35, COL_BLACK);

  drawWifiIcon(hx, HEADER_Y);
  drawProfileIcon(hx + 45, HEADER_Y);

  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(hx + 75, HEADER_Y - 8);
  gfx->print("Friyay//1.0");
}

void drawWifiIcon(int x, int y) {
  int barW = 4, barGap = 3, baseY = y + 10;
  for (int i = 0; i < 4; i++) {
    int barH = 6 + i * 5;
    uint16_t col = (i < wifiStrength) ? COL_CYAN : COL_DARK_GRAY;
    gfx->fillRect(x + i * (barW + barGap), baseY - barH, barW, barH, col);
  }
}

void drawProfileIcon(int x, int y) {
  gfx->drawCircle(x, y, 10, COL_CYAN);
  gfx->fillCircle(x, y - 3, 4, COL_CYAN);
  gfx->fillRect(x - 5, y + 5, 10, 3, COL_CYAN);
}

void drawSpotifyArea() {
  spotifySenderInitials = "";

  // Header
  gfx->fillRoundRect(ART_X, SPOT_TOP, ALBUM_ART_W, SPOT_HEADER_H, 8, COL_CYAN);
  gfx->fillRect(ART_X, SPOT_TOP + SPOT_HEADER_H - 8, ALBUM_ART_W, 8, COL_CYAN);
  gfx->setTextColor(COL_BLACK);
  gfx->setTextSize(3);
  gfx->setCursor(ART_X + 55, SPOT_TOP + 12);
  gfx->print("LISTEN");
  gfx->setTextSize(1);
  gfx->setCursor(ART_X + ALBUM_ART_W - 25, SPOT_TOP + 18);
  gfx->print("</>");

  // Content area with grid
  gfx->fillRect(ART_X, ART_AREA_Y, ALBUM_ART_W, ALBUM_ART_H, COL_SPOTIFY_BG);
  drawCyberpunkGrid(ART_X, ART_AREA_Y, ALBUM_ART_W, ALBUM_ART_H);

  gfx->setTextColor(COL_CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(ART_X + 55, ART_AREA_Y + ALBUM_ART_H / 2 - 10);
  gfx->print("Send Tunes");
}

void drawSenderBadge() {
  if (spotifySenderInitials.length() == 0) return;

  const int badgeW = 50, badgeH = 38;
  const int badgeX = ART_X + ALBUM_ART_W - badgeW - 8;
  const int badgeY = ART_AREA_Y - 47;

  gfx->fillRoundRect(badgeX, badgeY, badgeW, badgeH, 6, 0x2104);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  int textWidth = spotifySenderInitials.length() * 12;
  gfx->setCursor(badgeX + (badgeW - textWidth) / 2, badgeY + 11);
  gfx->print(spotifySenderInitials);
}

void displayQRPlaceholder() {
  hasSpotify = false;
  trackId = "";
  albumArtUrl = "";
  spotifyCodeUrl = "";
  spotifySenderInitials = "";

  gfx->fillRect(ART_X, ART_AREA_Y, ALBUM_ART_W, ALBUM_ART_H, COL_SPOTIFY_BG);
  drawCyberpunkGrid(ART_X, ART_AREA_Y, ALBUM_ART_W, ALBUM_ART_H);

  if (jpeg.openRAM((uint8_t*)qr_code_data, qr_code_len, jpegDrawCallbackQR)) {
    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
    if (jpeg.decode(0, 0, 0)) {
      showingQRCode = true;
    }
    jpeg.close();
  }

  gfx->setTextColor(COL_CYAN);
  gfx->setTextSize(3);
  gfx->setCursor(ART_X + 35, ART_AREA_Y + 220);
  gfx->print("Send Tunes");
}

// ============================================================
// JPEG CALLBACKS
// ============================================================

int jpegDrawCallback(JPEGDRAW *pDraw) {
  if (pDraw->x >= ALBUM_ART_W || pDraw->y >= ALBUM_ART_DISPLAY_H) return 1;
  gfx->draw16bitRGBBitmap(ART_X + pDraw->x, ART_AREA_Y + pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

int jpegDrawCallbackQR(JPEGDRAW *pDraw) {
  if (pDraw->x >= 180) return 1;
  gfx->draw16bitRGBBitmap(ART_X + QR_OFFSET_X + pDraw->x, ART_AREA_Y + QR_OFFSET_Y + pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

int jpegDrawCallbackCode(JPEGDRAW *pDraw) {
  if (pDraw->x >= ALBUM_ART_W || pDraw->y >= 70) return 1;
  gfx->draw16bitRGBBitmap(ART_X + pDraw->x - 17, ART_AREA_Y + 225 + pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

// ============================================================
// WIFI SETUP
// ============================================================

void startWiFiSetup() {
  inSetup = true;

  gfx->fillScreen(COL_BLACK);
  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(3);
  gfx->setCursor(200, 220);
  gfx->print("Scanning WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  netCount = min((int)WiFi.scanNetworks(), MAX_WIFI_NETWORKS);

  for (int i = 0; i < netCount; i++) {
    networks[i] = WiFi.SSID(i);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("FRIYAY-Setup");

  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.onNotFound(handleRoot);
  server.begin();

  drawNetList();
}

void drawNetList() {
  gfx->fillScreen(COL_BLACK);

  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(3);
  gfx->setCursor(280, 15);
  gfx->print("WiFi Setup");

  gfx->setTextSize(2);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(100, 55);
  gfx->print("Tap network, enter password, connect");

  int startY = 170, rowHeight = 60;

  for (int i = 0; i < netCount; i++) {
    int y = startY + (i * rowHeight);

    if (i == selNetwork) {
      gfx->fillRoundRect(30, y, 420, 50, 5, COL_YELLOW);
      gfx->setTextColor(COL_BLACK);
    } else {
      gfx->drawRoundRect(30, y, 420, 50, 5, COL_GRAY);
      gfx->setTextColor(COL_WHITE);
    }

    gfx->setTextSize(2);
    gfx->setCursor(45, y + 17);
    gfx->print(networks[i]);
  }

  // Password field
  gfx->drawRoundRect(30, 420, 420, 45, 5, COL_YELLOW);
  gfx->setTextColor(kbInput.length() > 0 ? COL_WHITE : COL_GRAY);
  gfx->setTextSize(2);
  gfx->setCursor(45, 432);
  if (kbInput.length() > 0) {
    String stars = "";
    for (unsigned int i = 0; i < kbInput.length(); i++) stars += "*";
    gfx->print(stars);
  } else {
    gfx->print("Password...");
  }

  // Buttons
  gfx->fillRoundRect(470, 420, 80, 45, 5, COL_CYAN);
  gfx->setTextColor(COL_BLACK);
  gfx->setCursor(490, 432);
  gfx->print("ABC");

  bool canConnect = (selNetwork >= 0 && kbInput.length() > 0);
  if (canConnect) {
    gfx->fillRoundRect(570, 420, 120, 45, 5, COL_VU_GREEN);
    gfx->setTextColor(COL_BLACK);
  } else {
    gfx->drawRoundRect(570, 420, 120, 45, 5, COL_GRAY);
    gfx->setTextColor(COL_GRAY);
  }
  gfx->setCursor(590, 432);
  gfx->print("Connect");

  gfx->fillRoundRect(700, 420, 80, 45, 5, COL_ORANGE);
  gfx->setTextColor(COL_BLACK);
  gfx->setCursor(715, 432);
  gfx->print("Scan");
}

void drawKeyboard() {
  kbVisible = true;

  gfx->fillRect(0, 140, 800, 340, COL_DARK_GRAY);
  gfx->drawRect(0, 140, 800, 340, COL_YELLOW);

  gfx->fillRect(50, 150, 700, 40, COL_BLACK);
  gfx->drawRect(50, 150, 700, 40, COL_CYAN);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(60, 160);
  gfx->print(kbInput);
  gfx->print("_");

  const char* rows[] = {"!@#$%^&*()", "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int rowY[] = {200, 245, 290, 335, 380};
  int rowX[] = {35, 35, 35, 70, 115};
  int keyW = 68, keyH = 40;

  for (int r = 0; r < 5; r++) {
    int x = rowX[r];
    for (int k = 0; rows[r][k]; k++) {
      char c = rows[r][k];
      if (!capsOn && c >= 'A' && c <= 'Z') c += 32;

      gfx->fillRoundRect(x, rowY[r], keyW - 4, keyH - 4, 4, COL_GRAY);
      gfx->setTextColor(COL_WHITE);
      gfx->setTextSize(2);
      gfx->setCursor(x + 24, rowY[r] + 10);
      char str[2] = {c, 0};
      gfx->print(str);
      x += keyW;
    }
  }

  gfx->fillRoundRect(500, 380, 90, keyH - 4, 4, capsOn ? COL_YELLOW : COL_GRAY);
  gfx->setTextColor(capsOn ? COL_BLACK : COL_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(515, 390);
  gfx->print("CAPS");

  gfx->fillRoundRect(35, 430, 450, keyH - 4, 4, COL_GRAY);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(200, 440);
  gfx->print("SPACE");

  gfx->fillRoundRect(500, 430, 90, keyH - 4, 4, COL_ORANGE);
  gfx->setTextColor(COL_BLACK);
  gfx->setCursor(525, 440);
  gfx->print("DEL");

  gfx->fillRoundRect(605, 430, 90, keyH - 4, 4, COL_VU_GREEN);
  gfx->setTextColor(COL_BLACK);
  gfx->setCursor(620, 440);
  gfx->print("DONE");

  gfx->fillRoundRect(700, 430, 60, keyH - 4, 4, COL_GRAY);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(722, 438);
  gfx->print(".");
}

void handleKBTouch() {
  if (touchX >= 605 && touchX <= 695 && touchY >= 430 && touchY <= 466) {
    kbVisible = false;
    drawNetList();
    return;
  }

  if (touchX >= 500 && touchX <= 590 && touchY >= 430 && touchY <= 466) {
    if (kbInput.length() > 0) {
      kbInput.remove(kbInput.length() - 1);
      drawKeyboard();
    }
    return;
  }

  if (touchX >= 500 && touchX <= 590 && touchY >= 380 && touchY <= 416) {
    capsOn = !capsOn;
    drawKeyboard();
    return;
  }

  if (touchX >= 35 && touchX <= 485 && touchY >= 430 && touchY <= 466) {
    kbInput += " ";
    drawKeyboard();
    return;
  }

  if (touchX >= 700 && touchX <= 760 && touchY >= 430 && touchY <= 466) {
    kbInput += ".";
    drawKeyboard();
    return;
  }

  const char* rows[] = {"!@#$%^&*()", "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int rowY[] = {200, 245, 290, 335, 380};
  int rowX[] = {35, 35, 35, 70, 115};
  int keyW = 68;

  for (int r = 0; r < 5; r++) {
    if (touchY >= rowY[r] && touchY < rowY[r] + 40) {
      int kx = touchX - rowX[r];
      if (kx >= 0) {
        int k = kx / keyW;
        int len = strlen(rows[r]);
        if (k < len) {
          char c = rows[r][k];
          if (!capsOn && c >= 'A' && c <= 'Z') c += 32;
          kbInput += c;
          drawKeyboard();
          return;
        }
      }
    }
  }
}

void handleSetupTouch() {
  if (kbVisible) {
    handleKBTouch();
    return;
  }

  int startY = 170, rowHeight = 60;
  int listEndY = startY + (MAX_WIFI_NETWORKS * rowHeight);

  if (touchX >= 30 && touchX <= 450 && touchY >= startY && touchY < listEndY) {
    int idx = (touchY - startY) / rowHeight;
    if (idx < netCount) {
      selNetwork = idx;
      drawNetList();
    }
  }

  if (touchY >= 420 && touchY <= 465) {
    if ((touchX >= 30 && touchX <= 450) || (touchX >= 470 && touchX <= 550)) {
      drawKeyboard();
    }
    else if (touchX >= 570 && touchX <= 690 && selNetwork >= 0 && kbInput.length() > 0) {
      doConnect();
    }
    else if (touchX >= 700 && touchX <= 780) {
      startWiFiSetup();
    }
  }
}

void doConnect() {
  gfx->fillScreen(COL_BLACK);
  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(3);
  gfx->setCursor(200, 220);
  gfx->print("Connecting...");

  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);

  WiFi.mode(WIFI_STA);
  WiFi.begin(networks[selNetwork].c_str(), kbInput.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 15) {
    delay(400);
    gfx->print(".");
    tries++;
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    prefs.putString("ssid", networks[selNetwork]);
    prefs.putString("pass", kbInput);

    gfx->fillScreen(COL_BLACK);
    gfx->setTextColor(COL_VU_GREEN);
    gfx->setCursor(250, 200);
    gfx->print("Connected!");
    gfx->setTextSize(2);
    gfx->setCursor(250, 260);
    gfx->print("IP: ");
    gfx->print(WiFi.localIP());
    delay(1000);

    wifiOK = true;
    inSetup = false;
    kbInput = "";
    selNetwork = -1;

    configTime(-5 * 3600, 3600, "pool.ntp.org");
    int t = 0;
    while (!getLocalTime(&tinfo) && t < 8) { delay(500); t++; yield(); }

    client.setInsecure();
    client.setTimeout(1500);
    getWeather();
    wifiStrength = calculateWifiStrength(WiFi.RSSI());

    drawUI();
    displayQRPlaceholder();
  } else {
    gfx->fillScreen(COL_BLACK);
    gfx->setTextColor(COL_RED);
    gfx->setCursor(200, 220);
    gfx->print("Failed!");
    delay(1000);
    kbInput = "";
    startWiFiSetup();
  }
}

void tryConnect() {
  gfx->fillScreen(COL_BLACK);
  gfx->setTextColor(COL_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(200, 220);
  gfx->print("Connecting to ");
  gfx->print(savedSSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    gfx->print(".");
    tries++;
    yield();
  }

  wifiOK = (WiFi.status() == WL_CONNECTED);
}

void handleRoot() {
  server.send(200, "text/html",
    "<html><body style='text-align:center;font-family:sans-serif;padding:40px'>"
    "<h1 style='color:#FFD700'>FRIYAY FOREVER</h1>"
    "<p>Use the touch screen to connect to WiFi</p></body></html>");
}

// ============================================================
// TELEGRAM
// ============================================================

void checkTelegram() {
  int n = bot.getUpdates(bot.last_message_received + 1);

  for (int i = 0; i < n; i++) {
    String chatId = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from = bot.messages[i].from_name;
    int64_t senderId = strtoll(bot.messages[i].chat_id.c_str(), NULL, 10);

    int fIdx = getFriendIdx(senderId);

    if (fIdx >= 0) {
      String t = text;
      t.toLowerCase();

      if (t.indexOf("/commit") >= 0 || t == "in" || t == "commit" || t == "riding") {
        friends[fIdx].committed = true;
        broadcast("🏂 " + String(friends[fIdx].initials) + " is IN!");
        drawButtons();
        triggerScanner();
        continue;
      }

      if (t.indexOf("/uncommit") >= 0 || t == "out" || t == "bail") {
        friends[fIdx].committed = false;
        broadcast("😢 " + String(friends[fIdx].initials) + " is OUT");
        drawButtons();
        triggerScanner();
        continue;
      }
    }

    if (text.indexOf("spotify.com") >= 0 || text.indexOf("open.spotify") >= 0) {
      if (fIdx >= 0) spotifySenderInitials = String(friends[fIdx].initials);
      parseSpotify(text);
      showMessage(from + " shared music!");
      continue;
    }

    if (text == "/start" || text == "/help") {
      String help = "🏂 FRIYAY FOREVER\n\n";
      help += "/commit - You're in!\n";
      help += "/uncommit - Can't make it\n";
      help += "/status - Who's riding\n";
      help += "/weather - Conditions\n\n";
      help += "📱 System:\n";
      help += "/version - Firmware info\n";
      help += "/update - Check for updates\n";
      help += "/install - Install update\n\n";
      help += "Or just say 'in' or 'out'";
      bot.sendMessage(chatId, help, "");
      continue;
    }

    if (text == "/status") {
      String s = "📊 Status:\n\n";
      for (int j = 0; j < NUM_FRIENDS; j++) {
        s += friends[j].committed ? "✅ " : "⬜ ";
        s += friends[j].initials;
        s += "\n";
      }
      s += "\n⏱️ " + String(hrsLeft) + "h " + String(minLeft) + "m to Friday";
      bot.sendMessage(chatId, s, "");
      continue;
    }

    if (text == "/weather") {
      String w = "🌤️ Chapel Hill\n\n🌡️ " + String((int)currTemp) + "°F\n💧 " + String(precipitation, 1) + "mm\n🏂 Score: " + String(fukLvl * 10) + "/100";
      bot.sendMessage(chatId, w, "");
      continue;
    }

    // OTA Update Commands
    if (text == "/version") {
      String v = "📱 Firmware Info\n\n";
      v += "Version: v" + otaUpdater.getCurrentVersion() + "\n";
      v += "Board: ESP32-8048S043C\n";
      v += "Unit: " + String(friends[MY_FRIEND_INDEX].initials) + "\n";
      v += "WiFi: " + WiFi.SSID() + "\n";
      v += "IP: " + WiFi.localIP().toString();
      bot.sendMessage(chatId, v, "");
      continue;
    }

    if (text == "/update") {
      bot.sendMessage(chatId, "🔄 Checking for firmware updates...", "");

      if (otaUpdater.checkForUpdate()) {
        String msg = "✅ Update available!\n\n";
        msg += "Current: v" + otaUpdater.getCurrentVersion() + "\n";
        msg += "Latest: v" + otaUpdater.getLatestVersion() + "\n";
        if (otaUpdater.getReleaseNotes().length() > 0) {
          msg += "\n📝 " + otaUpdater.getReleaseNotes() + "\n";
        }
        if (otaUpdater.isCriticalUpdate()) {
          msg += "\n⚠️ CRITICAL UPDATE\n";
        }
        msg += "\nSend /install to update now";
        bot.sendMessage(chatId, msg, "");
      } else {
        String msg = "✅ You're up to date!\n\n";
        msg += "Version: v" + otaUpdater.getCurrentVersion();
        if (otaUpdater.getLastError().length() > 0) {
          msg += "\n\n⚠️ " + otaUpdater.getLastError();
        }
        bot.sendMessage(chatId, msg, "");
      }
      continue;
    }

    if (text == "/install") {
      if (!otaUpdater.isUpdateAvailable()) {
        // Check again in case they haven't run /update recently
        if (!otaUpdater.checkForUpdate()) {
          bot.sendMessage(chatId, "ℹ️ No update available.\n\nYou're running v" + otaUpdater.getCurrentVersion(), "");
          continue;
        }
      }

      // Notify everyone that this unit is updating
      String initials = String(friends[MY_FRIEND_INDEX].initials);
      broadcast("⚙️ " + initials + "'s unit is updating to v" + otaUpdater.getLatestVersion() + "...");

      bot.sendMessage(chatId, "🚀 Installing update...\n\nDevice will reboot when complete!", "");

      // Small delay to ensure message is sent
      delay(1000);

      // Set flag and perform update
      otaInProgress = true;
      otaUpdater.setProgressCallback(otaProgressCallback);

      if (!otaUpdater.performUpdate()) {
        // Update failed (didn't reboot)
        otaInProgress = false;
        String errMsg = "❌ Update failed!\n\n";
        errMsg += otaUpdater.getLastError();
        bot.sendMessage(chatId, errMsg, "");
        drawTimer();  // Restore timer display
      }
      // If successful, device will have rebooted
      continue;
    }

    if (fIdx >= 0) {
      showMessage(String(friends[fIdx].initials) + ": " + text);
    }
  }
}

void showMessage(String msg) {
  currMsg = sanitizeMessage(msg);
  showingMsg = true;
  newMsg = true;
  msgTime = millis();
  msgScrollPos = 0;
  triggerScanner();
  drawTimer();
}

int getFriendIdx(int64_t id) {
  for (int i = 0; i < NUM_FRIENDS; i++) {
    if (friends[i].telegramId == id) return i;
  }
  return -1;
}

void broadcast(String msg) {
  for (int i = 0; i < NUM_FRIENDS; i++) {
    if (friends[i].telegramId != 0) {
      bot.sendMessage(String(friends[i].telegramId), msg, "");
    }
  }
}

String sanitizeMessage(String msg) {
  String cleaned = "";
  for (unsigned int i = 0; i < msg.length(); i++) {
    char c = msg.charAt(i);
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\r') {
      cleaned += c;
    }
  }
  return cleaned;
}

void parseSpotify(String text) {
  int idx = text.indexOf("/track/");
  if (idx >= 0) {
    int start = idx + 7;
    int end = text.indexOf("?", start);
    if (end < 0) end = min((int)text.length(), start + 22);
    trackId = text.substring(start, end);
    hasSpotify = true;
    showingQRCode = false;
    fetchSpotifyArt();
  }
}

void fetchSpotifyArt() {
  if (trackId.length() == 0 || WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://open.spotify.com/oembed?url=https://open.spotify.com/track/" + trackId;
  http.begin(url);
  http.setTimeout(5000);

  if (http.GET() == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    if (!deserializeJson(doc, payload)) {
      const char* thumbUrl = doc["thumbnail_url"];
      if (thumbUrl) {
        albumArtUrl = String(thumbUrl);
        if (albumArtUrl.indexOf("ab67616d0000b273") >= 0) {
          albumArtUrl.replace("ab67616d0000b273", "ab67616d00001e02");
        }
        downloadAndDisplayImage();
      }
    }
  }
  http.end();
}

// ============================================================
// IMAGE DOWNLOADING
// ============================================================

uint8_t* downloadImageFromUrl(String url, int* outLen) {
  *outLen = 0;
  if (WiFi.status() != WL_CONNECTED) return nullptr;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);

  if (http.GET() != 200) {
    http.end();
    return nullptr;
  }

  int len = http.getSize();
  if (len <= 0 || len > 300000) {
    http.end();
    return nullptr;
  }

  uint8_t *buffer = (uint8_t*)ps_malloc(len);
  if (!buffer) {
    http.end();
    return nullptr;
  }

  WiFiClient *stream = http.getStreamPtr();
  int bytesRead = 0;
  while (bytesRead < len && stream->connected()) {
    int toRead = min(1024, len - bytesRead);
    int thisRead = stream->readBytes(buffer + bytesRead, toRead);
    if (thisRead > 0) bytesRead += thisRead;
    else delay(10);
  }
  http.end();

  if (bytesRead != len) {
    free(buffer);
    return nullptr;
  }

  *outLen = len;
  return buffer;
}

void downloadAndDisplayImage() {
  if (albumArtUrl.length() == 0) return;

  int len = 0;
  uint8_t *buffer = downloadImageFromUrl(albumArtUrl, &len);
  if (buffer) {
    decodeAndDisplayJpeg(buffer, len);
    free(buffer);
  }
}

void decodeAndDisplayJpeg(uint8_t *buffer, int size) {
  // Clear album art area before drawing
  gfx->fillRect(ART_X, ART_AREA_Y, ALBUM_ART_W, ALBUM_ART_DISPLAY_H, COL_SPOTIFY_BG);

  if (jpeg.openRAM(buffer, size, jpegDrawCallback)) {
    // Get original dimensions
    int imgWidth = jpeg.getWidth();
    int imgHeight = jpeg.getHeight();

    // Use 1:1 scale for 300px images - clip to fill container
    int scale = 0;
    int scaledWidth = imgWidth;
    int scaledHeight = imgHeight;

    // Center the image - negative offset means overflow gets clipped
    int offsetX = (ALBUM_ART_W - scaledWidth) / 2;
    int offsetY = (ALBUM_ART_DISPLAY_H - scaledHeight) / 2;

    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);

    if (jpeg.decode(offsetX, offsetY, scale)) {
      drawSenderBadge();
      // Fetch and display Spotify code after successful album art decode
      if (trackId.length() > 0) getSpotifyCode();
    }
    jpeg.close();
  }
}

void getSpotifyCode() {
  if (trackId.length() == 0) return;
  spotifyCodeUrl = "https://scannables.scdn.co/uri/plain/jpeg/000000/white/500/spotify:track:" + trackId;
  downloadAndDisplayCode();
}

void downloadAndDisplayCode() {
  if (spotifyCodeUrl.length() == 0) return;

  int len = 0;
  uint8_t *buffer = downloadImageFromUrl(spotifyCodeUrl, &len);
  if (buffer) {
    decodeAndDisplayCode(buffer, len);
    free(buffer);
  }
}

void decodeAndDisplayCode(uint8_t *buffer, int size) {
  int codeY = ART_AREA_Y + 215;
  gfx->fillRect(ART_X - 1, codeY, ALBUM_ART_W + 2, 80, COL_BLACK);

  if (jpeg.openRAM(buffer, size, jpegDrawCallbackCode)) {
    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
    jpeg.decode(0, 0, JPEG_SCALE_HALF);
    jpeg.close();
  }
}

// ============================================================
// WEATHER & SENSORS
// ============================================================

void getWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  char url[400];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,precipitation"
    "&daily=temperature_2m_max,precipitation_sum"
    "&temperature_unit=fahrenheit&timezone=America/New_York"
    "&forecast_days=7",
    LATITUDE, LONGITUDE);

  http.begin(url);
  http.setTimeout(10000);

  if (http.GET() == 200) {
    String resp = http.getString();
    DynamicJsonDocument doc(4096);

    if (!deserializeJson(doc, resp)) {
      currTemp = doc["current"]["temperature_2m"];
      precipitation = doc["current"]["precipitation"];
      weatherOK = true;

      JsonArray highTemps = doc["daily"]["temperature_2m_max"];
      JsonArray rainAmounts = doc["daily"]["precipitation_sum"];

      for (int i = 0; i < 7 && i < (int)highTemps.size(); i++) {
        forecastHighTemp[i] = highTemps[i];
        forecastRain[i] = rainAmounts[i];
      }
      forecastLoaded = true;
      calcWeather();
    }
  }
  http.end();
}

void calcWeather() {
  float rainInches = precipitation / 25.4;
  wetLvl = constrain((int)(rainInches * 5), 0, 10);

  if (currTemp <= 32) tmpLvl = 0;
  else if (currTemp >= 100) tmpLvl = 10;
  else tmpLvl = constrain((int)((currTemp - 32) / 6.8), 0, 10);

  float tempDiff = abs(currTemp - 65);
  int tempScore = (currTemp < 32 || currTemp > 100) ? 0 : constrain(10 - (int)(tempDiff / 5), 0, 10);
  int rainPenalty = constrain((int)(rainInches * 5), 0, 10);
  fukLvl = constrain(tempScore - rainPenalty, 0, 10);
}

void selectDay(int dayIndex) {
  selectedDay = (dayIndex < 0 || dayIndex > 6) ? -1 : dayIndex;
  lastDaySelectTime = (selectedDay >= 0) ? millis() : 0;

  gfx->fillRect(MARGIN, DAYS_Y - 2, PANEL_W, DAY_H + 10, COL_BLACK);
  drawDays();

  if (selectedDay < 0 || !forecastLoaded) calcWeather();
  else calcWeatherForDay(selectedDay);

  drawWeatherBars();
}

void calcWeatherForDay(int dayIndex) {
  if (dayIndex < 0 || dayIndex >= 7 || !forecastLoaded) {
    calcWeather();
    return;
  }

  float temp = forecastHighTemp[dayIndex];
  float rain = forecastRain[dayIndex];
  float rainInches = rain / 25.4;

  wetLvl = constrain((int)(rainInches * 5), 0, 10);

  if (temp <= 32) tmpLvl = 0;
  else if (temp >= 100) tmpLvl = 10;
  else tmpLvl = constrain((int)((temp - 32) / 6.8), 0, 10);

  float tempDiff = abs(temp - 65);
  int tempScore = (temp < 32 || temp > 100) ? 0 : constrain(10 - (int)(tempDiff / 5), 0, 10);
  int rainPenalty = constrain((int)(rainInches * 5), 0, 10);
  fukLvl = constrain(tempScore - rainPenalty, 0, 10);

  currTemp = temp;
  precipitation = rain;
}

int calculateWifiStrength(int rssi) {
  if (rssi >= -50) return 4;
  if (rssi >= -60) return 3;
  if (rssi >= -70) return 2;
  return 1;
}

void readSensors() {
  // v26 FIX: Check if ADS1115 is available
  if (!adsOK) {
    // Fallback to direct analog read
    int raw = analogRead(MQ135_PIN);
    aqiLvl = map(constrain(raw, 0, 4095), 0, 4095, 10, 0);
    co2Lvl = aqiLvl;
    return;
  }

  int16_t rawADC = ads.readADC_SingleEnded(0);
  aqiLvl = map(constrain(rawADC, 0, 20000), 0, 20000, 10, 0);
  aqiLvl = constrain(aqiLvl, 0, 10);
  co2Lvl = aqiLvl;
}

// ============================================================
// TIME & RESET
// ============================================================

void calcCountdown() {
  time_t now = time(nullptr);
  struct tm fri = tinfo;

  int days = (5 - dayOfWeek + 7) % 7;
  if (days == 0 && tinfo.tm_hour >= 15) days = 7;

  fri.tm_mday += days;
  fri.tm_hour = 15;
  fri.tm_min = 0;
  fri.tm_sec = 0;

  secToFri = difftime(mktime(&fri), now);
  if (secToFri < 0) secToFri = 0;

  hrsLeft = secToFri / 3600;
  minLeft = (secToFri % 3600) / 60;
  secLeft = secToFri % 60;
}

void checkReset() {
  if (dayOfWeek == 5 && tinfo.tm_hour == 16 && tinfo.tm_min == 0 && tinfo.tm_sec < 2) {
    for (int i = 0; i < NUM_FRIENDS; i++) {
      friends[i].committed = false;
    }
    broadcast("🔄 Reset! See you next Friday 🏂");
    drawButtons();
  }
}

void checkQRReminder() {
  unsigned long now = millis();
  if (now - lastQRCheck < 60000) return;
  lastQRCheck = now;

  if (tinfo.tm_hour == 0 && tinfo.tm_min == 0 && tinfo.tm_sec < 2) {
    displayQRPlaceholder();
    broadcast("📱 Don't forget to share your tunes!");
    if (selectedDay >= 0) selectDay(-1);
  }
}

// ============================================================
// OTA UPDATE FUNCTIONS
// ============================================================

void otaProgressCallback(int progress) {
  // Clear and redraw the timer area with update progress
  gfx->fillRect(TIMER_X, TIMER_Y, TIMER_W, TIMER_H, COL_BLACK);
  gfx->drawRoundRect(TIMER_X, TIMER_Y, TIMER_W, TIMER_H, 8, COL_CYAN);

  int centerY = TIMER_Y + TIMER_H / 2;

  // Title
  gfx->setTextColor(COL_CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(TIMER_X + 80, centerY - 45);
  gfx->print("UPDATING FIRMWARE");

  // Progress bar
  int barW = TIMER_W - 60;
  int barH = 30;
  int barX = TIMER_X + 30;
  int barY = centerY - 10;

  // Bar outline
  gfx->drawRect(barX, barY, barW, barH, COL_YELLOW);

  // Filled portion
  int fillW = ((barW - 4) * progress) / 100;
  if (fillW > 0) {
    gfx->fillRect(barX + 2, barY + 2, fillW, barH - 4, COL_VU_GREEN);
  }

  // Percentage text
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  String pctStr = String(progress) + "%";
  int tw = pctStr.length() * 18;
  gfx->setCursor(TIMER_X + (TIMER_W - tw) / 2, centerY + 30);
  gfx->print(pctStr);

  // Keep LED breathing during update
  updateBreathingLED();
}

void checkForOTAUpdates() {
  Serial.println("[OTA] Performing scheduled update check...");

  if (otaUpdater.checkForUpdate()) {
    Serial.printf("[OTA] Update available: %s -> %s\n",
                 otaUpdater.getCurrentVersion().c_str(),
                 otaUpdater.getLatestVersion().c_str());

    // Notify all friends about available update
    String msg = "📢 Firmware update available!\n\n";
    msg += "Current: v" + otaUpdater.getCurrentVersion() + "\n";
    msg += "Latest: v" + otaUpdater.getLatestVersion() + "\n\n";
    msg += "Send /update for details";
    broadcast(msg);
  } else {
    Serial.println("[OTA] No update available or check failed");
    if (otaUpdater.getLastError().length() > 0) {
      Serial.printf("[OTA] Error: %s\n", otaUpdater.getLastError().c_str());
    }
  }
}
