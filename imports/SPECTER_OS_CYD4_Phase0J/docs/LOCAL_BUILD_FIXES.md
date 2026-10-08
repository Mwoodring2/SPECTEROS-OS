# Local build corrections

Preserved supplied 8-bit renderer. Corrected unsupported transparent TFT_eSprite pushImage calls using keyed RGB565 RAM compositing for body, overlay, and replacement frames. Added required forward declarations.

Current package supplies Patrol and Rest body modes; individual action body poses are not implemented. Visual verification that FX-only assets contain no Ghost silhouette remains pending on hardware.
