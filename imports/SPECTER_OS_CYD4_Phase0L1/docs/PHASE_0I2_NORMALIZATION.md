# Phase 0I.2 — FX normalization

Problem confirmed on physical hardware:
- authored action sprites changed apparent Ghost size
- frames shifted vertically/horizontally
- DATA sequence used an overly broad crop and read as broken

Fix:
- every FX frame uses one fixed source scale: 0.45
- every FX frame is anchored by Ghost's body bottom-center
- effects may extend around the body, but the body itself stays consistent
- DATA/SCAN was recropped into three clean authored frames
- no per-frame `contain()` fitting is used

Runtime body anchor:
- patrol body anchor: (48, 88) inside the normal 96x96 sprite
- FX body anchor: (72, 92) inside the 144x104 action canvas

The renderer aligns those anchors before drawing.
