#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace forge::sprite {

enum class BodyMode : uint8_t {
  Patrol,
  Rest
};

enum class FxMode : uint8_t {
  None,
  Scan,
  Charge,
  Stabilize,
  Play,
  Chirp
};

enum class ReplacementMode : uint8_t {
  None,
  Teleport,
  Dissolve
};

struct LayerPlan {
  BodyMode body = BodyMode::Patrol;
  FxMode fx = FxMode::None;
  ReplacementMode replacement = ReplacementMode::None;

  bool usesReplacement() const {
    return replacement != ReplacementMode::None;
  }
};

inline bool shouldDrawBody(const LayerPlan& plan) {
  return !plan.usesReplacement();
}

inline bool shouldDrawFx(const LayerPlan& plan) {
  return plan.fx != FxMode::None;
}

}  // namespace forge::sprite
