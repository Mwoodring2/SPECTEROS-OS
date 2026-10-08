#pragma once
namespace input {
struct Point { int x; int y; };
// Reader reports true while pressed, in rotated display pixel coordinates.
using Reader = bool (*)(Point& point);
void attach(Reader reader);
bool pressed(Point& point);
}
