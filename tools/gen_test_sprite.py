#!/usr/bin/env python3
# This file is part of ProGearSDK.
# Copyright (c) 2024-2025 ProGearSDK contributors
# SPDX-License-Identifier: MIT

"""
Generate a simple test sprite for testing the asset pipeline.
Creates a 64x16 sprite sheet with 4 frames of a spinning ball.
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Error: Pillow required. Install with: pip install pillow", file=sys.stderr)
    sys.exit(1)


def main():
    output_path = sys.argv[1] if len(sys.argv) > 1 else 'ball.png'

    # Create 64x16 image (4 frames of 16x16)
    img = Image.new('RGBA', (64, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Colors
    ball_color = (255, 128, 0)  # Orange
    highlight = (255, 200, 100)
    shadow = (180, 80, 0)

    for frame in range(4):
        x_offset = frame * 16

        # Draw ball (filled circle)
        draw.ellipse([x_offset + 2, 2, x_offset + 13, 13], fill=ball_color)

        # Add rotating highlight based on frame
        highlight_pos = [
            (x_offset + 4, 4),
            (x_offset + 10, 4),
            (x_offset + 10, 10),
            (x_offset + 4, 10),
        ]
        hx, hy = highlight_pos[frame]
        draw.ellipse([hx, hy, hx + 3, hy + 3], fill=highlight)

        # Add shadow on opposite side
        shadow_pos = [
            (x_offset + 9, 9),
            (x_offset + 3, 9),
            (x_offset + 3, 3),
            (x_offset + 9, 3),
        ]
        sx, sy = shadow_pos[frame]
        draw.ellipse([sx, sy, sx + 2, sy + 2], fill=shadow)

    img.save(output_path)
    print(f"Generated {output_path}")


if __name__ == '__main__':
    main()
