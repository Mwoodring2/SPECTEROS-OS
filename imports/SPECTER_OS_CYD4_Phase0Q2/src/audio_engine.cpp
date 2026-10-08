#include "audio_engine.h"

namespace specter::audio {

namespace {

using Note = Engine::Note;

// UI / identity
constexpr Note kBoot[] = {
    {660, 70, 18, 52},
    {880, 80, 20, 58},
    {1320, 105, 0, 64},
};

constexpr Note kUiConfirm[] = {
    {1180, 40, 0, 42},
};

// Actions
constexpr Note kData[] = {
    {880, 55, 16, 45},
    {1110, 55, 16, 50},
    {1480, 90, 0, 56},
};

constexpr Note kCharge[] = {
    {420, 70, 10, 42},
    {560, 70, 10, 48},
    {740, 70, 10, 53},
    {980, 110, 0, 58},
};

constexpr Note kStabilize[] = {
    {520, 55, 8, 38},
    {780, 70, 8, 44},
    {1040, 80, 10, 48},
    {780, 70, 0, 42},
};

constexpr Note kPlay[] = {
    {940, 55, 14, 50},
    {1320, 55, 14, 54},
    {1080, 45, 12, 48},
    {1540, 85, 0, 58},
};

// Teleport
constexpr Note kTeleportOut[] = {
    {760, 45, 6, 44},
    {620, 45, 6, 48},
    {480, 55, 6, 52},
    {340, 70, 0, 56},
};

constexpr Note kTeleportIn[] = {
    {380, 45, 6, 44},
    {560, 45, 6, 48},
    {840, 55, 6, 53},
    {1260, 85, 0, 58},
};

// Personality
constexpr Note kSleep[] = {
    {520, 80, 18, 30},
    {390, 110, 0, 26},
};

constexpr Note kWake[] = {
    {520, 55, 12, 34},
    {760, 70, 0, 42},
};

constexpr Note kHappy[] = {
    {1180, 45, 12, 42},
    {1480, 65, 0, 48},
};

constexpr Note kAnnoyed[] = {
    {620, 45, 10, 38},
    {510, 75, 0, 42},
};

constexpr Note kChirp[] = {
    {980, 35, 8, 34},
    {1240, 45, 0, 38},
};

constexpr Note kLowPower[] = {
    {360, 80, 16, 30},
    {300, 110, 0, 27},
};

}  // namespace

Engine engine;

void Engine::begin(int dacPin, int ampEnablePin) {
  dacPin_ = dacPin;
  ampEnablePin_ = ampEnablePin;

  if (ampEnablePin_ >= 0) {
    pinMode(ampEnablePin_, OUTPUT);
    setAmp(false);
  }

  if (dacPin_ >= 0) {
    dacWrite(dacPin_, 128);
  }

  Serial.printf("[AUDIO] init dac=%d amp_en=%d volume=%u muted=%d\n",
                dacPin_, ampEnablePin_, volume_, muted_ ? 1 : 0);
}

void Engine::setAmp(bool enabled) {
  if (ampEnablePin_ < 0) return;
  // Board amp control is active-low.
  digitalWrite(ampEnablePin_, enabled ? LOW : HIGH);
}

void Engine::setMuted(bool muted) {
  muted_ = muted;
  if (muted_) stop();
}

void Engine::setVolume(uint8_t volume) {
  volume_ = volume > 100 ? 100 : volume;
}

void Engine::writeCentered(bool high) {
  if (dacPin_ < 0) return;

  uint8_t level = 0;
  if (active_ && notes_ && noteIndex_ < noteCount_) {
    level = notes_[noteIndex_].level;
  }

  const uint16_t scaled = static_cast<uint16_t>(level) * volume_ / 100;
  const int16_t delta = static_cast<int16_t>(scaled);
  const uint8_t value = static_cast<uint8_t>(
      constrain(high ? 128 + delta : 128 - delta, 0, 255));

  dacWrite(dacPin_, value);
}

const Engine::Note* Engine::patternFor(Cue cue, uint8_t& count) const {
  count = 0;

  switch (cue) {
    case Cue::Boot:
      count = sizeof(kBoot) / sizeof(kBoot[0]); return kBoot;
    case Cue::UiConfirm:
      count = sizeof(kUiConfirm) / sizeof(kUiConfirm[0]); return kUiConfirm;
    case Cue::Data:
      count = sizeof(kData) / sizeof(kData[0]); return kData;
    case Cue::Charge:
      count = sizeof(kCharge) / sizeof(kCharge[0]); return kCharge;
    case Cue::Stabilize:
      count = sizeof(kStabilize) / sizeof(kStabilize[0]); return kStabilize;
    case Cue::Play:
      count = sizeof(kPlay) / sizeof(kPlay[0]); return kPlay;
    case Cue::TeleportOut:
      count = sizeof(kTeleportOut) / sizeof(kTeleportOut[0]); return kTeleportOut;
    case Cue::TeleportIn:
      count = sizeof(kTeleportIn) / sizeof(kTeleportIn[0]); return kTeleportIn;
    case Cue::Sleep:
      count = sizeof(kSleep) / sizeof(kSleep[0]); return kSleep;
    case Cue::Wake:
      count = sizeof(kWake) / sizeof(kWake[0]); return kWake;
    case Cue::Happy:
      count = sizeof(kHappy) / sizeof(kHappy[0]); return kHappy;
    case Cue::Annoyed:
      count = sizeof(kAnnoyed) / sizeof(kAnnoyed[0]); return kAnnoyed;
    case Cue::Chirp:
      count = sizeof(kChirp) / sizeof(kChirp[0]); return kChirp;
    case Cue::LowPower:
      count = sizeof(kLowPower) / sizeof(kLowPower[0]); return kLowPower;
    default:
      return nullptr;
  }
}

void Engine::play(Cue cue, uint8_t priority) {
  if (muted_ || cue == Cue::None) return;

  if (active_ && priority < priority_) {
    return;
  }

  uint8_t count = 0;
  const Note* pattern = patternFor(cue, count);
  if (pattern == nullptr || count == 0) return;

  cue_ = cue;
  priority_ = priority;
  notes_ = pattern;
  noteCount_ = count;
  noteIndex_ = 0;
  active_ = true;
  notePlaying_ = false;
  squareHigh_ = false;
  noteStartMs_ = 0;
  gapStartMs_ = 0;
  nextToggleUs_ = 0;
  halfPeriodUs_ = 0;

  setAmp(true);
  Serial.printf("[AUDIO] cue=%u priority=%u\n",
                static_cast<unsigned>(cue_), priority_);
}

void Engine::startNote(uint32_t nowMs, uint32_t nowUs) {
  if (!active_ || notes_ == nullptr || noteIndex_ >= noteCount_) {
    finishCue();
    return;
  }

  const Note& note = notes_[noteIndex_];
  noteStartMs_ = nowMs;
  notePlaying_ = true;
  squareHigh_ = false;

  if (note.frequencyHz == 0) {
    halfPeriodUs_ = 0;
    dacWrite(dacPin_, 128);
    return;
  }

  halfPeriodUs_ = 500000UL / note.frequencyHz;
  if (halfPeriodUs_ == 0) halfPeriodUs_ = 1;
  nextToggleUs_ = nowUs;
  writeCentered(false);
}

void Engine::finishCue() {
  active_ = false;
  notePlaying_ = false;
  cue_ = Cue::None;
  priority_ = 0;
  notes_ = nullptr;
  noteCount_ = 0;
  noteIndex_ = 0;
  if (dacPin_ >= 0) dacWrite(dacPin_, 128);
  setAmp(false);
}

void Engine::stop() {
  finishCue();
}

void Engine::update() {
  if (!active_ || muted_) return;

  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  if (!notePlaying_) {
    if (noteStartMs_ == 0) {
      startNote(nowMs, nowUs);
      return;
    }

    const Note& previous = notes_[noteIndex_];
    if (nowMs - gapStartMs_ >= previous.gapMs) {
      ++noteIndex_;
      if (noteIndex_ >= noteCount_) {
        finishCue();
        return;
      }
      startNote(nowMs, nowUs);
    }
    return;
  }

  const Note& note = notes_[noteIndex_];

  if (nowMs - noteStartMs_ >= note.durationMs) {
    notePlaying_ = false;
    gapStartMs_ = nowMs;
    if (dacPin_ >= 0) dacWrite(dacPin_, 128);

    if (note.gapMs == 0) {
      ++noteIndex_;
      if (noteIndex_ >= noteCount_) {
        finishCue();
        return;
      }
      startNote(nowMs, nowUs);
    }
    return;
  }

  if (halfPeriodUs_ == 0) return;

  // Catch up at most a few toggles per loop so rendering is never starved.
  uint8_t catchUp = 0;
  while (static_cast<int32_t>(nowUs - nextToggleUs_) >= 0 && catchUp < 4) {
    squareHigh_ = !squareHigh_;
    writeCentered(squareHigh_);
    nextToggleUs_ += halfPeriodUs_;
    ++catchUp;
  }
}

}  // namespace specter::audio
