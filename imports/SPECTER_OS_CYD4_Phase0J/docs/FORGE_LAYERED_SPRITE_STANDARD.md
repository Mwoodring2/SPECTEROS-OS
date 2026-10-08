# Forge Layered Sprite Standard v1

Use this architecture for sprite-driven projects when appropriate.

## Layer 1 — Body
- fixed body dimensions
- fixed body anchor
- one active body state
- body owns apparent character scale
- never resize body per effect

## Layer 2 — FX-only overlay
- transparent canvas may be larger than body
- same shared anchor
- must not duplicate the character body
- intended for cones, rings, sparks, particles, trails, UI-like effects

## Layer 3 — Full replacement
- exclusive
- normal body is not drawn
- intended for teleport, dissolve, morph, death, cinematic transformation

## Draw order
1. environment
2. props
3. replacement OR body
4. FX-only overlays
5. small expression accents
6. UI

## Priority
replacement > body state > FX overlay > normal patrol

## SPECTER//OS implementation
Body:
- patrol idle frames
- rest pose

FX-only:
- scan
- charge
- stabilize/play ring
- chirp/play

Replacement:
- teleport
- dissolve/glitch

All layers share the same bottom-center anchor.
