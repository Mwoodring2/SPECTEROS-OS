#include <Arduino.h>
#include "board/cyd4_config.h"
#include "ui/boot_screen.h"
#include "ghost/ghost_state.h"
#include "input/touch.h"
#include "ui/hardware_test.h"

#ifdef SPECTER_HARDWARE_TEST
void setup() {
    board::begin();
    ui::beginHardwareTest();
}
void loop() {
    ui::tickHardwareTest();
    delay(10);
}
#else
static bool home = false;
static uint32_t lastFrame;
void setup() {
    board::begin();
    ghost::pet.begin(millis());
    ui::beginBoot(millis());
    lastFrame = millis() - 50;
}
void loop() {
    const uint32_t now = millis();
    ghost::pet.tick(now);
    input::Point point;
    if (input::pressed(point) && home) ui::touchHome(point.x, point.y, now);
    if (now - lastFrame < 50) return;
    lastFrame = now;
    if (home) {
        ui::renderHome(now);
    } else if (ui::renderBoot(now)) {
        home = true;
        ui::renderHome(now);
    }
}
#endif
