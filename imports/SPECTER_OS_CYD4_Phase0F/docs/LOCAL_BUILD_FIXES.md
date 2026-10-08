# Local CYD4 build corrections

The supplied Phase 0F ZIP required these corrections before flashing:
- Restore the accepted 8-bit RGB332 stage buffer from the working Phase 0E.1 build.
- Composite nontransparent RGB565 pixels with drawPixel; TFT_eSprite pushImage does not provide the display API transparent-key overload.
- Declare choosePatrolTarget before endHabitatInteraction calls it.

Hardware configuration, sprite assets, pet-state key, and habitat interaction behavior remain as supplied.
