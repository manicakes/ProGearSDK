/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// palette.c - Palette management functions

#include <palette.h>

// === Single Color Operations ===

void NGPalSetColor(u8 palette, u8 index, NGColor color) {
    volatile u16 *pal = NGPalGetColorPtr(palette, index);
    *pal = color;
}

NGColor NGPalGetColor(u8 palette, u8 index) {
    volatile u16 *pal = NGPalGetColorPtr(palette, index);
    return *pal;
}

// === Bulk Operations ===

void NGPalSet(u8 palette, const NGColor colors[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 0; i < NG_PAL_SIZE; i++) {
        pal[i] = colors[i];
    }
}

void NGPalCopy(u8 dst_palette, u8 src_palette) {
    volatile u16 *dst = NGPalGetPtr(dst_palette);
    volatile u16 *src = NGPalGetPtr(src_palette);
    for (u8 i = 0; i < NG_PAL_SIZE; i++) {
        dst[i] = src[i];
    }
}

void NGPalFill(u8 palette, u8 start_idx, u8 count, NGColor color) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 0; i < count && (start_idx + i) < NG_PAL_SIZE; i++) {
        pal[start_idx + i] = color;
    }
}

void NGPalClear(u8 palette) {
    volatile u16 *pal = NGPalGetPtr(palette);
    // Set reference color for index 0
    pal[0] = NG_COLOR_REFERENCE;
    // Clear rest to black
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NG_COLOR_BLACK;
    }
}

// === Gradient Generation ===

void NGPalGradient(u8 palette, u8 start_idx, u8 end_idx,
                   NGColor start_color, NGColor end_color) {
    if (start_idx >= NG_PAL_SIZE || end_idx >= NG_PAL_SIZE) return;
    if (start_idx > end_idx) {
        // Swap
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
        u8 ratio = (i * 255) / steps;
        pal[start_idx + i] = NGColorBlend(start_color, end_color, ratio);
    }
}

void NGPalGradientToBlack(u8 palette, u8 start_idx, u8 end_idx, NGColor color) {
    NGPalGradient(palette, start_idx, end_idx, color, NG_COLOR_BLACK);
}

void NGPalGradientToWhite(u8 palette, u8 start_idx, u8 end_idx, NGColor color) {
    NGPalGradient(palette, start_idx, end_idx, color, NG_COLOR_WHITE);
}

// === Fade Effects ===

void NGPalFadeToBlack(u8 palette, u8 amount) {
    volatile u16 *pal = NGPalGetPtr(palette);
    // Skip index 0 (reference color)
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorDarken(pal[i], amount);
    }
}

void NGPalFadeToWhite(u8 palette, u8 amount) {
    volatile u16 *pal = NGPalGetPtr(palette);
    // Skip index 0 (reference color)
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorLighten(pal[i], amount);
    }
}

void NGPalFadeToColor(u8 palette, NGColor target, u8 amount) {
    if (amount > 31) amount = 31;
    volatile u16 *pal = NGPalGetPtr(palette);
    // Convert amount to 0-255 range for blend
    u8 ratio = amount * 8;  // Approximate: 31*8 = 248
    // Skip index 0 (reference color)
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        pal[i] = NGColorBlend(pal[i], target, ratio);
    }
}

// === Palette Backup/Restore ===

void NGPalBackup(u8 palette, NGColor buffer[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 0; i < NG_PAL_SIZE; i++) {
        buffer[i] = pal[i];
    }
}

void NGPalRestore(u8 palette, const NGColor buffer[NG_PAL_SIZE]) {
    volatile u16 *pal = NGPalGetPtr(palette);
    for (u8 i = 0; i < NG_PAL_SIZE; i++) {
        pal[i] = buffer[i];
    }
}

// === Convenience Functions ===

void NGPalSetupShaded(u8 palette, NGColor base_color) {
    volatile u16 *pal = NGPalGetPtr(palette);

    pal[0] = NG_COLOR_REFERENCE;  // Transparent/reference
    pal[1] = base_color;          // Full brightness

    // Generate darkening shades
    for (u8 i = 2; i < NG_PAL_SIZE; i++) {
        u8 darken_amount = (i - 1) * 2;  // Progressively darker
        pal[i] = NGColorDarken(base_color, darken_amount);
    }
}

void NGPalSetupGrayscale(u8 palette) {
    volatile u16 *pal = NGPalGetPtr(palette);

    pal[0] = NG_COLOR_REFERENCE;

    // Generate 15 shades from white to near-black
    for (u8 i = 1; i < NG_PAL_SIZE; i++) {
        u8 level = 31 - ((i - 1) * 2);  // 31, 29, 27, ... 3
        pal[i] = NGColorGray(level);
    }
}

// === Backdrop Color ===

#define NG_BACKDROP_ADDR 0x401FFE

void NGPalSetBackdrop(NGColor color) {
    *(volatile u16*)NG_BACKDROP_ADDR = color;
}

NGColor NGPalGetBackdrop(void) {
    return *(volatile u16*)NG_BACKDROP_ADDR;
}

// === Default Initialization ===

void NGPalInitDefault(void) {
    // Set up palette 0 for basic text display
    // The standard NeoGeo font uses:
    //   Color 0: transparent
    //   Color 1: main text color
    //   Color 2: shadow/outline color
    volatile u16 *pal = NGPalGetPtr(0);

    pal[0] = NG_COLOR_REFERENCE;   // Reference color (required)
    pal[1] = NG_COLOR_WHITE;       // Main text color
    pal[2] = NG_COLOR_BLACK;       // Text shadow (transparent on black background)
    pal[3] = NG_COLOR_GRAY;
    pal[4] = NG_COLOR_GRAY_LIGHT;

    // Some useful colors
    pal[5] = NG_COLOR_RED;
    pal[6] = NG_COLOR_GREEN;
    pal[7] = NG_COLOR_BLUE;
    pal[8] = NG_COLOR_YELLOW;
    pal[9] = NG_COLOR_CYAN;
    pal[10] = NG_COLOR_MAGENTA;
    pal[11] = NG_COLOR_ORANGE;

    // Fill rest with black
    for (u8 i = 12; i < NG_PAL_SIZE; i++) {
        pal[i] = NG_COLOR_BLACK;
    }
}
