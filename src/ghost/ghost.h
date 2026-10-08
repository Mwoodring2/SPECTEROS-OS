#pragma once
#include <stdint.h>
namespace ghost {
// Reveal ranges from zero (invisible) to 255 (fully visible).
void draw(int centerX, int centerY, int pixelSize, uint8_t reveal, uint32_t frame);
}
