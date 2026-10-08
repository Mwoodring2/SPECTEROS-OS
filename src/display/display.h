#pragma once
#include <stdint.h>

// Supply these callbacks from the board's display driver. Coordinates are pixels.
namespace display {
struct Driver {
    int width;
    int height;
    void (*clear)(uint16_t color);
    void (*rectangle)(int x, int y, int width, int height, uint16_t color);
    void (*text)(int centerX, int y, const char* text, uint16_t color, int scale);
    void (*present)();
};
void attach(const Driver& driver);
bool ready();
const Driver& driver();
}
