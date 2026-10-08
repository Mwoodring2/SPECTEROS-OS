#include "boot_screen.h"
#include "theme.h"
#include "../display/display.h"
#include "../ghost/ghost.h"
#include "../ghost/ghost_state.h"
#include <stdio.h>
#include <time.h>
namespace ui {
namespace {
void outline(int x, int y, int w, int h) {
    const auto& d = display::driver();
    d.rectangle(x, y, w, 1, theme::cyan);
    d.rectangle(x, y + h - 1, w, 1, theme::cyan);
    d.rectangle(x, y, 1, h, theme::cyan);
    d.rectangle(x + w - 1, y, 1, h, theme::cyan);
}
void stat(int row, const char* label, unsigned value) {
    const auto& d = display::driver();
    const int y = d.height * (60 + row * 6) / 100;
    const int x = d.width * 30 / 100;
    const int w = d.width * 45 / 100;
    d.text(d.width * 15 / 100, y, label, theme::cyan, 1);
    d.rectangle(x, y, w, 7, theme::dimCyan);
    if (value) d.rectangle(x, y, w * value / 100, 7, theme::cyan);
    char text[8];
    snprintf(text, sizeof(text), "%u%%", value);
    d.text(d.width * 87 / 100, y, text, theme::cyan, 1);
}
}
void renderHome(uint32_t now) {
    if (!display::ready()) return;
    const auto& d = display::driver();
    const auto& pet = ghost::pet;
    d.clear(theme::black);
    d.text(d.width / 5, d.height / 25, "SPECTER//OS", theme::cyan, 1);
    char clock[8] = "--:--";
    const time_t epoch = time(nullptr);
    if (epoch >= 1700000000) {
        struct tm local;
        localtime_r(&epoch, &local);
        strftime(clock, sizeof(clock), "%H:%M", &local);
    }
    d.text(d.width * 87 / 100, d.height / 25, clock, theme::cyan, 1);
    const auto emotion = pet.emotion();
    const bool play = pet.playing(now);
    int size = d.width / 40;
    if (size > d.height / 55) size = d.height / 55;
    if (size < 1) size = 1;
    const uint32_t frame = now / 100;
    const int bob = int(frame % 16 < 8 ? frame % 8 : 7 - frame % 8) - 3;
    const int sway = play ? (int(frame % 7) - 3) * size : 0;
    const uint8_t reveal = emotion == ghost::Emotion::Unstable ? 160 : 255;
    ghost::draw(d.width / 2 + sway, d.height * 31 / 100 + bob, size, reveal, frame);
    // Tired ghosts fade; unstable ghosts tear; play emits glitch particles.
    if (emotion == ghost::Emotion::Tired)
        for (int y = 0; y < 12; y += 2)
            d.rectangle(d.width / 2 - 5 * size, d.height * 31 / 100 + bob + (y - 6) * size,
                        11 * size, 1, theme::black);
    if (play || emotion == ghost::Emotion::Happy)
        for (int i = 0; i < 3; ++i)
            d.rectangle(d.width / 2 + (int((frame + i * 7) % 31) - 15) * size,
                        d.height * 31 / 100 + (i - 1) * 5 * size, 2, 2, theme::cyan);
    const char* reaction = play ? "PLAYING" : emotion == ghost::Emotion::Unstable ? "UNSTABLE" :
        emotion == ghost::Emotion::Tired ? "TIRED" : emotion == ghost::Emotion::Happy ? "HAPPY" : "IDLE";
    d.text(d.width / 2, d.height / 2, reaction, theme::dimCyan, 1);
    stat(0, "SIGNAL", pet.state.signal);
    stat(1, "CHARGE", pet.state.charge);
    stat(2, "MOOD", pet.state.mood);
    stat(3, "STABILITY", pet.state.stability);
    const char* labels[] = {"DATA", "PLAY", "CHARGE", "STABLE"};
    const int y = d.height * 86 / 100;
    const int h = d.height / 10;
    for (int i = 0; i < 4; ++i) {
        const int x = i * d.width / 4 + 2;
        const int w = d.width / 4 - 4;
        outline(x, y, w, h);
        d.text(x + w / 2, y + (h - 8) / 2, labels[i], theme::cyan, 1);
    }
    d.present();
}
void touchHome(int x, int y, uint32_t now) {
    if (!display::ready()) return;
    const auto& d = display::driver();
    const int top = d.height * 86 / 100;
    if (x < 0 || x >= d.width || y < top || y >= top + d.height / 10) return;
    for (int i = 0; i < 4; ++i) {
        const int left = i * d.width / 4 + 2;
        if (x >= left && x < left + d.width / 4 - 4) {
            const ghost::Action actions[] = {ghost::Action::Data, ghost::Action::Play,
                ghost::Action::Charge, ghost::Action::Stabilize};
            ghost::pet.act(actions[i], now);
            return;
        }
    }
}
}
