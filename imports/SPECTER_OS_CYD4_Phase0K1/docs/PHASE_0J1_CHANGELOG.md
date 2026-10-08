# Phase 0J.1 — FX Cleanup & Alignment

Parent: Phase 0J architecture PASS.

No compositor architecture changes.

## DATA / SCAN
- removed residual character pixels from scan overlay
- retained cyan projection artwork only
- added +4 px X alignment trim so the projection sits just outside the stable body

## CHARGE
- removed central character mass from energy frames
- retained saturated cyan/magenta energy plume
- added +5 px X trim to keep the plume attached to Ghost without covering the body

## PLAY / STABILIZE / CHIRP
- restored the magenta orbital ring that Phase 0J's rectangular body stripping removed
- suppresses central cyan body details while allowing magenta ring pixels to cross the silhouette
- added -1 px Y trim

## Locked
- body scale and bottom-center anchor
- Body / FX / Replacement compositor
- exclusive replacement behavior
- 8-bit buffered habitat renderer
- strict station gating
- autonomy thresholds
- tool teleport behavior
