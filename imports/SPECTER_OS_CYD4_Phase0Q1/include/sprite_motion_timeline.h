#pragma once
#include <Arduino.h>
namespace forge::sprite {
struct MotionPose { int8_t x=0; int8_t y=0; uint8_t frame=0; };
struct MotionKeyframe { uint16_t at_ms=0; MotionPose pose; };
// Standard: never scale body; use frame selection + small anchor-relative offsets.
}
