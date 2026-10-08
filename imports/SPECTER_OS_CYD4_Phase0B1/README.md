# SPECTER//OS — CYD 4" Phase 0B

**Digital Ghost virtual-pet foundation**

This package is built directly on the accepted Phase 0A CYD4 hardware base.
It intentionally preserves the working ST7796S / XPT2046 configuration and COM4 setup.

## What Phase 0B adds

- SPECTER//OS boot/glitch splash
- Digital Ghost phase-in
- actual Digital Ghost art embedded as RGB565 sprite frames
- four-frame idle hover animation
- persistent virtual-pet state
- four pet stats:
  - SIGNAL
  - INTEGRITY
  - MOOD
  - CHARGE
- four touch actions:
  - DATA
  - PLAY
  - CHARGE
  - STABILIZE
- powered-on stat decay
- NVS save/load
- simple state reactions:
  - ONLINE
  - CONTENT
  - LOW CHARGE
  - UNSTABLE

## Important

Phase 0B expects the **Phase 0A touch calibration already stored in NVS**.

Do not erase the board before flashing 0B unless you intend to run Phase 0A calibration again.

If calibration is missing, 0B stops and tells you to return to Phase 0A instead of guessing touch coordinates.

## Flash

With the board still on COM4:

```powershell
pio run -e cyd4
pio run -e cyd4 -t upload
pio device monitor -p COM4 -b 115200
```

## First playable loop

After boot:

1. Ghost phases onto the screen.
2. Ghost idles in the center.
3. Stats appear above it.
4. Tap the four bottom actions.
5. Stats change immediately and are saved to NVS.
6. Ghost state changes as the values move through thresholds.
7. Stats slowly decay while the device remains powered.

Phase 0B deliberately does **not** implement offline/RTC decay yet. We first validate the pet loop and persistence.

## Pet actions

| Action | Effect |
|---|---|
| DATA | +Signal, small +Mood |
| PLAY | +Mood, small Charge/Signal cost |
| CHARGE | +Charge |
| STABILIZE | +Integrity, small Signal cost |

## Visual states

- **ONLINE** — normal neutral state
- **CONTENT** — high Mood + adequate Signal
- **LOW CHARGE** — Charge <= 24
- **UNSTABLE** — Integrity <= 29

These are first-pass state hooks. Dedicated animation sets come later.

## Sprite asset

The locked Digital Ghost master sheet is included at:

```text
assets/source/digital_ghost_master_sheet.png
```

The four embedded Phase 0B idle frames are generated into:

```text
include/ghost_idle_frames.h
```

A small extracted preview is included at:

```text
assets/ghost_idle_preview.png
```

The reproducible conversion script is:

```text
tools/build_ghost_frames.py
```

It requires Pillow if you regenerate the header on the PC.

## Acceptance checklist

- boot splash renders correctly
- Digital Ghost phase-in appears
- idle animation runs without obvious screen flicker
- existing Phase 0A touch calibration is reused
- all four pet buttons respond correctly
- each button changes the intended stat
- stats survive reboot
- Ghost state label changes at thresholds
- no watchdog resets
- no spontaneous touch presses
- serial monitor remains clean

## Next after acceptance — Phase 0C

Phase 0C should focus on making Ghost feel alive:

- richer idle animation
- tap-the-Ghost interaction
- reactions / emotes
- sleep/dormancy
- better action animations
- real elapsed-time handling
- first simple mini-game

---

## Phase 0B.1 sprite-rendering hotfix

This package is the accepted Phase 0B build with one targeted display fix:

```cpp
tft.setSwapBytes(true);
```

It is enabled immediately after `tft.init()` and before sprite rendering.

### Why

The Phase 0B UI, touch, pet state, and controls passed on physical hardware, but the embedded
Digital Ghost RGB565 sprite appeared noisy/striped. The PC-side source sprite was correct.

This hotfix addresses the RGB565 byte ordering used by `TFT_eSPI::pushImage()` without changing:

- display pins
- touch pins
- orientation
- calibration
- pet logic
- stat behavior
- NVS format
- COM4 configuration

### Acceptance

After flashing, verify:

- Ghost colors and shape render normally
- no striped/static-like corruption inside the sprite
- UI colors remain correct
- touch calibration is still retained
- pet stats/actions still work
- no new flicker or reboot loop

If the Ghost remains corrupted after this patch, stop there; the next diagnostic will be the
sprite conversion/keying path rather than changing the working display configuration.

