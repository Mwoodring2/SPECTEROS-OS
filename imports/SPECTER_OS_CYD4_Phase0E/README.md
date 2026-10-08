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



---

# Phase 0C — Ghost behavior and personality

Phase 0C builds directly on the accepted Phase 0B.1 sprite-rendering base.

## New behavior

- Ghost can now be tapped directly
- most taps produce a friendly reaction
- every fourth direct tap produces a mild annoyed reaction
- small hidden `AFFECTION` value tracks interaction history
- random idle personality events occur while awake
- the sprite has a subtle hover/bob motion
- DATA / PLAY / CHARGE / STABILIZE now create distinct visual reactions
- Ghost supports a simple **Dormant** state
- tapping the top-right state label toggles dormancy
- tapping Ghost while dormant wakes it
- dormancy slowly restores Charge and Mood
- pet state remains persistent in NVS

## Controls

- **Tap Ghost** — interact directly
- **Tap ONLINE / CONTENT / etc. in the top-right** — sleep/wake
- **DATA** — restore Signal
- **PLAY** — increase Mood and Affection
- **CHARGE** — restore Charge
- **STABILIZE** — restore Integrity

## Important

The accepted Phase 0B.1 rendering configuration remains unchanged:

```cpp
tft.setSwapBytes(true);
```

Do not remove it.

## Deliberately deferred

Phase 0C still does not implement true powered-off elapsed-time decay.
That needs a deliberate time source / RTC strategy rather than guessing based on boot time.

The first mini-game is also deferred until the direct interaction loop is validated on hardware.

## Phase 0C acceptance

- Ghost idle animation remains clean
- tapping Ghost visibly reacts
- repeated taps eventually show the annoyed reaction
- action buttons still work
- each action shows distinct feedback
- tapping the state label enters DORMANT
- dormant Ghost displays sleep cues
- tapping Ghost wakes it
- state survives reboot
- no display corruption
- no touch regression
- no watchdog resets


---

# Phase 0D — Autonomous Patrol + Movement Personality

Phase 0D makes Digital Ghost roam the full usable pet field instead of remaining center-anchored.

## New movement behavior

Ghost now:

- patrols across the full central field
- selects random destinations
- pauses naturally after arriving
- changes altitude
- returns toward center for action reactions
- moves more quickly when CONTENT
- moves slowly when LOW CHARGE
- jitters/behaves less predictably when UNSTABLE
- travels to a dedicated resting zone when DORMANT
- can be lured toward a tapped point in the patrol field
- remains directly tappable at its current location

## Patrol field

The active field stays between the stat HUD and action buttons so Ghost never covers critical controls.

Approximate usable region:

```text
x: 16 -> 464
y: 106 -> 260
```

Sprite bounds are clamped inside that area.

## Touch additions

- Tap Ghost: interact as before.
- Tap empty space in the central field: Ghost treats it as a signal ping and moves toward it.
- Tap the top-right state label: enter or leave Dormant state.

## State-based movement

### CONTENT
Faster, broader patrol.

### ONLINE
Normal patrol.

### LOW CHARGE
Slower movement with shorter trips.

### UNSTABLE
Small erratic drift/jitter.

### DORMANT
Ghost moves to a fixed sleep zone and remains there.

## Preserved from Phase 0C

- accepted ST7796S configuration
- XPT2046 calibration
- COM4
- Digital Ghost art
- `tft.setSwapBytes(true)`
- pet stats/actions
- NVS persistence
- direct tap reactions
- dormancy
- action effects

## Phase 0D acceptance

- Ghost traverses left/right across most of the visible pet field
- Ghost also changes vertical position
- Ghost pauses between patrol legs
- Ghost never overlaps the bottom action row
- Ghost never overlaps the stat bars
- tapping Ghost works at different patrol positions
- tapping empty field attracts Ghost
- CONTENT state visibly moves faster
- LOW CHARGE visibly moves slower
- DORMANT causes Ghost to move to its sleep zone
- action buttons still work while Ghost is elsewhere
- no major sprite trails
- no unacceptable flicker
- no watchdog resets

## Next phase

After patrol is accepted, the next logical milestone is a true virtual-pet environment:

- field props / home objects
- contextual destinations
- feeding/data terminal
- charging dock
- sleep node
- first mini-game
- object-aware autonomous behavior


---

# Phase 0D.1 — Direction flip + clean patrol loop

This patch addresses two issues observed on the physical CYD4 build:

1. Ghost patrolled left/right but only **faced left**.
2. The animation cycle occasionally showed **undesired extra-frame artifacts**.

## Fixes

- added **right-facing mirrored patrol frames**
- Ghost now flips direction based on horizontal movement
- changed the patrol loop to a cleaner ping-pong cycle:

```text
0 -> 1 -> 2 -> 1 -> repeat
```

- removed the fourth patrol frame from the moving loop so the idle motion reads cleaner during traversal

## Preserved

- full-field patrol behavior
- tap-to-lure
- action reactions
- sleep/dormancy
- stats/persistence
- RGB565 byte-swap fix
- CYD4 display/touch configuration
- COM4

## Acceptance

- Ghost faces left when moving left
- Ghost faces right when moving right
- patrol loop looks cleaner
- no stray “other sprite” artifacts appear during normal patrol animation
- Ghost remains tappable while moving
- action buttons still work


---

# Phase 0D.2 — One-sided sprite artifact hotfix

The physical test showed artifacts on one facing direction.

Root cause was identified in the asset conversion pipeline: the earlier fixed-width
crop windows were wide enough to capture pixels from the neighboring sprite in the
master sheet. When mirrored for the opposite direction, that leaked fragment appeared
on the opposite side of Ghost.

## Fix

- replaced broad centered crops with exact isolated top-row sprite ranges
- regenerated all three left-facing frames
- regenerated all three mirrored right-facing frames
- retained the clean `0 -> 1 -> 2 -> 1` patrol loop
- no movement or pet behavior changes

This patch is intentionally asset-only.

## Acceptance

- no detached sprite fragment on the left-facing loop
- no detached sprite fragment on the right-facing loop
- direction flip remains correct
- patrol behavior remains unchanged


---

# Phase 0D.3 — Patrol-status redraw hotfix

This patch addresses the remaining minor visual issue seen after Phase 0D.2:
the moving Ghost erase region could briefly clip the field status text, producing
partial labels such as `TROL`.

## Fix

- promoted the field status to persistent UI state
- the status strip is now redrawn whenever Ghost is redrawn
- status text is still updated for actions/reactions as before

No movement, sprite, or pet-logic behavior is intentionally changed.

## Environment note

The **digital environment/background** is still planned, but it is intentionally
deferred until after patrol readability is stable. The next environment-focused
phase should add a proper habitat layer: a digital backdrop plus props such as a
data terminal, charging dock, and sleep node.

## Acceptance

- `PATROL` never becomes partially erased during movement
- other status labels (`PLAYING`, `CHARGING`, etc.) remain intact during movement
- patrol behavior is unchanged
- sprite cleanliness is unchanged


---

# Phase 0E — Digital Environment Foundation

Phase 0E gives Digital Ghost its first real habitat.

This is intentionally the **foundation** layer: a lightweight, code-drawn digital environment
that keeps the current pet loop stable while making the field feel like a place Ghost actually
lives in.

## What this phase adds

### Digital backdrop
- dark cyber/digital stage background
- subtle top glow band
- faint grid lines
- simple “data-city” background bars
- habitat frame preserved inside the patrol area

### Habitat props
Ghost now has named environment props:

- **DATA terminal** on the left
- **CHARGE dock** on the right
- **SLEEP node** on the lower left
- **STABILIZER beacon** on the upper right

These props are lightweight vector UI art, so no new external art pipeline is required yet.

### Purposeful movement
Ghost can still roam freely, but now also uses the environment:

- low charge tends to favor the charge dock
- unstable state tends to favor the stabilizer
- dormancy sends Ghost to the sleep node
- DATA action sends Ghost toward the data terminal
- CHARGE action sends Ghost toward the charge dock
- STABILIZE action sends Ghost toward the stabilizer
- PLAY keeps Ghost in a more free-roaming mode

### Preserved
- clean left/right direction flip
- clean `0 -> 1 -> 2 -> 1` loop
- full-field patrol
- clean sprite frames
- persistent field status text
- tap Ghost interaction
- tap-to-lure movement
- dormancy
- pet stats and persistence
- working CYD4 display/touch configuration
- `tft.setSwapBytes(true)`

## Touch behavior
- Tap Ghost: interact directly
- Tap empty field: signal ping / lure Ghost
- Tap top-right state label: enter/exit dormancy
- Action buttons now also align with habitat props

## Acceptance focus
- environment should feel readable but not cluttered
- Ghost should remain clearly visible over the background
- props should read as locations
- movement should feel more purposeful
- no regression in sprite cleanliness, touch, or patrol

## Important note
This is the **first** environment phase, not the final art pass.

A later environment phase can replace or augment the vector habitat with richer background art,
more detailed stations, and contextual animations once the layout is accepted on hardware.
