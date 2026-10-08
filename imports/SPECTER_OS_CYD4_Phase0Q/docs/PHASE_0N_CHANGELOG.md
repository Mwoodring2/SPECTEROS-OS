# Phase 0N — Bond, Growth & Personality Core

Parent: Phase 0M (audio implemented, hardware validation pending).

## Persistent growth
Added a separate `GrowthState` saved under NVS key `growth0n`.

Fields:
- XP
- level
- curiosity
- playfulness
- resilience
- harmony
- rewarded action count
- level-up count

This is separate from the existing pet-state blob so the validated pet-state layout remains intact.

## XP / level
- levels 1–20
- 100 XP × current level required for the next level
- DATA / CHARGE / STABILIZE: +8 XP
- PLAY: +10 XP
- friendly Ghost tap: +2 XP
- per-action anti-farming reward cooldown: 4.5 s

Level-up:
- +2 affection
- temporary `LEVEL UP` status
- persistent save

## Personality axes
DATA:
- curiosity +3
- harmony +1

CHARGE:
- harmony +3
- resilience +1

STABILIZE:
- resilience +3
- harmony +1

PLAY:
- playfulness +4

Friendly taps:
- playfulness +1

Axes never decay.

## Personality labels
- CURIOUS
- PLAYFUL
- STEADY
- HARMONIC
- BALANCED

The dominant trait must lead by at least 8 points to display a specialized personality.

## Personality behavior
Long-term personality now biases idle expression only:
- curiosity -> SCAN
- harmony / resilience -> OBSERVE
- playfulness -> CHIRP
- low resilience slightly increases GLITCH

Integrity pressure still overrides personality.

Strict station gating is unchanged. Personality cannot route Ghost to DATA / CHARGE / STABILIZE.

## UI
Header now displays:
`LV n // PERSONALITY`

## Preserved
- Phase 0L.1 approved sprite/timing system
- Phase 0M audio architecture
- 8-bit buffered renderer
- strict station gating
- autonomy thresholds
