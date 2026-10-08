# SPECTER//OS Phase 0C acceptance

## Baseline regression
- [ ] Display remains 480x320 landscape
- [ ] Touch calibration remains correct
- [ ] Digital Ghost sprite remains clean
- [ ] RGB565 byte-swap remains enabled
- [ ] No boot loop / watchdog reset

## Personality
- [ ] Direct Ghost tap registers
- [ ] Friendly tap reaction appears
- [ ] Mild annoyed tap reaction eventually appears
- [ ] Idle personality events occur occasionally
- [ ] Hover/bob motion looks acceptable

## Pet actions
- [ ] DATA has a distinct visual response
- [ ] PLAY has a distinct visual response
- [ ] CHARGE has a distinct visual response
- [ ] STABILIZE has a distinct visual response

## Dormancy
- [ ] Top-right state label toggles dormancy
- [ ] DORMANT label appears
- [ ] sleep visual cue appears
- [ ] tapping Ghost wakes it
- [ ] Charge recovers slowly while dormant

## Persistence
- [ ] Pet stats survive reboot
- [ ] Interaction state does not corrupt NVS

## UX
- [ ] Ghost still feels readable at this size
- [ ] Action feedback is noticeable
- [ ] No obvious false touch triggers
