#!/usr/bin/env python3
"""
gen_fighter.py - Placeholder brawler art: a boxy humanoid and a ground tile.

Deliberately plain. The figure is built from rectangles - head, torso, two
fists, two legs - so a punch reads clearly at a glance and the walk cycle is
obvious without any real animation work.

Three opaque colours only, and the pipeline assigns palette indices by
descending pixel frequency, so the ordering is tuned the same way as the ball:

    torso/limbs (most) -> 1, outline -> 2, skin (fewest) -> 3

The recolour palettes in assets.yaml are authored to match that order, so an
enemy is a palette swap of the same tiles rather than separate art.

Frames: 0 idle, 1 walk A, 2 walk B, 3 punch.

Usage:
    python3 gen_fighter.py -o demos/showcase/assets --report
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw

W, H = 32, 48
CLEAR = (0, 0, 0, 0)

BODY = (72, 108, 200, 255)    # most pixels -> palette index 1
LINE = (24, 24, 40, 255)       # -> index 2
SKIN = (240, 190, 140, 255)    # fewest -> index 3


def figure(frame):
    """One 32x48 frame. Boxes only, outlined so shapes stay legible."""
    img = Image.new("RGBA", (W, H), CLEAR)
    d = ImageDraw.Draw(img)

    def box(x0, y0, x1, y1, fill):
        d.rectangle([x0, y0, x1, y1], fill=fill, outline=LINE)

    walk_a = (frame == 1)
    walk_b = (frame == 2)
    punch = (frame == 3)

    # Head, with a slight bob on one walk frame so the cycle reads
    head_y = 3 + (1 if walk_b else 0)
    box(11, head_y, 20, head_y + 9, SKIN)

    # Torso
    box(9, head_y + 10, 22, head_y + 26, BODY)

    # Arms. The punching arm extends forward with a bigger fist; the other
    # stays tucked, so the difference is unmistakable.
    arm_y = head_y + 13
    if punch:
        box(23, arm_y - 1, 30, arm_y + 6, SKIN)   # extended fist
        box(22, arm_y + 1, 24, arm_y + 4, BODY)   # forearm
        box(5, arm_y + 2, 9, arm_y + 7, SKIN)     # rear fist
    else:
        box(5, arm_y + 2, 9, arm_y + 7, SKIN)
        box(22, arm_y + 2, 26, arm_y + 7, SKIN)

    # Legs. Split apart on walk A, together on walk B and idle.
    leg_top = head_y + 27
    if walk_a:
        box(8, leg_top, 13, H - 2, BODY)
        box(18, leg_top, 23, H - 2, BODY)
    elif walk_b:
        box(10, leg_top, 15, H - 2, BODY)
        box(16, leg_top, 21, H - 2, BODY)
    else:
        box(10, leg_top, 14, H - 2, BODY)
        box(17, leg_top, 21, H - 2, BODY)

    return img


def ground_tile():
    """16x16 paving tile: flat base, darker seams, a couple of flecks."""
    img = Image.new("RGBA", (16, 16), (96, 84, 76, 255))
    d = ImageDraw.Draw(img)
    d.line([(0, 0), (15, 0)], fill=(72, 62, 56, 255))
    d.line([(0, 0), (0, 15)], fill=(72, 62, 56, 255))
    d.point([(4, 6), (11, 9), (7, 12)], fill=(120, 108, 98, 255))
    return img


def main():
    ap = argparse.ArgumentParser(description="Generate brawler placeholder art")
    ap.add_argument("-o", "--outdir", required=True)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    frames = [figure(i) for i in range(4)]
    strip = Image.new("RGBA", (W * len(frames), H), CLEAR)
    for i, f in enumerate(frames):
        strip.paste(f, (i * W, 0), f)
    strip.save(os.path.join(args.outdir, "fighter.png"))
    ground_tile().save(os.path.join(args.outdir, "ground.png"))

    counts = {}
    for px in strip.getdata():
        if px[3] >= 128:
            counts[px[:3]] = counts.get(px[:3], 0) + 1
    order = sorted(counts.items(), key=lambda kv: -kv[1])

    if args.report:
        names = {BODY[:3]: "body", LINE[:3]: "outline", SKIN[:3]: "skin"}
        print(f"fighter.png {strip.size[0]}x{strip.size[1]} ({len(frames)} frames)")
        for i, (c, n) in enumerate(order, start=1):
            print(f"  palette index {i} = {names.get(c, '?'):8s} rgb{c}  {n} px")
        print("ground.png  16x16")

    if [c for c, _ in order] != [BODY[:3], LINE[:3], SKIN[:3]]:
        print("ERROR: frequency order changed; the recolour palettes in assets.yaml "
              "are authored for body > outline > skin and would be wrong.",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
