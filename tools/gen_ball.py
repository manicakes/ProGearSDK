#!/usr/bin/env python3
"""
gen_ball.py - Generate the spinning basketball sprite strip for the showcase demo.

Renders a shaded sphere with basketball seams rotating about the vertical axis,
as a horizontal strip of 32x32 frames (the layout assets.yaml expects).

Only three opaque colors are used. The asset pipeline assigns palette indices by
descending pixel frequency (build_palette_from_frames in progear_assets.py), and
the ball_* recolor palettes in assets.yaml are written as
index 1 = brightest, 2 = mid, 3 = darkest. So the render is tuned to come out as

    body (most pixels) -> 1, highlight -> 2, seams+rim (fewest) -> 3

which keeps the seams the darkest color on every recolor, not just the default
orange. --report prints the resulting order so a tweak that flips it is caught.

Usage:
    python3 gen_ball.py -o demos/showcase/assets/ball.png --report
"""

import argparse
import math
import sys

from PIL import Image

FRAMES = 16
SIZE = 32
RADIUS = 14.0  # 28px diameter, matching the original asset's bounding box

# Classic basketball: orange body, lighter top-left highlight, near-black seams.
BODY = (255, 140, 0, 255)
HIGHLIGHT = (255, 200, 100, 255)
SEAM = (40, 18, 4, 255)
CLEAR = (0, 0, 0, 0)

# Light direction (screen space, +y is down) - upper left, slightly toward viewer
LIGHT = (-0.45, -0.55, 0.70)

# Seam geometry: three mutually perpendicular great circles give the 8-panel
# basketball pattern - an equator plus two meridians. As the ball spins the
# meridians are seen obliquely and read as the bowed side seams.
SEAM_NORMALS = [
    (0.0, 1.0, 0.0),  # equator
    (1.0, 0.0, 0.0),  # meridian facing the viewer at rest
    (0.0, 0.0, 1.0),  # meridian perpendicular to it
]

SEAM_HALF_WIDTH = 0.07   # angular half-width of a seam band
TILT_DEG = 18.0          # tip the seam system so the equator is not dead level
HIGHLIGHT_THRESHOLD = 0.64
RIM_THRESHOLD = -0.75     # unlit crescent along the lower-right limb


def normalize(v):
    m = math.sqrt(sum(c * c for c in v))
    return tuple(c / m for c in v)


def render_frame(spin):
    """Render one 32x32 frame with the ball rotated `spin` radians about Y."""
    img = Image.new("RGBA", (SIZE, SIZE), CLEAR)
    px = img.load()

    light = normalize(LIGHT)
    cs, sn = math.cos(spin), math.sin(spin)
    tilt = math.radians(TILT_DEG)
    ct, st = math.cos(tilt), math.sin(tilt)

    center = SIZE / 2.0
    for iy in range(SIZE):
        for ix in range(SIZE):
            # Sample at pixel centers so the disc stays symmetric
            nx = (ix + 0.5 - center) / RADIUS
            ny = (iy + 0.5 - center) / RADIUS
            d2 = nx * nx + ny * ny
            if d2 > 1.0:
                continue

            nz = math.sqrt(1.0 - d2)  # front surface of the sphere
            diffuse = nx * light[0] + ny * light[1] + nz * light[2]

            # Screen -> ball-local: undo the tilt about Z, then the spin about Y
            lx = nx * ct + ny * st
            ly = -nx * st + ny * ct
            lz = nz
            bx = lx * cs - lz * sn
            by = ly
            bz = lx * sn + lz * cs

            on_seam = any(
                abs(bx * n[0] + by * n[1] + bz * n[2]) < SEAM_HALF_WIDTH
                for n in SEAM_NORMALS
            )

            if on_seam or diffuse < RIM_THRESHOLD:
                px[ix, iy] = SEAM
            elif diffuse > HIGHLIGHT_THRESHOLD:
                px[ix, iy] = HIGHLIGHT
            else:
                px[ix, iy] = BODY

    return img


def main():
    ap = argparse.ArgumentParser(description="Generate the basketball sprite strip")
    ap.add_argument("-o", "--output", required=True, help="Output PNG path")
    ap.add_argument("--report", action="store_true",
                    help="Print resulting palette index order")
    args = ap.parse_args()

    strip = Image.new("RGBA", (SIZE * FRAMES, SIZE), CLEAR)
    for f in range(FRAMES):
        # The three seam planes are mutually perpendicular, so a quarter turn
        # about Y maps the seam set onto itself - the pattern's period is pi/2,
        # not 2*pi. Sweeping pi/2 across the 16 frames loops seamlessly and
        # keeps every frame distinct; a full turn would emit each frame 4 times.
        strip.paste(render_frame(0.5 * math.pi * f / FRAMES), (f * SIZE, 0))

    strip.save(args.output)

    frames = [strip.crop((f * SIZE, 0, (f + 1) * SIZE, SIZE)).tobytes()
              for f in range(FRAMES)]
    if len(set(frames)) != FRAMES:
        print(f"ERROR: only {len(set(frames))} of {FRAMES} frames are distinct.",
              file=sys.stderr)
        return 1

    counts = {}
    for pixel in strip.getdata():
        if pixel[3] >= 128:
            counts[pixel[:3]] = counts.get(pixel[:3], 0) + 1
    order = sorted(counts.items(), key=lambda kv: -kv[1])

    if args.report:
        names = {BODY[:3]: "body", HIGHLIGHT[:3]: "highlight", SEAM[:3]: "seam"}
        print(f"Wrote {args.output} ({SIZE * FRAMES}x{SIZE}, {FRAMES} frames)")
        for i, (color, n) in enumerate(order, start=1):
            print(f"  palette index {i} = {names.get(color, '?'):9s} rgb{color}  {n} px")

    expected = [BODY[:3], HIGHLIGHT[:3], SEAM[:3]]
    if [c for c, _ in order] != expected:
        print("ERROR: frequency order is not body > highlight > seam; the recolor "
              "palettes in assets.yaml would map seams to a non-darkest index.",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
