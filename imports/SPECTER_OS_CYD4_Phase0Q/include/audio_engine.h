#pragma once

#include <Arduino.h>

namespace specter::audio {

enum class Cue : uint8_t {
  None,
  Boot,
  UiConfirm,
  Data,
  Charge,
  Stabilize,
  Play,
  TeleportOut,
  TeleportIn,
  Sleep,
  Wake,
  Happy,
  Annoyed,
  Chirp,
  LowPower
};

class Engine {
 public:
  void begin(int dacPin, int ampEnablePin);
  void update();

  void play(Cue cue, uint8_t priority = 1);
  void stop();

  void setMuted(bool muted);
  bool muted() const { return muted_; }

  void setVolume(uint8_t volume);
  uint8_t volume() const { return volume_; }

  bool active() const { return active_; }
  Cue currentCue() const { return cue_; }

 public:
  struct Note {
    uint16_t frequencyHz;
    uint16_t durationMs;
    uint16_t gapMs;
    uint8_t level;
  };

 private:
  const Note* patternFor(Cue cue, uint8_t& count) const;
  void startNote(uint32_t nowMs, uint32_t nowUs);
  void finishCue();
  void setAmp(bool enabled);
  void writeCentered(bool high);

  int dacPin_ = -1;
  int ampEnablePin_ = -1;

  bool muted_ = false;
  uint8_t volume_ = 58;

  bool active_ = false;
  bool notePlaying_ = false;
  bool squareHigh_ = false;
  Cue cue_ = Cue::None;
  uint8_t priority_ = 0;
  uint8_t noteIndex_ = 0;
  uint8_t noteCount_ = 0;

  const Note* notes_ = nullptr;

  uint32_t noteStartMs_ = 0;
  uint32_t gapStartMs_ = 0;
  uint32_t nextToggleUs_ = 0;
  uint32_t halfPeriodUs_ = 0;
};

extern Engine engine;

}  // namespace specter::audio
