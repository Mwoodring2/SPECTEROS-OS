#include "boot_screen.h"
#include "theme.h"
#include "../display/display.h"
#include "../ghost/ghost.h"
namespace ui {
static uint32_t started;
void beginBoot(uint32_t now) { started = now; }
bool renderBoot(uint32_t now) {
    const uint32_t elapsed = now - started;
    if (elapsed >= 7000) return true;
    if (!display::ready()) return false;
    const auto& d = display::driver();
    const int cx = d.width / 2;
    const int cy = d.height / 2;
    const uint32_t frame = elapsed / 50;
    d.clear(theme::black);
    if (elapsed >= 600 && elapsed < 1300) {
        // Small deterministic glitch bars clustered around the center.
        for (int i = 0; i < 9; ++i) {
            const uint32_t noise = (frame * 1664525u + i * 1013904223u);
            d.rectangle(cx + int(noise % 61) - 30,
                        cy + int((noise >> 8) % 21) - 10,
                        2 + int((noise >> 16) % 12), 1, theme::cyan);
        }
    }
    if (elapsed >= 1300) {
        d.text(cx, d.height / 5, "SPECTER//OS", theme::cyan, 2);
    }
    if (elapsed >= 2500 && elapsed < 5500) {
        d.text(cx, d.height * 4 / 5,
               "INITIALIZING SPECTRAL INTERFACE...", theme::dimCyan, 1);
    }
    if (elapsed >= 3900) {
        const uint8_t reveal = elapsed >= 5500 ? 255 :
            uint8_t((elapsed - 3900) * 255 / 1600);
        int size = d.width / 32;
        if (size > d.height / 40) size = d.height / 40;
        if (size < 1) size = 1;
        ghost::draw(cx, cy, size, reveal, frame);
    }
    if (elapsed >= 5500)
        d.text(cx, d.height * 4 / 5, "GHOST SYSTEM ONLINE", theme::cyan, 1);
    d.present();
    return false;
}
}
