#include "touch.h"
namespace input {
static Reader readTouch = nullptr;
static bool wasDown = false;
void attach(Reader reader) { readTouch = reader; wasDown = false; }
bool pressed(Point& point) {
    const bool down = readTouch && readTouch(point);
    const bool edge = down && !wasDown;
    wasDown = down;
    return edge;
}
}
