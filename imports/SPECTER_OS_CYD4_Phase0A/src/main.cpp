#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "board_config.h"

namespace {
TFT_eSPI tft;
Preferences prefs;
constexpr uint16_t kBg      = TFT_BLACK;
constexpr uint16_t kCyan    = TFT_CYAN;
constexpr uint16_t kMagenta = TFT_MAGENTA;
constexpr uint16_t kWhite   = TFT_WHITE;
constexpr uint16_t kDim     = 0x4208;
constexpr uint16_t kGreen   = TFT_GREEN;
constexpr uint16_t kYellow  = TFT_YELLOW;
uint16_t touchCal[5] = {0, 0, 0, 0, 0};
bool touchReady = false;
uint32_t lastStatsMs = 0;

void drawFrame() {
  tft.fillScreen(kBg);
  tft.drawRect(4, 4, tft.width() - 8, tft.height() - 8, kCyan);
  tft.drawFastHLine(18, 42, tft.width() - 36, kDim);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", 18, 14, 4);
  tft.setTextColor(kWhite, kBg);
  tft.drawString("CYD4 HARDWARE TEST", 205, 18, 2);
  tft.setTextColor(kDim, kBg);
  tft.drawString("ST7796S | 480x320 LANDSCAPE | XPT2046", 18, 51, 2);

  const int left = 18;
  const int top = 82;
  const int gap = 10;
  const int w = (tft.width() - 36 - gap) / 2;
  const int h = 86;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      const int x = left + col * (w + gap);
      const int y = top + row * (h + gap);
      tft.drawRoundRect(x, y, w, h, 7, kDim);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kWhite, kBg);
      String label = "TOUCH " + String(row * 2 + col + 1);
      tft.drawString(label, x + w / 2, y + h / 2, 2);
    }
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kDim, kBg);
  tft.drawString("Tap quadrants. Marker + coordinates should follow your finger.", 18, 286, 2);
}

void drawStatusLine(const String& text, uint16_t color = kCyan) {
  tft.fillRect(18, 265, tft.width() - 36, 17, kBg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, kBg);
  tft.drawString(text, 18, 265, 2);
}

bool loadCalibration() {
  prefs.begin("specter", true);
  const size_t n = prefs.getBytesLength("touchcal");
  if (n == sizeof(touchCal)) {
    const size_t read = prefs.getBytes("touchcal", touchCal, sizeof(touchCal));
    prefs.end();
    return read == sizeof(touchCal);
  }
  prefs.end();
  return false;
}

bool saveCalibration() {
  prefs.begin("specter", false);
  const size_t written = prefs.putBytes("touchcal", touchCal, sizeof(touchCal));
  prefs.end();
  return written == sizeof(touchCal);
}

void calibrateTouch() {
  tft.fillScreen(kBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", tft.width() / 2, 42, 4);
  tft.setTextColor(kWhite, kBg);
  tft.drawString("TOUCH CALIBRATION", tft.width() / 2, 82, 4);
  tft.setTextColor(kDim, kBg);
  tft.drawString("Touch the highlighted corners when prompted.", tft.width() / 2, 122, 2);
  delay(800);

  tft.calibrateTouch(touchCal, kMagenta, kBg, 15);
  tft.setTouch(touchCal);
  if (saveCalibration()) Serial.println("[TOUCH] Calibration saved to NVS.");
  else Serial.println("[TOUCH] WARNING: failed to save calibration.");

  Serial.printf("[TOUCH] cal={%u,%u,%u,%u,%u}\n",
                touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4]);
}

void drawTouchMarker(uint16_t x, uint16_t y) {
  drawFrame();
  tft.drawCircle(x, y, 12, kMagenta);
  tft.drawCircle(x, y, 13, kCyan);
  tft.drawFastHLine(max(0, int(x) - 18), y, 36, kWhite);
  tft.drawFastVLine(x, max(0, int(y) - 18), 36, kWhite);
  drawStatusLine("TOUCH x=" + String(x) + " y=" + String(y), kGreen);
}

void printBootInfo() {
  Serial.println();
  Serial.println("======================================");
  Serial.println(" SPECTER//OS - CYD4 PHASE 0A");
  Serial.println("======================================");
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Expected display: ST7796S 480x320 landscape");
  Serial.println("Expected touch: XPT2046");
  Serial.println("PlatformIO port: COM4");
}
}

void setup() {
  Serial.begin(specter::board::kSerialBaud);
  delay(250);
  printBootInfo();

  pinMode(specter::board::kTftBl, OUTPUT);
  digitalWrite(specter::board::kTftBl, HIGH);

  tft.init();
  tft.setRotation(specter::board::kRotation);
  tft.fillScreen(kBg);
  Serial.printf("[DISPLAY] TFT_eSPI reports %dx%d\n", tft.width(), tft.height());

  if (loadCalibration()) {
    tft.setTouch(touchCal);
    touchReady = true;
    Serial.println("[TOUCH] Loaded calibration from NVS.");
  } else {
    Serial.println("[TOUCH] No saved calibration. Starting calibration.");
    calibrateTouch();
    touchReady = true;
  }

  drawFrame();
  drawStatusLine(touchReady ? "READY - touch the screen" : "TOUCH NOT READY",
                 touchReady ? kCyan : kYellow);
}

void loop() {
  if (touchReady) {
    uint16_t x = 0;
    uint16_t y = 0;
    if (tft.getTouch(&x, &y, 350)) {
      Serial.printf("[TOUCH] x=%u y=%u\n", x, y);
      drawTouchMarker(x, y);
      delay(90);
    }
  }

  const uint32_t now = millis();
  if (now - lastStatsMs >= 5000) {
    lastStatsMs = now;
    Serial.printf("[SYS] uptime=%lus heap=%u min_heap=%u\n",
                  now / 1000UL, ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }
  delay(5);
}
