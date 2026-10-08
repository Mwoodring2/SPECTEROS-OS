# Phase 0M — Audio & Personality Feedback

Parent: Phase 0L.1 sprite-animation milestone.

## Audio backend
Current default backend uses the Hosyond/LCDWiki ESP32-32E onboard audio path:
- DAC output: GPIO26
- onboard mono amplifier enable: GPIO4
- amplifier enable is active-low

Audio is synthesized as short non-blocking tone patterns.

The recently approved external NS4168 I2S module remains a future backend. Its BCLK / WS / DIN pins are intentionally not guessed in this build.

## Non-blocking engine
Added `audio_engine.h/.cpp`.

Properties:
- no delay() calls inside audio playback
- one active cue channel with priorities
- DAC output is serviced incrementally from `loop()`
- catch-up work is bounded so rendering is not starved
- amplifier powers down between cues
- mute and 0–100 volume API are included

Default:
- volume = 58
- muted = false

## Sound mappings
- Boot -> short identity sting
- Button touch -> UI confirm tick
- DATA -> scan chirp
- CHARGE -> rising energy cue
- STABILIZE -> harmonic repair cue
- PLAY -> playful digital chirp
- Teleport start -> descending spectral cue
- Teleport arrival -> rising arrival cue
- Sleep -> low soft descent
- Wake -> short rising cue
- Happy tap -> friendly chirp
- Annoyed tap -> lower warning chirp
- Idle CHIRP personality -> tiny personality chirp
- Autonomous low-charge trigger -> low-power cue

## Preserved
- Phase 0L.1 approved visual library
- sprite animation milestone
- 8-bit renderer
- strict station gating
- teleport visuals
- no blocking audio playback
