#include "ghost.h"
#include "../display/display.h"
#include "../ui/theme.h"
namespace ghost {
void draw(int cx, int cy, int size, uint8_t reveal, uint32_t frame) {
    if (!display::ready() || !reveal) return;
    static const char* sprite[] = {
        "00011111000", "00111111100", "01111111110", "11111111111",
        "11100100111", "11100100111", "11111111111", "11111111111",
        "11111111111", "11111111111", "11011011011", "10010010001"
    };
    const auto& d = display::driver();
    for (int y = 0; y < 12; ++y) {
        const bool phased = reveal == 255 || ((y * 37 + frame * 13) % 255) < reveal;
        if (!phased) continue;
        const int offset = reveal < 255 ? int((frame + y) % 3) - 1 : 0;
        for (int x = 0; x < 11; ++x)
            if (sprite[y][x] == '1')
                d.rectangle(cx + (x - 5) * size + offset, cy + (y - 6) * size,
                            size, size, theme::cyan);
    }
}
}
