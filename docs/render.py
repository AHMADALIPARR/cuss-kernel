#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Render docs/session.txt into VGA-styled terminal screenshots."""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "session.txt")
OUT = os.path.join(HERE, "screenshots")

FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 22)
BOLD = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 22)

BG = (5, 5, 8)
FG = (170, 170, 170)      # VGA 0x07 light gray
GREEN = (0, 170, 0)       # VGA 0x0A green
WHITE = (230, 230, 230)
PROMPT_C = (255, 255, 255)

SHOTS = [
    ("boot.png", 1, 22),
    ("demos.png", 23, 40),
    ("regs.png", 41, 60),
]


def draw(lines):
    ascent, descent = FONT.getmetrics()
    lh = ascent + descent + 6
    # measure widest line
    maxw = 0
    for ln in lines:
        w = FONT.getbbox(ln)[2] if ln else 0
        maxw = max(maxw, w)
    pad = 28
    img = Image.new("RGB", (maxw + pad * 2, lh * len(lines) + pad * 2), BG)
    d = ImageDraw.Draw(img)
    y = pad
    for i, ln in enumerate(lines):
        if i == 0 and ln.startswith("cuss-kernel v0.1"):
            col, fnt = GREEN, BOLD
        elif ln.startswith("cuss>"):
            col, fnt = WHITE, BOLD
            # prompt part bright, rest normal
            d.text((pad, y), "cuss>", font=BOLD, fill=PROMPT_C)
            rest = ln[5:]
            if rest:
                d.text((pad + BOLD.getbbox("cuss>")[2], y), rest, font=FONT, fill=FG)
            y += lh
            continue
        elif ln.startswith("[") or "words. running" in ln:
            col, fnt = (120, 200, 120), FONT
        else:
            col, fnt = FG, FONT
        d.text((pad, y), ln, font=fnt, fill=col)
        y += lh
    # subtle scanlines
    px = img.load()
    w, h = img.size
    for yy in range(0, h, 4):
        for xx in range(w):
            r, g, b = px[xx, yy]
            px[xx, yy] = (r * 9 // 10, g * 9 // 10, b * 9 // 10)
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    with open(SRC) as f:
        all_lines = f.read().split("\n")
    for name, a, b in SHOTS:
        seg = all_lines[a - 1:b]
        img = draw(seg)
        img.save(os.path.join(OUT, name))
        print("wrote", name, img.size)


if __name__ == "__main__":
    main()
