#include "cyd4_config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include "../display/display.h"
#include "../input/touch.h"
namespace {
TFT_eSPI tft;
void clear(uint16_t color) { tft.fillScreen(color); }
void rectangle(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}
void text(int cx, int y, const char* value, uint16_t color, int scale) {
    tft.setTextFont(1);
    tft.setTextSize(scale);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(value, cx, y);
}
void present() {}
bool readTouch(input::Point& point) {
    uint16_t x, y;
    if (!tft.getTouch(&x, &y)) return false;
    point = {int(x), int(y)};
    return true;
}
}
namespace board {
void begin() {
    Serial.begin(115200);
    pinMode(33, OUTPUT);
    digitalWrite(33, HIGH);
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH); // Deselect SD card.
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH); // Disable unused speaker amplifier.
    tft.init();
    tft.setRotation(1);
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);
    Preferences prefs;
    uint16_t calibration[5];
    bool calibrated = false;
    const bool storage = prefs.begin("specter-touch", false);
    if (storage && prefs.getBytesLength("landscape") == sizeof(calibration)) {
        prefs.getBytes("landscape", calibration, sizeof(calibration));
        calibrated = true;
    }
    if (!calibrated) {
        tft.fillScreen(TFT_BLACK);
        text(240, 120, "TOUCH CALIBRATION", TFT_CYAN, 2);
        text(240, 150, "Tap each corner target", TFT_WHITE, 1);
        tft.calibrateTouch(calibration, TFT_CYAN, TFT_BLACK, 15);
        if (storage) prefs.putBytes("landscape", calibration, sizeof(calibration));
    }
    if (storage) prefs.end();
    tft.setTouch(calibration);
    display::attach({tft.width(), tft.height(), clear, rectangle, text, present});
    input::attach(readTouch);
    Serial.println("SPECTER//OS: ST7796S 480x320 / XPT2046 ready");
}
}
