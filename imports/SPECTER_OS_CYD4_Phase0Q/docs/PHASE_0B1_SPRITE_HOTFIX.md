# Phase 0B.1 — Sprite rendering hotfix

Status entering hotfix:

- Phase 0B core loop: PASS
- Display/touch base: PASS
- Digital Ghost sprite: corrupted/noisy on TFT

Patch:

```cpp
tft.setSwapBytes(true);
```

No other runtime behavior is intentionally changed.

## Pass criteria

- [ ] Ghost silhouette is clean/readable
- [ ] cyan/magenta/white colors look correct
- [ ] transparent regions remain transparent
- [ ] UI remains correct
- [ ] touch still works
- [ ] NVS state persists
- [ ] no watchdog/reset issues
