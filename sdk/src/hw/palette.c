/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/palette.h>

/*
 * Fully unrolled palette operations for maximum speed.
 * 16 colors × 2 bytes = 32 bytes per palette.
 * Unrolling eliminates loop overhead entirely.
 */

void hw_palette_set(u8 index, const u16 *colors) {
    vu16 *pal = hw_palette_ptr(index);

    pal[0] = colors[0];
    pal[1] = colors[1];
    pal[2] = colors[2];
    pal[3] = colors[3];
    pal[4] = colors[4];
    pal[5] = colors[5];
    pal[6] = colors[6];
    pal[7] = colors[7];
    pal[8] = colors[8];
    pal[9] = colors[9];
    pal[10] = colors[10];
    pal[11] = colors[11];
    pal[12] = colors[12];
    pal[13] = colors[13];
    pal[14] = colors[14];
    pal[15] = colors[15];
}

void hw_palette_backup(u8 index, u16 *buffer) {
    vu16 *pal = hw_palette_ptr(index);

    buffer[0] = pal[0];
    buffer[1] = pal[1];
    buffer[2] = pal[2];
    buffer[3] = pal[3];
    buffer[4] = pal[4];
    buffer[5] = pal[5];
    buffer[6] = pal[6];
    buffer[7] = pal[7];
    buffer[8] = pal[8];
    buffer[9] = pal[9];
    buffer[10] = pal[10];
    buffer[11] = pal[11];
    buffer[12] = pal[12];
    buffer[13] = pal[13];
    buffer[14] = pal[14];
    buffer[15] = pal[15];
}

void hw_palette_restore(u8 index, const u16 *buffer) {
    hw_palette_set(index, buffer);
}

void hw_palette_copy(u8 dst, u8 src) {
    vu16 *d = hw_palette_ptr(dst);
    vu16 *s = hw_palette_ptr(src);

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
    d[4] = s[4];
    d[5] = s[5];
    d[6] = s[6];
    d[7] = s[7];
    d[8] = s[8];
    d[9] = s[9];
    d[10] = s[10];
    d[11] = s[11];
    d[12] = s[12];
    d[13] = s[13];
    d[14] = s[14];
    d[15] = s[15];
}

void hw_palette_fill(u8 index, u8 start, u8 count, u16 color) {
    vu16 *pal = hw_palette_ptr(index);

    for (u8 i = 0; i < count && (start + i) < PALETTE_SIZE; i++) {
        pal[start + i] = color;
    }
}

void hw_palette_clear(u8 index) {
    vu16 *pal = hw_palette_ptr(index);

    /* Color 0 is reference (transparent) - use dark bit */
    pal[0] = COLOR_DARK_BIT;

    /* Colors 1-15 are black */
    pal[1] = 0;
    pal[2] = 0;
    pal[3] = 0;
    pal[4] = 0;
    pal[5] = 0;
    pal[6] = 0;
    pal[7] = 0;
    pal[8] = 0;
    pal[9] = 0;
    pal[10] = 0;
    pal[11] = 0;
    pal[12] = 0;
    pal[13] = 0;
    pal[14] = 0;
    pal[15] = 0;
}
