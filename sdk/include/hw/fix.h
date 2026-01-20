/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/fix.h
 * @brief Fix layer service.
 * @internal This is an internal header - not for game developers.
 *
 * The fix layer is a 40x32 tile layer that renders on top of all sprites.
 * It's used for text, HUD elements, and UI that shouldn't scroll.
 *
 * The fix layer uses column-major addressing:
 *   VRAM address = 0x7000 + (x * 32) + y
 */

#ifndef HW_FIX_H
#define HW_FIX_H

#include <types.h>

/* Fix layer dimensions */
#define FIX_WIDTH  40
#define FIX_HEIGHT 32

/* Safe area (avoiding overscan on CRTs) */
#define FIX_SAFE_LEFT   1
#define FIX_SAFE_RIGHT  38
#define FIX_SAFE_TOP    2
#define FIX_SAFE_BOTTOM 29

/* Visible area accounting for hardware offset */
#define FIX_VISIBLE_TOP    2
#define FIX_VISIBLE_BOTTOM 29

/**
 * Put a single tile on the fix layer.
 *
 * @param x Column (0-39)
 * @param y Row (0-31)
 * @param tile Tile index
 * @param palette Palette index
 */
void hw_fix_put(u8 x, u8 y, u16 tile, u8 palette);

/**
 * Clear a rectangular region.
 *
 * @param x Start column
 * @param y Start row
 * @param w Width in tiles
 * @param h Height in tiles
 */
void hw_fix_clear(u8 x, u8 y, u8 w, u8 h);

/**
 * Clear the entire fix layer.
 */
void hw_fix_clear_all(void);

/**
 * Write a horizontal row of tiles.
 * Uses VRAMMOD=32 for efficient horizontal writes.
 *
 * @param x Start column
 * @param y Row
 * @param tiles Array of tile indices
 * @param count Number of tiles
 * @param palette Palette index
 */
void hw_fix_row(u8 x, u8 y, const u16 *tiles, u8 count, u8 palette);

/**
 * Write a text string using ASCII-to-tile mapping.
 * Character codes are added to font_base to get tile indices.
 *
 * @param x Start column
 * @param y Row
 * @param str Null-terminated string
 * @param palette Palette index
 * @param font_base Base tile index for font
 */
void hw_fix_text(u8 x, u8 y, const char *str, u8 palette, u16 font_base);

/**
 * Write a text string, returning the number of characters written.
 *
 * @param x Start column
 * @param y Row
 * @param str Null-terminated string
 * @param palette Palette index
 * @param font_base Base tile index for font
 * @return Number of characters written
 */
u8 hw_fix_text_len(u8 x, u8 y, const char *str, u8 palette, u16 font_base);

#endif /* HW_FIX_H */
