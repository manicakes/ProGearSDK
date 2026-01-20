/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file text.h
 * @brief Text rendering on the fix layer.
 *
 * The fix layer is a 40x32 tile layer that renders on top of all sprites.
 * Use it for text, HUD elements, and UI that shouldn't scroll with the world.
 *
 * @example
 * // Print text at a specific position
 * TextPrint(10, 5, 1, "SCORE: 12345");
 *
 * // Print centered text
 * TextPrintAligned(ALIGN_CENTER, ALIGN_TOP, 1, "GAME OVER");
 *
 * // Print formatted text
 * TextPrintf(2, 10, 1, "LIVES: %d", lives);
 */

#ifndef TEXT_H
#define TEXT_H

#include <types.h>

/** @defgroup fixlayer Fix Layer Constants
 *  @{
 */

/** Fix layer visible boundaries (accounting for overscan) */
#define FIX_VISIBLE_TOP    2  /**< First visible row (rows 0-1 are overscan) */
#define FIX_VISIBLE_BOTTOM 29 /**< Last visible row (rows 30-31 are overscan) */
#define FIX_VISIBLE_LEFT   1  /**< First visible column */
#define FIX_VISIBLE_RIGHT  38 /**< Last visible column */

/** Convenience macro for fix layer coordinates */
#define FIX_XY(x, y) ((x) + ((y) << 8))

/** @} */

/**
 * @defgroup text Text Rendering
 * @brief Print text to the screen.
 * @{
 */

/** Horizontal alignment options */
typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } HAlign;

/** Vertical alignment options */
typedef enum { ALIGN_TOP, ALIGN_MIDDLE, ALIGN_BOTTOM } VAlign;

/**
 * Set the font tile base.
 * Character codes are added to this value to get tile indices.
 * Default is 0 (uses BIOS font if available).
 *
 * @param base_tile First tile index of the font
 */
void TextSetFont(u16 base_tile);

/**
 * Print a string at a specific position.
 *
 * @param x Column (0-39)
 * @param y Row (0-31)
 * @param palette Palette index for the text
 * @param str String to print
 */
void TextPrint(u8 x, u8 y, u8 palette, const char *str);

/**
 * Print a formatted string at a specific position.
 * Supports %d, %u, %x, %s, %c format specifiers.
 *
 * @param x Column (0-39)
 * @param y Row (0-31)
 * @param palette Palette index
 * @param fmt Format string
 * @param ... Format arguments
 */
void TextPrintf(u8 x, u8 y, u8 palette, const char *fmt, ...);

/**
 * Print a string with alignment.
 * Position is calculated based on safe area bounds.
 *
 * @param h Horizontal alignment
 * @param v Vertical alignment
 * @param palette Palette index
 * @param str String to print
 */
void TextPrintAligned(HAlign h, VAlign v, u8 palette, const char *str);

/**
 * Print a string with alignment and offset.
 *
 * @param h Horizontal alignment
 * @param v Vertical alignment
 * @param offset_x Offset from aligned position (can be negative)
 * @param offset_y Offset from aligned position (can be negative)
 * @param palette Palette index
 * @param str String to print
 */
void TextPrintOffset(HAlign h, VAlign v, s8 offset_x, s8 offset_y, u8 palette, const char *str);

/**
 * Clear a rectangular region of the fix layer.
 *
 * @param x Start column
 * @param y Start row
 * @param w Width in tiles
 * @param h Height in tiles
 */
void TextClear(u8 x, u8 y, u8 w, u8 h);

/**
 * Clear the entire fix layer.
 */
void TextClearAll(void);

/** @} */

#endif /* TEXT_H */
