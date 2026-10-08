# Phase 0E.1 — Flicker hotfix

## Expected result

The full habitat is composited off-screen and transferred in one push.

### Verify
- [ ] heavy environment flicker is gone
- [ ] Ghost still patrols smoothly
- [ ] grid does not flash during movement
- [ ] habitat props do not flash during movement
- [ ] Ghost sprite remains clean
- [ ] direction flip remains correct
- [ ] status labels remain intact
- [ ] touch remains correct
- [ ] no reset/watchdog issue

### Failure mode
If `STAGE BUFFER ALLOC FAILED` appears, capture serial output including the
reported free heap. Do not change hardware/display settings.
