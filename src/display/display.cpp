#include "display.h"
namespace display {
static Driver active{};
void attach(const Driver& driver) { active = driver; }
bool ready() {
    return active.width > 0 && active.height > 0 && active.clear &&
           active.rectangle && active.text && active.present;
}
const Driver& driver() { return active; }
}
