"""
Generate clean left/right Ghost patrol frames from the master sheet.

Important: use exact isolated top-row alpha ranges. Do not return to broad
centered crops: those can include pieces of the neighboring sprite and create
one-sided artifacts after mirroring.
"""
from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "source" / "digital_ghost_master_sheet.png"
OUT = ROOT / "include" / "ghost_idle_frames.h"

RANGES = [(125, 308), (327, 514), (532, 718)]
TOP, BOTTOM = 0, 230
FRAME_W = FRAME_H = 96
KEY = 0x0001

def rgb565(r,g,b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def to565(canvas):
    vals=[]
    for r,g,b,a in canvas.getdata():
        if a < 24:
            vals.append(KEY)
        else:
            af=a/255.0
            v=rgb565(int(r*af),int(g*af),int(b*af))
            vals.append(0 if v == KEY else v)
    return vals

img=Image.open(SRC).convert("RGBA")
left=[]
right=[]

for x0,x1 in RANGES:
    crop=img.crop((x0,TOP,x1,BOTTOM))
    alpha=crop.getchannel("A").point(lambda p: 0 if p < 24 else p)
    crop.putalpha(alpha)
    fitted=ImageOps.contain(crop,(FRAME_W,FRAME_H),Image.Resampling.NEAREST)
    canvas=Image.new("RGBA",(FRAME_W,FRAME_H),(0,0,0,0))
    canvas.alpha_composite(fitted,((FRAME_W-fitted.width)//2,(FRAME_H-fitted.height)//2))
    left.append(to565(canvas))
    right.append(to565(ImageOps.mirror(canvas)))

lines=[
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "namespace specter::assets {",
    f"constexpr uint16_t kGhostWidth = {FRAME_W};",
    f"constexpr uint16_t kGhostHeight = {FRAME_H};",
    f"constexpr uint16_t kGhostTransparent = 0x{KEY:04X};",
    f"constexpr uint8_t kGhostFacingFrameCount = {len(left)};",
    "",
]

for prefix,frames in [("Left",left),("Right",right)]:
    for n,frame in enumerate(frames):
        lines.append(f"static const uint16_t PROGMEM kGhost{prefix}{n}[kGhostWidth * kGhostHeight] = {{")
        for i in range(0,len(frame),16):
            lines.append("  "+", ".join(f"0x{x:04X}" for x in frame[i:i+16])+",")
        lines += ["};",""]

lines.append("static const uint16_t* const kGhostLeftFrames[kGhostFacingFrameCount] = {")
for n in range(len(left)):
    lines.append(f"  kGhostLeft{n},")
lines.append("};")
lines.append("static const uint16_t* const kGhostRightFrames[kGhostFacingFrameCount] = {")
for n in range(len(right)):
    lines.append(f"  kGhostRight{n},")
lines += ["};","}  // namespace specter::assets",""]

OUT.write_text("\n".join(lines),encoding="utf-8")
print("Wrote",OUT)
