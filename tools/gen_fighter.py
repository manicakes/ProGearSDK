#!/usr/bin/env python3
"""
gen_fighter.py - Brawler art: the player walk cycle, and the ground tile.

The walk cycle comes from assets/fighter_walk.gif, a 16-frame animation drawn
at 4x. This script recovers the native 64x128 pixels (a plain nearest-neighbour
downscale - every 4x4 block in the source is a solid colour, so nothing is
resampled) and fits it to the hardware.

Two things need doing beyond the downscale:

Colour. The source has 139 colours and a NeoGeo palette holds 15 plus
transparent. The asset pipeline's own reducer keeps the 15 *most frequent*
colours and snaps everything else to the nearest survivor, which on artwork
this smoothly shaded would spend the whole palette on near-identical mid-greys
and lose the highlights and skin entirely. So the reduction happens here, with
median cut over the colours actually present, and the result is written out
already snapped to the 5-bit channels the hardware stores. The pipeline then
finds exactly 15 colours and passes them through untouched.

Enemies. They are the same tiles under a different palette, so the recolour has
to be authored against the pipeline's index assignment, which is by descending
pixel count. This script derives it from the quantised palette and prints it in
that order, ready to paste into assets.yaml. Armour (near-grey) is tinted; skin
(warm, saturated) is left alone so an enemy still reads as a person.

Usage:
    python3 gen_fighter.py -o demos/showcase/assets --report
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageSequence

SRC_GIF = "fighter_walk.gif"
SRC_SCALE = 4  # the GIF is drawn at 4x
FRAME_W, FRAME_H = 64, 128
PALETTE_SIZE = 15  # plus transparent

# The GIF is 16 frames of walk and nothing else, but idle and punch need frame
# indices of their own: boxes are declared per frame, so a punch sharing a walk
# frame's index would arm its hitbox every time the player took a step. Both are
# copies of the passing pose - the one frame with the legs together, so it reads
# as a stance rather than a step - until there is real art for them.
WALK_FRAMES = 16
NEUTRAL_FRAME = 10

# Skin is the only warm thing on the figure - the armour is neutral or slightly
# cool - so red minus blue separates them cleanly. It is about 3% of the pixels,
# far too little to survive a median cut run over everything at once, so it gets
# its own share of the palette.
SKIN_WARMTH = 30
SKIN_COLORS = 3
ARMOR_COLORS = 12

# Enemy recolour. Scaling a colour's brightness along this ramp keeps the
# original shading intact - black stays black, highlights stay bright - and only
# swings the hue, which a straight blend towards a tint colour would not do
# (it washes the darks out to muddy red).
ENEMY_RAMP = (1.0, 0.42, 0.30)


def snap5(c):
    """Snap a channel to the 5 bits the hardware keeps, as the pipeline does."""
    return (c >> 3) << 3


def native_frames(path):
    """The GIF's 16 frames at their true pixel size."""
    gif = Image.open(path)
    frames = []
    for f in ImageSequence.Iterator(gif):
        rgba = f.convert("RGBA")
        w, h = rgba.size
        frames.append(rgba.resize((w // SRC_SCALE, h // SRC_SCALE), Image.NEAREST))
    return frames


def is_skin(rgb):
    return rgb[0] - rgb[2] > SKIN_WARMTH


def median_cut(pixels, budget):
    """The `budget` colours that best represent `pixels`, snapped to 5 bits."""
    if not pixels or budget <= 0:
        return []
    flat = Image.new("RGB", (len(pixels), 1))
    flat.putdata(pixels)
    raw = flat.quantize(colors=budget, method=Image.MEDIANCUT).getpalette()
    return [tuple(snap5(c) for c in raw[i * 3:i * 3 + 3]) for i in range(budget)]


def quantise(frames):
    """
    Reduce every opaque pixel across all frames to at most PALETTE_SIZE colours.

    Only opaque pixels are considered, so the transparent majority of the canvas
    cannot skew the split, and skin and armour are cut separately against fixed
    budgets so the face is not averaged away into the armour it is outnumbered by.
    """
    opaque = [px[:3] for f in frames for px in f.getdata() if px[3] >= 128]
    skin = [p for p in opaque if is_skin(p)]
    armor = [p for p in opaque if not is_skin(p)]

    palette = median_cut(skin, SKIN_COLORS) + median_cut(armor, ARMOR_COLORS)

    # Snapping to 5 bits can collapse two entries onto each other; dropping the
    # duplicate is fine, it just means the art needed fewer colours.
    seen, unique = set(), []
    for c in palette:
        if c not in seen:
            seen.add(c)
            unique.append(c)
    return unique


def nearest(color, palette):
    """
    Closest palette entry, matched within the colour's own class.

    Plain nearest-neighbour would let a dark skin pixel land on a grey, since
    the shadowed side of the face sits closer to the armour in RGB than to the
    lit side of the face. Keeping skin on skin holds the face together.
    """
    same = [p for p in palette if is_skin(p) == is_skin(color)] or palette
    return min(same, key=lambda p: sum((a - b) ** 2 for a, b in zip(color, p)))


def remap(frame, palette, cache):
    """Redraw a frame using only palette colours, preserving alpha."""
    out = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    pixels = []
    for px in frame.getdata():
        if px[3] < 128:
            pixels.append((0, 0, 0, 0))
            continue
        key = px[:3]
        if key not in cache:
            cache[key] = nearest(key, palette)
        pixels.append(cache[key] + (255,))
    out.putdata(pixels)
    return out


def enemy_color(rgb):
    """Swing armour along the enemy ramp; leave skin alone so it still reads."""
    if is_skin(rgb):
        return rgb
    r, g, b = rgb
    level = (r * 77 + g * 151 + b * 28) >> 8  # perceived brightness
    return tuple(snap5(min(255, int(level * k))) for k in ENEMY_RAMP)


def to_neogeo(rgb):
    """5-bit RGB packed the way the hardware wants it (see progear_assets.py)."""
    r, g, b = (c >> 3 for c in rgb)
    return ((r & 1) << 14 | (g & 1) << 13 | (b & 1) << 12
            | (r >> 1) << 8 | (g >> 1) << 4 | (b >> 1))


def ground_tile():
    """16x16 paving tile: flat base, darker seams, a couple of flecks."""
    img = Image.new("RGBA", (16, 16), (96, 84, 76, 255))
    d = ImageDraw.Draw(img)
    d.line([(0, 0), (15, 0)], fill=(72, 62, 56, 255))
    d.line([(0, 0), (0, 15)], fill=(72, 62, 56, 255))
    d.point([(4, 6), (11, 9), (7, 12)], fill=(120, 108, 98, 255))
    return img


def main():
    ap = argparse.ArgumentParser(description="Generate brawler art")
    ap.add_argument("-o", "--outdir", required=True)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    src = os.path.join(args.outdir, SRC_GIF)
    frames = native_frames(src)

    if len(frames) != WALK_FRAMES:
        print(f"ERROR: expected {WALK_FRAMES} walk frames, got {len(frames)}",
              file=sys.stderr)
        return 1

    if frames[0].size != (FRAME_W, FRAME_H):
        print(f"ERROR: expected {FRAME_W}x{FRAME_H} native frames, got "
              f"{frames[0].size[0]}x{frames[0].size[1]}", file=sys.stderr)
        return 1

    palette = quantise(frames)
    cache = {}
    frames = [remap(f, palette, cache) for f in frames]

    # 0..15 walk, 16 idle, 17 punch
    frames.append(frames[NEUTRAL_FRAME].copy())
    frames.append(frames[NEUTRAL_FRAME].copy())

    strip = Image.new("RGBA", (FRAME_W * len(frames), FRAME_H), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        strip.paste(f, (i * FRAME_W, 0), f)
    strip.save(os.path.join(args.outdir, "fighter.png"))
    ground_tile().save(os.path.join(args.outdir, "ground.png"))

    # Rank by pixel count: this is the order the pipeline assigns indices in,
    # so the enemy palette has to be emitted the same way.
    counts = {}
    for px in strip.getdata():
        if px[3] >= 128:
            counts[px[:3]] = counts.get(px[:3], 0) + 1
    order = [c for c, _ in sorted(counts.items(), key=lambda kv: -kv[1])]

    if len(order) > PALETTE_SIZE:
        print(f"ERROR: {len(order)} colours survived, hardware allows "
              f"{PALETTE_SIZE}", file=sys.stderr)
        return 1

    if args.report:
        print(f"fighter.png {strip.size[0]}x{strip.size[1]} "
              f"({len(frames)} frames of {FRAME_W}x{FRAME_H}: "
              f"0-{WALK_FRAMES - 1} walk, {WALK_FRAMES} idle, "
              f"{WALK_FRAMES + 1} punch)")
        print(f"palette: {len(order)} colours\n")
        for i, c in enumerate(order, start=1):
            kind = "skin " if is_skin(c) else "armor"
            print(f"  {i:2d} {kind} rgb{c!s:<18} {counts[c]:6d} px  "
                  f"-> enemy rgb{enemy_color(c)}")
        print("\nfighter_enemy palette for assets.yaml:")
        vals = [0x8000] + [to_neogeo(enemy_color(c)) for c in order]
        vals += [0x0000] * (16 - len(vals))
        for row in range(2):
            line = ", ".join(f"0x{v:04X}" for v in vals[row * 8:row * 8 + 8])
            prefix = "    colors: [" if row == 0 else "             "
            print(f"{prefix}{line}" + ("," if row == 0 else "]"))
        print("\nground.png  16x16")

    return 0


if __name__ == "__main__":
    sys.exit(main())
