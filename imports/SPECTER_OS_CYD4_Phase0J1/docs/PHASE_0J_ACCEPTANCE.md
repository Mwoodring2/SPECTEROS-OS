# Phase 0J acceptance

## Layering
- [ ] normal patrol body remains same size during DATA
- [ ] normal patrol body remains same size during CHARGE
- [ ] normal patrol body remains same size during STABILIZE
- [ ] normal patrol body remains same size during PLAY
- [ ] FX overlay does not contain a second visible Ghost body
- [ ] teleport fully replaces body
- [ ] glitch/dissolve fully replaces body
- [ ] sleep uses fixed-size rest body

## Visual quality
- [ ] DATA reads as Ghost + scan FX, not duplicate Ghost
- [ ] CHARGE reads as Ghost + energy FX
- [ ] STABILIZE reads as Ghost + orbital FX
- [ ] PLAY reads as Ghost + orbital FX
- [ ] effects remain anchored during animation
- [ ] no body growth/shrink

## Regression
- [ ] 8-bit buffer remains stable
- [ ] no flicker
- [ ] strict station gating remains intact
- [ ] autonomy thresholds remain intact
- [ ] fast teleport still works
- [ ] touch remains responsive
