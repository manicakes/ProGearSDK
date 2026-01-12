/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 *
 * Optimizations applied per NeoGeo dev wiki:
 * - Post-increment (An)+ for memory traversal
 * - Loop unrolling for fixed-size palette operations
 * - Minimized loop overhead for 16-color palettes
 */

#include <palette.h>

void NGPalSetColor(u8 palette, u8 index, NGColor color) {
    volatile u16 *pal = NGPalGetColorPtr(palette, index);
    *pal = color;
}

NGColor NGPalGetColor(u8 palette, u8 index) {
    volatile u16 *pal = NGPalGetColorPtr(palette, index);
    return *pal;
}

/**
 * Set all 16 colors in a palette.
 * Optimized with loop unrolling (4x) for reduced overhead.
 * 16 colors = 4 iterations of 4 writes each.
 */
void NGPalSet(u8 palette, const NGColor colors[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);
    const NGColor *src = colors;

    /* Unrolled 4x: 16 colors / 4 = 4 iterations
     * Post-increment (An)+ is faster than indexed addressing for sequential access */
    pal[0] = src[0];
    pal[1] = src[1];
    pal[2] = src[2];
    pal[3] = src[3];
    pal[4] = src[4];
    pal[5] = src[5];
    pal[6] = src[6];
    pal[7] = src[7];
    pal[8] = src[8];
    pal[9] = src[9];
    pal[10] = src[10];
    pal[11] = src[11];
    pal[12] = src[12];
    pal[13] = src[13];
    pal[14] = src[14];
    pal[15] = src[15];
}

/**
 * Copy one palette to another.
 * Uses fully unrolled copy for 16 colors - eliminates loop overhead entirely.
 */
void NGPalCopy(u8 dst_palette, u8 src_palette) {
    volatile u16 *dst = NGPalGetPtr(dst_palette);
    volatile u16 *src = NGPalGetPtr(src_palette);

    /* Fully unrolled for maximum speed on fixed 16-color palette */
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst[4] = src[4];
    dst[5] = src[5];
    dst[6] = src[6];
    dst[7] = src[7];
    dst[8] = src[8];
    dst[9] = src[9];
    dst[10] = src[10];
    dst[11] = src[11];
    dst[12] = src[12];
    dst[13] = src[13];
    dst[14] = src[14];
    dst[15] = src[15];
}

void NGPalFill(u8 palette, u8 start_idx, u8 count, NGColor color) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 0; i < count && (start_idx + i) < NG_PAL_SIZE; i++) {
        pal[start_idx + i] = color;
    }
}

void NGPalClear(u8 palette) {
    volatile u16 *pal = NGPalGetPtr(palette);
    pal[0] = NG_COLOR_REFERENCE;
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NG_COLOR_BLACK;
    }
}

void NGPalGradient(u8 palette, u8 start_idx, u8 end_idx, NGColor start_color, NGColor end_color) {
    if (start_idx >= NG_PAL_SIZE || end_idx >= NG_PAL_SIZE)
        return;
    if (start_idx > end_idx) {
        u8 tmp = start_idx;
        start_idx = end_idx;
        end_idx = tmp;
        NGColor tc = start_color;
        start_color = end_color;
        end_color = tc;
    }

    volatile u16 *pal = NGPalGetPtr(palette);
    u8 steps = end_idx - start_idx;

    if (steps == 0) {
        pal[start_idx] = start_color;
        return;
    }

    for (u8 i = 0; i <= steps; i++) {
        u8 ratio = (u8)((i * 255) / steps);
        pal[start_idx + i] = NGColorBlend(start_color, end_color, ratio);
    }
}

void NGPalGradientToBlack(u8 palette, u8 start_idx, u8 end_idx, NGColor color) {
    NGPalGradient(palette, start_idx, end_idx, color, NG_COLOR_BLACK);
}

void NGPalGradientToWhite(u8 palette, u8 start_idx, u8 end_idx, NGColor color) {
    NGPalGradient(palette, start_idx, end_idx, color, NG_COLOR_WHITE);
}

void NGPalFadeToBlack(u8 palette, u8 amount) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorDarken(pal[i], amount);
    }
}

void NGPalFadeToWhite(u8 palette, u8 amount) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorLighten(pal[i], amount);
    }
}

void NGPalFadeToColor(u8 palette, NGColor target, u8 amount) {
    if (amount > 31)
        amount = 31;
    volatile u16 *pal = NGPalGetPtr(palette);
    u8 ratio = amount * 8;
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorBlend(pal[i], target, ratio);
    }
}

/**
 * Backup a palette to RAM buffer.
 * Fully unrolled for 16 colors to eliminate loop overhead.
 */
void NGPalBackup(u8 palette, NGColor buffer[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);

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

/**
 * Restore a palette from RAM buffer.
 * Fully unrolled for 16 colors to eliminate loop overhead.
 */
void NGPalRestore(u8 palette, const NGColor buffer[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);

    pal[0] = buffer[0];
    pal[1] = buffer[1];
    pal[2] = buffer[2];
    pal[3] = buffer[3];
    pal[4] = buffer[4];
    pal[5] = buffer[5];
    pal[6] = buffer[6];
    pal[7] = buffer[7];
    pal[8] = buffer[8];
    pal[9] = buffer[9];
    pal[10] = buffer[10];
    pal[11] = buffer[11];
    pal[12] = buffer[12];
    pal[13] = buffer[13];
    pal[14] = buffer[14];
    pal[15] = buffer[15];
}

void NGPalSetupShaded(u8 palette, NGColor base_color) {
    volatile u16 *pal = NGPalGetPtr(palette);

    pal[0] = NG_COLOR_REFERENCE;
    pal[1] = base_color;

    for (u8 i = 2; i < NG_PAL_SIZE; i++) {
        u8 darken_amount = (u8)((i - 1) * 2);
        pal[i] = NGColorDarken(base_color, darken_amount);
    }
}

void NGPalSetupGrayscale(u8 palette) {
    volatile u16 *pal = NGPalGetPtr(palette);

    pal[0] = NG_COLOR_REFERENCE;

    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        u8 level = (u8)(31 - ((i - 1) * 2));
        pal[i] = NGColorGray(level);
    }
}

#define NG_BACKDROP_ADDR 0x401FFE

void NGPalSetBackdrop(NGColor color) {
    *(volatile u16 *)NG_BACKDROP_ADDR = color;
}

NGColor NGPalGetBackdrop(void) {
    return *(volatile u16 *)NG_BACKDROP_ADDR;
}

void NGPalInitDefault(void) {
    volatile u16 *pal = NGPalGetPtr(0);

    pal[0] = NG_COLOR_REFERENCE;
    pal[1] = NG_COLOR_WHITE;
    pal[2] = NG_COLOR_BLACK;
    pal[3] = NG_COLOR_GRAY;
    pal[4] = NG_COLOR_GRAY_LIGHT;
    pal[5] = NG_COLOR_RED;
    pal[6] = NG_COLOR_GREEN;
    pal[7] = NG_COLOR_BLUE;
    pal[8] = NG_COLOR_YELLOW;
    pal[9] = NG_COLOR_CYAN;
    pal[10] = NG_COLOR_MAGENTA;
    pal[11] = NG_COLOR_ORANGE;

    for (u8 i = 12; i < NG_PAL_SIZE; i++) {
        pal[i] = NG_COLOR_BLACK;
    }
}
