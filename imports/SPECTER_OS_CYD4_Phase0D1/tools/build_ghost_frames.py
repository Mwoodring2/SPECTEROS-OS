"""
Generate left-facing and right-facing Phase 0D.1 patrol frames
from assets/source/digital_ghost_master_sheet.png.
"""
from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "source" / "digital_ghost_master_sheet.png"
OUT = ROOT / "include" / "ghost_idle_frames.h"

CENTERS = [155, 390, 625]
CROP_W, CROP_H = 225, 220
FRAME_W, FRAME_H = 96, 96
KEY = 0x0001

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def canvas_to_565(canvas):
    vals = []
    for r, g, b, a in canvas.getdata():
        if a < 24:
            vals.append(KEY)
        else:
            af = a / 255.0
            v = rgb565(int(r * af), int(g * af), int(b * af))
            vals.append(0 if v == KEY else v)
    return vals

img = Image.open(SRC).convert("RGBA")
left_frames = []
right_frames = []

for cx in CENTERS:
    x0 = max(0, cx - CROP_W // 2)
    crop = img.crop((x0, 0, min(img.width, x0 + CROP_W), CROP_H))
    fitted = ImageOps.contain(crop, (FRAME_W, FRAME_H), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", (FRAME_W, FRAME_H), (0,0,0,0))
    canvas.alpha_composite(fitted, ((FRAME_W - fitted.width) // 2, (FRAME_H - fitted.height) // 2))
    left_frames.append(canvas_to_565(canvas))
    right_frames.append(canvas_to_565(ImageOps.mirror(canvas)))

lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "namespace specter::assets {",
    f"constexpr uint16_t kGhostWidth = {FRAME_W};",
    f"constexpr uint16_t kGhostHeight = {FRAME_H};",
    f"constexpr uint16_t kGhostTransparent = 0x{KEY:04X};",
    f"constexpr uint8_t kGhostFacingFrameCount = {len(left_frames)};",
    "",
]

for prefix, frames in [("Left", left_frames), ("Right", right_frames)]:
    for n, frame in enumerate(frames):
        lines.append(f"static const uint16_t PROGMEM kGhost{prefix}{n}[kGhostWidth * kGhostHeight] = {{")
        for i in range(0, len(frame), 16):
            lines.append("  " + ", ".join(f"0x{x:04X}" for x in frame[i:i+16]) + ",")
        lines += ["};", ""]

lines.append("static const uint16_t* const kGhostLeftFrames[kGhostFacingFrameCount] = {")
for n in range(len(left_frames)):
    lines.append(f"  kGhostLeft{n},")
lines.append("};")

lines.append("static const uint16_t* const kGhostRightFrames[kGhostFacingFrameCount] = {")
for n in range(len(right_frames)):
    lines.append(f"  kGhostRight{n},")
lines += ["};", "}  // namespace specter::assets", ""]
OUT.write_text("\n".join(lines), encoding="utf-8")
print("Wrote", OUT)
