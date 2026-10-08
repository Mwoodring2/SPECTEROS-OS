#pragma once
#include <stdint.h>
namespace ui {
void beginBoot(uint32_t now);
// Returns true when the sequence has completed.
bool renderBoot(uint32_t now);
void renderHome(uint32_t now);
void touchHome(int x, int y, uint32_t now);
}
