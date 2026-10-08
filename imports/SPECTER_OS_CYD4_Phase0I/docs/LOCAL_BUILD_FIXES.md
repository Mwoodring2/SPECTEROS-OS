# Local CYD4 build corrections

Kept the supplied 8-bit stage and sparse idle cadence. Replaced unsupported TFT_eSprite transparent pushImage calls with keyed RGB565 RAM compositing for both idle and FX frames. Added necessary function forward declarations. Sprite/FX art, mappings, station gating, and state format remain as supplied.
