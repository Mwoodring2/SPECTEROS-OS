# Phase 0J.2 — Direction + Clean FX

Parent: Phase 0J architecture PASS.

Phase 0J.1 was insufficient visually.

## Direction
- fixed teleport completion facing bug (old code compared destination after position overwrite)
- station interactions now deliberately face their station
- DATA and SLEEP face left
- CHARGE and STABILIZE face right
- normal patrol still updates facing from movement
- idle FX follow current facing
- replacement teleport/dissolve have left/right variants
- rest pose has left/right variants

## Clean FX
Production overlays are no longer extracted from flattened character artwork.

They are clean FX-only assets:
- DATA: cyan projection cone + scan bands + particles
- CHARGE: cyan energy plume + magenta energy flecks
- PLAY/STABILIZE/CHIRP: clean orbit rings + moving particles

There are zero Ghost-body pixels in these production FX overlays.

## Locked
- Phase 0J Body / FX / Replacement architecture
- fixed body scale and bottom-center anchor
- accepted 8-bit buffered renderer
- strict station gating
- autonomy thresholds
