#!/usr/bin/env python3
# This file is part of ProGearSDK.
# Copyright (c) 2024-2025 ProGearSDK contributors
# SPDX-License-Identifier: MIT

"""
Shared utilities for NeoGeo S-ROM (fix layer) generation.

S-ROM tile format:
- 8x8 pixels, 4bpp (16 colors per tile)
- 32 bytes per tile
- Column-based storage with interleaved column pairs

Address bits within a tile: HCLLL
  H = half (0=right columns 4-7, 1=left columns 0-3)
  C = column pair within half (0=outer, 1=inner)
  L = line (0-7)

Byte order in each 32-byte tile:
  Bytes 0-7:   H=0,C=0 -> columns 4,5, lines 0-7
  Bytes 8-15:  H=0,C=1 -> columns 6,7, lines 0-7
  Bytes 16-23: H=1,C=0 -> columns 0,1, lines 0-7
  Bytes 24-31: H=1,C=1 -> columns 2,3, lines 0-7

Each byte holds two pixels: left pixel in bits 0-3, right pixel in bits 4-7.
"""


def pixels_to_srom_tile(pixels_8x8):
    """
    Convert 8x8 pixel array to 32-byte NeoGeo S-ROM tile format.

    Args:
        pixels_8x8: 2D array [row][col] of pixel values (0-15).
                    Row 0 is top, column 0 is left.

    Returns:
        bytes: 32-byte S-ROM tile data
    """
    tile = bytearray(32)

    # Column pairs in storage order: (4,5), (6,7), (0,1), (2,3)
    col_pairs = [(4, 5), (6, 7), (0, 1), (2, 3)]

    for pair_idx, (col_left, col_right) in enumerate(col_pairs):
        for row in range(8):
            byte_idx = pair_idx * 8 + row
            left_pixel = pixels_8x8[row][col_left] & 0xF
            right_pixel = pixels_8x8[row][col_right] & 0xF
            tile[byte_idx] = left_pixel | (right_pixel << 4)

    return bytes(tile)


def bitmap_to_pixels(bitmap_8x8):
    """
    Convert 1bpp bitmap to 8x8 pixel array.

    Args:
        bitmap_8x8: List of 8 bytes, one per row.
                    MSB is leftmost pixel (column 0).
                    Bit value 1 becomes pixel value 1.

    Returns:
        list: 2D array [row][col] of pixel values (0 or 1)
    """
    pixels = []
    for row_byte in bitmap_8x8:
        row = []
        for col in range(8):
            # MSB (bit 7) is column 0, so shift accordingly
            pixel = 1 if (row_byte & (0x80 >> col)) else 0
            row.append(pixel)
        pixels.append(row)
    return pixels
