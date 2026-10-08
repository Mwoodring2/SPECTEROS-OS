# Phase 0K.2 — Stabilize FX Detail Parity

Parent: Phase 0K.1.

## Goal
Replace the placeholder-looking stabilize ring with a dedicated high-detail stabilize overlay so it matches the quality bar of the other Ghost FX states.

## What changed
- Added a dedicated `ghost_stabilize_frames.h` overlay asset set.
- Replaced the old placeholder `drawCircle(...)` stabilize ring.
- Stabilize now renders as a richer authored overlay:
  - outer harmonic ring
  - segmented rotating arcs
  - inner containment ring
  - repair-beam details
  - orbiting accent particles
  - support glyph / repair icon
  - lower resonance pedestal

## Behavior
- Stabilize remains FX-only and layered above the body.
- No body pixels are embedded in the stabilize overlay.
- Left/right variants remain supported.
- The canonical anchor, scale, and 8-bit renderer are preserved.

## Preserved
- Phase 0J layered compositor architecture
- Phase 0K body motion and timing pass
- Phase 0K.1 teleport cleanup
- strict station gating
- accepted 8-bit buffered renderer
