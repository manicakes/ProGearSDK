/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file fix.h
 * @brief Fix layer (text) rendering.
 *
 * The fix layer is a 40x32 tile layer used primarily for text and HUD.
 * It overlays sprites and is not affected by scrolling.
 *
 * @section fixlayout Screen Layout
 * - Total: 40x32 tiles (640x512 virtual)
 * - Visible: 40x28 tiles (320x224 pixels)
 * - Safe area: 38x25 tiles (accounts for CRT overscan)
 */

#ifndef FIX_H
#define FIX_H

#include <neogeo.h>
#include <hw/lspc.h>

/** @defgroup fixconst Fix Layer Constants
 *  @{
 *
 *  Core constants (FIX_WIDTH, FIX_HEIGHT, FIX_SAFE_*, FIX_VISIBLE_*) are
 *  defined in hw/lspc.h as the single source of truth. The NG_ prefixed
 *  versions below are aliases for the public API.
 */

/* Aliases to hw/lspc.h constants (NG_ prefix for public API consistency) */
#define NG_FIX_WIDTH          FIX_WIDTH          /**< Fix layer width in tiles */
#define NG_FIX_HEIGHT         FIX_HEIGHT         /**< Fix layer height in tiles */
#define NG_FIX_VISIBLE_TOP    FIX_VISIBLE_TOP    /**< First visible row (inclusive) */
#define NG_FIX_VISIBLE_BOTTOM FIX_VISIBLE_BOTTOM /**< Last visible row (inclusive) */
#define NG_FIX_SAFE_TOP       FIX_SAFE_TOP       /**< Safe area top row (inclusive) */
#define NG_FIX_SAFE_BOTTOM    FIX_SAFE_BOTTOM    /**< Safe area bottom row (inclusive) */
#define NG_FIX_SAFE_LEFT      FIX_SAFE_LEFT      /**< Safe area left column (inclusive) */
#define NG_FIX_SAFE_RIGHT     FIX_SAFE_RIGHT     /**< Safe area right column (inclusive) */

/* Constants not in hw/lspc.h */
#define NG_FIX_VRAM           0x7000 /**< Fix layer VRAM address */
#define NG_FIX_VISIBLE_LEFT   0      /**< First visible column (inclusive) */
#define NG_FIX_VISIBLE_RIGHT  39     /**< Last visible column (inclusive) */
#define NG_FIX_COLOR_TEXT     1      /**< Main text color index */
#define NG_FIX_COLOR_SHADOW   2      /**< Shadow/outline color index */

/** @} */

/** @defgroup fixlayout Text Layout System
 *  @brief Alignment-based text positioning.
 *  @{
 */

/** Horizontal alignment */
typedef enum {
    NG_ALIGN_LEFT = 0,   /**< Align to left edge */
    NG_ALIGN_CENTER = 1, /**< Center horizontally */
    NG_ALIGN_RIGHT = 2   /**< Align to right edge */
} NGFixHAlign;

/** Vertical alignment */
typedef enum {
    NG_ALIGN_TOP = 0,    /**< Align to top edge */
    NG_ALIGN_MIDDLE = 1, /**< Center vertically */
    NG_ALIGN_BOTTOM = 2  /**< Align to bottom edge */
} NGFixVAlign;

/** Layout descriptor for text positioning */
typedef struct {
    u8 h_align;  /**< Horizontal alignment */
    u8 v_align;  /**< Vertical alignment */
    s8 offset_x; /**< X offset from aligned position */
    s8 offset_y; /**< Y offset from aligned position */
} NGFixLayout;

/**
 * Create layout with alignment.
 * @param h Horizontal alignment
 * @param v Vertical alignment
 * @return Layout descriptor
 */
NGFixLayout NGFixLayoutAlign(NGFixHAlign h, NGFixVAlign v);

/**
 * Create layout with alignment and offset.
 * @param h Horizontal alignment
 * @param v Vertical alignment
 * @param offset_x X offset from aligned position
 * @param offset_y Y offset from aligned position
 * @return Layout descriptor
 */
NGFixLayout NGFixLayoutOffset(NGFixHAlign h, NGFixVAlign v, s8 offset_x, s8 offset_y);

/**
 * Create layout for exact position.
 * @param x X position (relative to safe area)
 * @param y Y position (relative to safe area)
 * @return Layout descriptor
 */
static inline NGFixLayout NGFixLayoutXY(u8 x, u8 y) {
    NGFixLayout layout = {NG_ALIGN_LEFT, NG_ALIGN_TOP, (s8)(x - NG_FIX_SAFE_LEFT),
                          (s8)(y - NG_FIX_SAFE_TOP)};
    return layout;
}

/** @} */

/** @defgroup fixfunc Fix Layer Functions
 *  @{
 */

/**
 * Put a single tile on the fix layer.
 * @param x X position (0-39)
 * @param y Y position (0-31)
 * @param tile Tile index
 * @param palette Palette index (0-255)
 */
void NGFixPut(u8 x, u8 y, u16 tile, u8 palette);

/**
 * Clear a rectangular region.
 * @param x X position
 * @param y Y position
 * @param w Width in tiles
 * @param h Height in tiles
 */
void NGFixClear(u8 x, u8 y, u8 w, u8 h);

/**
 * Clear the entire fix layer.
 */
void NGFixClearAll(void);

/** @} */

/** @defgroup textfunc Text Rendering
 *  @{
 */

/**
 * Set font base tile (optional).
 * ASCII characters map to: font_base + (char - 0x20)
 * @param font_base_tile First tile of font
 */
void NGTextSetFont(u16 font_base_tile);

/**
 * Print a string.
 * @param layout Layout descriptor
 * @param palette Palette index
 * @param str String to print
 */
void NGTextPrint(NGFixLayout layout, u8 palette, const char *str);

/**
 * Print formatted text.
 * Supports: %d, %x, %s, %c
 * @param layout Layout descriptor
 * @param palette Palette index
 * @param fmt Format string
 * @param ... Format arguments
 */
void NGTextPrintf(NGFixLayout layout, u8 palette, const char *fmt, ...);

/** @} */

/** @defgroup fixclean Clean API Aliases
 *  @brief Shorter names without NG_ prefix for cleaner code.
 *  @{
 */

/* Constants: FIX_WIDTH, FIX_HEIGHT, FIX_SAFE_*, FIX_VISIBLE_TOP/BOTTOM
 * are already defined in hw/lspc.h (included above). Only define the
 * remaining aliases here. */
#define FIX_VRAM           NG_FIX_VRAM
#define FIX_VISIBLE_LEFT   NG_FIX_VISIBLE_LEFT
#define FIX_VISIBLE_RIGHT  NG_FIX_VISIBLE_RIGHT
#define FIX_COLOR_TEXT     NG_FIX_COLOR_TEXT
#define FIX_COLOR_SHADOW   NG_FIX_COLOR_SHADOW

/* Type aliases */
typedef NGFixHAlign FixHAlign;
typedef NGFixVAlign FixVAlign;
typedef NGFixLayout FixLayout;

/* Alignment constants */
#define ALIGN_LEFT   NG_ALIGN_LEFT
#define ALIGN_CENTER NG_ALIGN_CENTER
#define ALIGN_RIGHT  NG_ALIGN_RIGHT
#define ALIGN_TOP    NG_ALIGN_TOP
#define ALIGN_MIDDLE NG_ALIGN_MIDDLE
#define ALIGN_BOTTOM NG_ALIGN_BOTTOM

/* Function aliases */
#define FixLayoutAlign  NGFixLayoutAlign
#define FixLayoutOffset NGFixLayoutOffset
#define FixLayoutXY     NGFixLayoutXY
#define FixPut          NGFixPut
#define FixClear        NGFixClear
#define FixClearAll     NGFixClearAll
#define TextPrint       NGTextPrint
#define TextPrintf      NGTextPrintf

/** @} */

#endif // FIX_H
