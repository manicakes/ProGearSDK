/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_color.h
 * @brief NeoGeo color definitions and utilities.
 *
 * NeoGeo uses a 16-bit color format:
 * - Bit 15: Dark bit (shared LSB for all channels)
 * - Bit 14: R0 (red LSB)
 * - Bit 13: G0 (green LSB)
 * - Bit 12: B0 (blue LSB)
 * - Bits 11-8: R4-R1 (red upper nibble)
 * - Bits 7-4: G4-G1 (green upper nibble)
 * - Bits 3-0: B4-B1 (blue upper nibble)
 *
 * Each channel has 5 bits + 1 shared dark bit = 32 shades per channel.
 */

#ifndef _NG_COLOR_H_
#define _NG_COLOR_H_

#include <ng_types.h>

/**
 * @defgroup color Color Utilities
 * @ingroup hal
 * @brief NeoGeo 16-bit color construction and manipulation.
 * @{
 */

/** NeoGeo 16-bit color type */
typedef u16 NGColor;

/** @name Color Construction */
/** @{ */

/** Build color from 5-bit RGB components (0-31 each) */
#define NG_RGB(r, g, b)                                                                      \
    ((((r) & 1) << 14) | (((g) & 1) << 13) | (((b) & 1) << 12) | ((((r) >> 1) & 0xF) << 8) | \
     ((((g) >> 1) & 0xF) << 4) | (((b) >> 1) & 0xF))

/** Alias for NG_RGB (5-bit components) */
#define NG_RGB5(r, g, b)                                                                     \
    ((((r) & 1) << 14) | (((g) & 1) << 13) | (((b) & 1) << 12) | ((((r) >> 1) & 0xF) << 8) | \
     ((((g) >> 1) & 0xF) << 4) | (((b) >> 1) & 0xF))

/** Build color from 5-bit RGB with dark bit set */
#define NG_RGB5_DARK(r, g, b) (NG_RGB5(r, g, b) | 0x8000)

/** Build color from 4-bit RGB components (0-15 each) */
#define NG_RGB4(r, g, b) ((((r) & 0xF) << 8) | (((g) & 0xF) << 4) | ((b) & 0xF))

/** Build color from 4-bit RGB with dark bit */
#define NG_RGB4_DARK(r, g, b) (NG_RGB4(r, g, b) | 0x8000)

/** Build color from 8-bit RGB (0-255), auto-converts to 5-bit */
#define NG_RGB8(r, g, b) NG_RGB5((r) >> 3, (g) >> 3, (b) >> 3)

/** Build color from 8-bit RGB with dark bit */
#define NG_RGB8_DARK(r, g, b) (NG_RGB8(r, g, b) | 0x8000)
/** @} */

/** @name Color Constants */
/** @{ */

#define NG_COLOR_REFERENCE 0x8000 /**< Reference/backdrop marker */

#define NG_COLOR_BLACK   NG_RGB4(0x0, 0x0, 0x0) /**< Pure black */
#define NG_COLOR_WHITE   NG_RGB4(0xF, 0xF, 0xF) /**< Pure white */
#define NG_COLOR_RED     NG_RGB4(0xF, 0x0, 0x0) /**< Bright red */
#define NG_COLOR_GREEN   NG_RGB4(0x0, 0xF, 0x0) /**< Bright green */
#define NG_COLOR_BLUE    NG_RGB4(0x0, 0x0, 0xF) /**< Bright blue */
#define NG_COLOR_YELLOW  NG_RGB4(0xF, 0xF, 0x0) /**< Yellow */
#define NG_COLOR_CYAN    NG_RGB4(0x0, 0xF, 0xF) /**< Cyan */
#define NG_COLOR_MAGENTA NG_RGB4(0xF, 0x0, 0xF) /**< Magenta */

#define NG_COLOR_DARK_RED     NG_RGB4(0x8, 0x0, 0x0) /**< Dark red */
#define NG_COLOR_DARK_GREEN   NG_RGB4(0x0, 0x8, 0x0) /**< Dark green */
#define NG_COLOR_DARK_BLUE    NG_RGB4(0x0, 0x0, 0x8) /**< Dark blue */
#define NG_COLOR_DARK_YELLOW  NG_RGB4(0x8, 0x8, 0x0) /**< Dark yellow */
#define NG_COLOR_DARK_CYAN    NG_RGB4(0x0, 0x8, 0x8) /**< Dark cyan */
#define NG_COLOR_DARK_MAGENTA NG_RGB4(0x8, 0x0, 0x8) /**< Dark magenta */

#define NG_COLOR_GRAY_DARK  NG_RGB4(0x4, 0x4, 0x4) /**< Dark gray */
#define NG_COLOR_GRAY       NG_RGB4(0x8, 0x8, 0x8) /**< Medium gray */
#define NG_COLOR_GRAY_LIGHT NG_RGB4(0xC, 0xC, 0xC) /**< Light gray */

#define NG_COLOR_ORANGE        NG_RGB4(0xF, 0x8, 0x0) /**< Orange */
#define NG_COLOR_HERMES_ORANGE NG_RGB4(0xF, 0x7, 0x2) /**< Hermes orange */
#define NG_COLOR_PINK          NG_RGB4(0xF, 0x8, 0xC) /**< Pink */
#define NG_COLOR_PURPLE        NG_RGB4(0x8, 0x0, 0xF) /**< Purple */
#define NG_COLOR_BROWN         NG_RGB4(0x8, 0x4, 0x0) /**< Brown */
#define NG_COLOR_DARK_ORANGE   NG_RGB4(0xA, 0x4, 0x0) /**< Dark orange */
#define NG_COLOR_SKY_BLUE      NG_RGB4(0x4, 0x8, 0xF) /**< Sky blue */
#define NG_COLOR_LIME          NG_RGB4(0x8, 0xF, 0x0) /**< Lime green */

#define NG_COLOR_WHITE_BRIGHT NG_RGB4_DARK(0xF, 0xF, 0xF) /**< Brightest white */
#define NG_COLOR_RED_BRIGHT   NG_RGB4_DARK(0xF, 0x0, 0x0) /**< Brightest red */
#define NG_COLOR_GREEN_BRIGHT NG_RGB4_DARK(0x0, 0xF, 0x0) /**< Brightest green */
#define NG_COLOR_BLUE_BRIGHT  NG_RGB4_DARK(0x0, 0x0, 0xF) /**< Brightest blue */
/** @} */

/** @name Color Extraction */
/** @{ */

/** Extract 5-bit red component (0-31) */
static inline u8 NGColorGetRed(NGColor c) {
    // R0 at bit 14, R4-R1 at bits 11-8
    return ((c >> 14) & 1) | (((c >> 8) & 0xF) << 1);
}

/** Extract 5-bit green component (0-31) */
static inline u8 NGColorGetGreen(NGColor c) {
    // G0 at bit 13, G4-G1 at bits 7-4
    return ((c >> 13) & 1) | (((c >> 4) & 0xF) << 1);
}

/** Extract 5-bit blue component (0-31) */
static inline u8 NGColorGetBlue(NGColor c) {
    // B0 at bit 12, B4-B1 at bits 3-0
    return ((c >> 12) & 1) | ((c & 0xF) << 1);
}

/** Check if dark bit is set */
static inline u8 NGColorIsDark(NGColor c) {
    return (c >> 15) & 1;
}
/** @} */

/** @name Color Manipulation */
/** @{ */

/** Set the dark bit on a color */
static inline NGColor NGColorSetDark(NGColor c) {
    return c | 0x8000;
}

/** Clear the dark bit */
static inline NGColor NGColorClearDark(NGColor c) {
    return c & 0x7FFF;
}

/**
 * Blend two colors.
 * @param a First color
 * @param b Second color
 * @param ratio Blend ratio (0=all a, 255=all b)
 * @return Blended color
 */
NGColor NGColorBlend(NGColor a, NGColor b, u8 ratio);

/**
 * Darken a color toward black.
 * @param c Color to darken
 * @param amount Darken amount (0=no change, 31=black)
 * @return Darkened color
 */
NGColor NGColorDarken(NGColor c, u8 amount);

/**
 * Lighten a color toward white.
 * @param c Color to lighten
 * @param amount Lighten amount (0=no change, 31=white)
 * @return Lightened color
 */
NGColor NGColorLighten(NGColor c, u8 amount);

/**
 * Invert a color.
 * @param c Color to invert
 * @return Inverted color
 */
NGColor NGColorInvert(NGColor c);

/**
 * Convert color to grayscale.
 * @param c Color to convert
 * @return Grayscale color
 */
NGColor NGColorGrayscale(NGColor c);

/**
 * Adjust brightness.
 * @param c Color to adjust
 * @param amount Adjustment (-31 to +31)
 * @return Adjusted color
 */
NGColor NGColorAdjustBrightness(NGColor c, s8 amount);
/** @} */

/** @name Color Generation */
/** @{ */

/**
 * Create color from HSV values.
 * @param h Hue (0-255, wraps around)
 * @param s Saturation (0=gray, 255=full color)
 * @param v Value/brightness (0-255)
 * @return NeoGeo color
 */
NGColor NGColorFromHSV(u8 h, u8 s, u8 v);

/**
 * Create a shade of gray.
 * @param level Gray level (0=black, 31=white)
 * @return Grayscale color
 */
static inline NGColor NGColorGray(u8 level) {
    return NG_RGB5(level, level, level);
}
/** @} */

/** @} */ /* end of color group */

#endif // _NG_COLOR_H_
