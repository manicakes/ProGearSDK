#!/usr/bin/env python3
"""
gen_demo_sprites.py - Placeholder sprites for the tilemap demo's hitbox showcase.

Deliberately plain: flat shapes that read clearly at 16-32px so the demo is
about the collision behaviour, not the art. Two opaque colours each, which
keeps the palette pressure low.

Usage:
    python3 gen_demo_sprites.py -o demos/showcase/assets
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw

CLEAR = (0, 0, 0, 0)


def bullet():
    """8x8 pellet, drawn in a 16x16 cell so it tiles cleanly."""
    img = Image.new("RGBA", (16, 16), CLEAR)
    d = ImageDraw.Draw(img)
    d.ellipse([4, 5, 11, 10], fill=(255, 240, 120, 255))
    d.ellipse([5, 6, 8, 9], fill=(255, 255, 220, 255))
    return img


def walker_frames():
    """
    32x32 enemy, two frames so the legs alternate.
    A squat body with eyes, so 'stomped from above' reads naturally.
    """
    frames = []
    for step in (0, 1):
        img = Image.new("RGBA", (32, 32), CLEAR)
        d = ImageDraw.Draw(img)
        body = (200, 60, 60, 255)
        trim = (255, 170, 170, 255)

        d.rectangle([6, 10, 25, 26], fill=body)       # body
        d.rectangle([9, 6, 22, 11], fill=body)        # head bump
        d.rectangle([10, 13, 14, 17], fill=trim)      # eyes
        d.rectangle([17, 13, 21, 17], fill=trim)
        # Feet alternate so the walk cycle is visible
        if step == 0:
            d.rectangle([6, 27, 13, 30], fill=trim)
            d.rectangle([18, 27, 25, 29], fill=trim)
        else:
            d.rectangle([6, 27, 13, 29], fill=trim)
            d.rectangle([18, 27, 25, 30], fill=trim)
        frames.append(img)
    return frames


def main():
    ap = argparse.ArgumentParser(description="Generate placeholder demo sprites")
    ap.add_argument("-o", "--outdir", required=True)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    b = bullet()
    b.save(os.path.join(args.outdir, "bullet.png"))

    frames = walker_frames()
    strip = Image.new("RGBA", (32 * len(frames), 32), CLEAR)
    for i, f in enumerate(frames):
        strip.paste(f, (i * 32, 0), f)
    strip.save(os.path.join(args.outdir, "walker.png"))

    if args.report:
        print(f"bullet.png  {b.size[0]}x{b.size[1]}")
        print(f"walker.png  {strip.size[0]}x{strip.size[1]} ({len(frames)} frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
