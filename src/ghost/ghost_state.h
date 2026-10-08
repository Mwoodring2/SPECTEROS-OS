#pragma once
#include <stdint.h>
namespace ghost {
enum class Emotion { Happy, Neutral, Tired, Unstable };
enum class Action { Data, Play, Charge, Stabilize };
struct State {
    uint8_t signal = 72;
    uint8_t charge = 54;
    uint8_t mood = 86;
    uint8_t stability = 80;
    uint32_t care = 0;
};
class Pet {
public:
    State state;
    void begin(uint32_t now);
    void tick(uint32_t now);
    bool act(Action action, uint32_t now);
    Emotion emotion() const;
    bool playing(uint32_t now) const;
private:
    uint32_t lastTick = 0;
    uint32_t remainder = 0;
    uint32_t lastSave = 0;
    uint32_t lastAction = 0;
    uint32_t playStarted = 0;
    bool hasAction = false;
    bool hasPlay = false;
    int64_t epochAnchor = 0;
    void decay(uint32_t minutes);
    void save();
};
extern Pet pet;
}
