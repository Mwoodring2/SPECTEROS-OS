"""
Regenerate include/ghost_idle_frames.h from assets/source/digital_ghost_master_sheet.png.

This script documents the Phase 0B sprite extraction pipeline. It is intentionally
simple and uses fixed top-row crop centers from the locked Digital Ghost sheet.
"""
from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "source" / "digital_ghost_master_sheet.png"
OUT = ROOT / "include" / "ghost_idle_frames.h"

CENTERS = [155, 390, 625, 860]
CROP_W, CROP_H = 225, 220
FRAME_W, FRAME_H = 96, 96
KEY = 0x0001

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

img = Image.open(SRC).convert("RGBA")
frames = []

for cx in CENTERS:
    x0 = max(0, cx - CROP_W // 2)
    crop = img.crop((x0, 0, min(img.width, x0 + CROP_W), CROP_H))
    fitted = ImageOps.contain(crop, (FRAME_W, FRAME_H), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", (FRAME_W, FRAME_H), (0,0,0,0))
    canvas.alpha_composite(fitted, ((FRAME_W-fitted.width)//2, (FRAME_H-fitted.height)//2))

    values = []
    for r,g,b,a in canvas.getdata():
        if a < 24:
            values.append(KEY)
            continue
        af = a / 255.0
        v = rgb565(int(r*af), int(g*af), int(b*af))
        values.append(0 if v == KEY else v)
    frames.append(values)

lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "namespace specter::assets {",
    f"constexpr uint16_t kGhostWidth = {FRAME_W};",
    f"constexpr uint16_t kGhostHeight = {FRAME_H};",
    f"constexpr uint16_t kGhostTransparent = 0x{KEY:04X};",
    f"constexpr uint8_t kGhostIdleFrameCount = {len(frames)};",
    "",
]

for n, frame in enumerate(frames):
    lines.append(f"static const uint16_t PROGMEM kGhostIdle{n}[kGhostWidth * kGhostHeight] = {{")
    for i in range(0, len(frame), 16):
        lines.append("  " + ", ".join(f"0x{x:04X}" for x in frame[i:i+16]) + ",")
    lines += ["};", ""]

lines.append("static const uint16_t* const kGhostIdleFrames[kGhostIdleFrameCount] = {")
for n in range(len(frames)):
    lines.append(f"  kGhostIdle{n},")
lines += ["};", "}  // namespace specter::assets", ""]

OUT.write_text("\n".join(lines), encoding="utf-8")
print("Wrote", OUT)
