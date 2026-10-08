#include "hardware_test.h"
#include "../display/display.h"
#include "../input/touch.h"
#include "theme.h"
#include <Arduino.h>
#include <stdio.h>
namespace ui {
void beginHardwareTest() {
    const auto& d = display::driver();
    d.clear(theme::black);
    d.text(240, 16, "SPECTER//OS", theme::cyan, 3);
    d.text(240, 48, "CYD4 HARDWARE TEST", theme::white, 2);
    d.text(240, 90, "DISPLAY: ST7796S", theme::cyan, 2);
    d.text(240, 114, "RESOLUTION: 480x320", theme::cyan, 2);
    d.text(240, 138, "TOUCH: XPT2046", theme::cyan, 2);
    d.text(240, 162, "PORT: COM4", theme::cyan, 2);
    d.rectangle(30, 204, 420, 2, theme::cyan);
    d.rectangle(30, 274, 420, 2, theme::cyan);
    d.rectangle(30, 204, 2, 72, theme::cyan);
    d.rectangle(448, 204, 2, 72, theme::cyan);
    d.text(240, 232, "[ TOUCH TEST AREA ]", theme::cyan, 2);
    d.present();
}
void tickHardwareTest() {
    input::Point point;
    if (!input::pressed(point)) return;
    beginHardwareTest();
    const auto& d = display::driver();
    d.rectangle(point.x - 5, point.y - 1, 11, 3, theme::white);
    d.rectangle(point.x - 1, point.y - 5, 3, 11, theme::white);
    char status[64];
    snprintf(status, sizeof(status), "TOUCH X:%d Y:%d %s", point.x, point.y,
        point.x >= 30 && point.x < 450 && point.y >= 204 && point.y < 276 ? "AREA OK" : "DETECTED");
    d.text(240, 294, status, theme::white, 1);
    d.present();
    Serial.println(status);
}
}
