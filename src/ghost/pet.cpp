#include "ghost_state.h"
#include <Preferences.h>
#include <time.h>
namespace ghost {
Pet pet;
namespace {
constexpr int64_t validEpoch = 1700000000;
uint8_t subtract(uint8_t value, uint32_t amount) {
    return amount >= value ? 0 : uint8_t(value - amount);
}
uint8_t add(uint8_t value, int amount) {
    return value + amount > 100 ? 100 : uint8_t(value + amount);
}
bool valid(const State& s) {
    return s.signal <= 100 && s.charge <= 100 && s.mood <= 100 && s.stability <= 100;
}
}
void Pet::begin(uint32_t now) {
    Preferences prefs;
    if (prefs.begin("specter-pet", true)) {
        if (prefs.getUInt("version", 0) == 1) {
            State loaded;
            loaded.signal = prefs.getUChar("signal", 72);
            loaded.charge = prefs.getUChar("charge", 54);
            loaded.mood = prefs.getUChar("mood", 86);
            loaded.stability = prefs.getUChar("stability", 80);
            loaded.care = prefs.getUInt("care", 0);
            if (valid(loaded)) state = loaded;
            remainder = prefs.getUInt("remainder", 0) % 60000;
            epochAnchor = prefs.getLong64("epoch", 0);
        }
        prefs.end();
    }
    const int64_t epoch = int64_t(time(nullptr));
    if (epoch >= validEpoch && epochAnchor >= validEpoch && epoch > epochAnchor) {
        const uint64_t duration = uint64_t(remainder) + uint64_t(epoch - epochAnchor) * 1000;
        decay(uint32_t(duration / 60000));
        remainder = uint32_t(duration % 60000);
    }
    lastTick = lastSave = now;
    tick(now);
    save();
}
void Pet::decay(uint32_t minutes) {
    // One point per minute for signal/charge/mood, two for stability.
    state.signal = subtract(state.signal, minutes);
    state.charge = subtract(state.charge, minutes);
    state.mood = subtract(state.mood, minutes);
    state.stability = subtract(state.stability, minutes > 50 ? 100 : minutes * 2);
}
void Pet::tick(uint32_t now) {
    const uint32_t elapsed = now - lastTick;
    lastTick = now;
    uint64_t duration = uint64_t(remainder) + elapsed;
    const int64_t epoch = int64_t(time(nullptr));
    epochAnchor = epoch >= validEpoch ? epoch : 0;
    decay(uint32_t(duration / 60000));
    remainder = uint32_t(duration % 60000);
    if (now - lastSave >= 60000) {
        save();
        lastSave = now;
    }
}
bool Pet::act(Action action, uint32_t now) {
    tick(now);
    if (hasAction && now - lastAction < 750) return false;
    hasAction = true;
    lastAction = now;
    switch (action) {
    case Action::Data: state.signal = add(state.signal, 18); break;
    case Action::Charge: state.charge = add(state.charge, 22); break;
    case Action::Stabilize: state.stability = add(state.stability, 20); break;
    case Action::Play:
        if (state.charge < 5) return false;
        state.charge = subtract(state.charge, 5);
        state.mood = add(state.mood, 16);
        hasPlay = true;
        playStarted = now;
        break;
    }
    if (state.care != UINT32_MAX) ++state.care;
    save();
    lastSave = now;
    return true;
}
Emotion Pet::emotion() const {
    if (state.stability < 25) return Emotion::Unstable;
    if (state.charge < 25) return Emotion::Tired;
    if (state.mood >= 70 && state.signal >= 40) return Emotion::Happy;
    return Emotion::Neutral;
}
bool Pet::playing(uint32_t now) const { return hasPlay && now - playStarted < 2500; }
void Pet::save() {
    Preferences prefs;
    if (!prefs.begin("specter-pet", false)) return;
    prefs.putUChar("signal", state.signal);
    prefs.putUChar("charge", state.charge);
    prefs.putUChar("mood", state.mood);
    prefs.putUChar("stability", state.stability);
    prefs.putUInt("care", state.care);
    prefs.putUInt("remainder", remainder);
    prefs.putLong64("epoch", epochAnchor);
    prefs.putUInt("version", 1);
    prefs.end();
}
}
