# Phase 0J.2 acceptance

## Direction
- [ ] patrol turns left when moving left
- [ ] patrol turns right when moving right
- [ ] DATA interaction faces left toward terminal
- [ ] CHARGE interaction faces right toward dock
- [ ] STABILIZE interaction faces right toward stabilizer
- [ ] SLEEP faces left toward sleep node
- [ ] idle scan effect follows current facing
- [ ] teleport replacement is direction-aware

## Clean FX
- [ ] DATA cone contains no second Ghost pixels
- [ ] DATA cone originates cleanly from facing side
- [ ] CHARGE plume contains no second Ghost pixels
- [ ] PLAY/STABILIZE rings are clean and readable
- [ ] FX do not change Ghost body scale
- [ ] FX remain aligned to shared anchor

## Regression
- [ ] body size remains fixed
- [ ] no flicker
- [ ] 8-bit buffer remains active
- [ ] strict station gating remains intact
- [ ] autonomy thresholds remain intact
