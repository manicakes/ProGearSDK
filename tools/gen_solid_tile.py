#!/usr/bin/env python3
# This file is part of ProGearSDK.
# Copyright (c) 2024-2025 ProGearSDK contributors
# SPDX-License-Identifier: MIT

"""
Generate a simple solid color test tile for debugging C-ROM format.
Creates a 16x16 tile where all pixels are color index 1.

MAME interleaves C1 and C2 byte-by-byte:
  Final[0] = C1[0], Final[1] = C2[0], Final[2] = C1[1], Final[3] = C2[1]...

So to get bp0, bp1, bp2, bp3 in final memory:
  C1 should contain: bp0, bp2, bp0, bp2, ...
  C2 should contain: bp1, bp3, bp1, bp3, ...
"""

import sys

def main():
    c1_path = sys.argv[1] if len(sys.argv) > 1 else 'test-c1.bin'
    c2_path = sys.argv[2] if len(sys.argv) > 2 else 'test-c2.bin'

    # For a solid tile with all pixels = color index 1:
    # Color 1 in binary = 0001
    # bp0 = 1, bp1 = 0, bp2 = 0, bp3 = 0
    # So bp0 = 0xFF (all pixels have bit 0 set)
    # bp1 = bp2 = bp3 = 0x00

    # BIOS uses tiles 0-255, so we put our tile at index 256
    TILE_INDEX = 256
    TILE_SIZE = 64  # bytes per tile
    TILE_OFFSET = TILE_INDEX * TILE_SIZE  # 16384

    # Need enough space for tile 256 (at least 256*64 + 64 = 16448 bytes)
    c1_data = bytearray(TILE_OFFSET + TILE_SIZE)
    c2_data = bytearray(TILE_OFFSET + TILE_SIZE)

    # After MAME interleaving, we want: bp0, bp1, bp2, bp3
    # MAME produces: C1[0], C2[0], C1[1], C2[1]
    # So: C1[0]=bp0, C2[0]=bp1, C1[1]=bp2, C2[1]=bp3
    #
    # C1 stores: bp0, bp2, bp0, bp2, ...
    # C2 stores: bp1, bp3, bp1, bp3, ...

    for i in range(0, TILE_SIZE, 2):
        c1_data[TILE_OFFSET + i] = 0xFF      # bp0 = all 1s (color bit 0)
        c1_data[TILE_OFFSET + i + 1] = 0x00  # bp2 = all 0s
        c2_data[TILE_OFFSET + i] = 0x00      # bp1 = all 0s
        c2_data[TILE_OFFSET + i + 1] = 0x00  # bp3 = all 0s

    # Pad to 64KB minimum
    while len(c1_data) < 65536:
        c1_data.append(0)
    while len(c2_data) < 65536:
        c2_data.append(0)

    with open(c1_path, 'wb') as f:
        f.write(c1_data)
    with open(c2_path, 'wb') as f:
        f.write(c2_data)

    print(f"Generated {c1_path} and {c2_path}")
    print(f"Tile {TILE_INDEX} = solid color 1 (should be orange)")


if __name__ == '__main__':
    main()
