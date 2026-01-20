/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/sprite.h
 * @brief Sprite rendering service.
 * @internal This is an internal header - not for game developers.
 *
 * This module handles all sprite control block (SCB) manipulation.
 * It provides efficient functions that only write changed data and
 * use optimized VRAM access patterns.
 */

#ifndef HW_SPRITE_H
#define HW_SPRITE_H

#include <types.h>

/*
 * ============================================================================
 *  SCB WRITING - GRANULAR OPERATIONS
 * ============================================================================
 *
 * These functions write specific SCB regions. Use them when you know
 * exactly what changed (e.g., position-only updates skip tile data).
 */

/**
 * Write tile data to SCB1 for a single sprite column.
 * Uses VRAMMOD=1 for sequential writes down the column.
 *
 * @param sprite Sprite index (0-380)
 * @param height Number of tile rows (1-32)
 * @param tiles Array of tile indices (length = height)
 * @param attrs Array of attribute words (length = height)
 */
void hw_sprite_write_tiles(u16 sprite, u8 height, const u16 *tiles, const u16 *attrs);

/**
 * Write a single tile and attribute to SCB1.
 * More efficient than hw_sprite_write_tiles for single-tile updates.
 *
 * @param sprite Sprite index
 * @param row Tile row (0-31)
 * @param tile Tile index
 * @param attr Attribute word (palette << 8 | flip bits)
 */
void hw_sprite_write_tile(u16 sprite, u8 row, u16 tile, u16 attr);

/**
 * Write shrink value to SCB2 for a range of sprites.
 * Uses VRAMMOD=1 for batch writes.
 *
 * @param first First sprite index
 * @param count Number of sprites
 * @param shrink Shrink value (0x0FFF = full size)
 */
void hw_sprite_write_shrink(u16 first, u8 count, u16 shrink);

/**
 * Write Y position and height to SCB3.
 * First sprite gets the position, subsequent sprites get sticky bit.
 *
 * @param first First sprite index
 * @param count Number of sprites in chain
 * @param y Screen Y coordinate (0 = top)
 * @param height Sprite height in tiles (1-32)
 */
void hw_sprite_write_ychain(u16 first, u8 count, s16 y, u8 height);

/**
 * Write X positions to SCB4 for a chain of sprites.
 * Each sprite is offset by (column * 16 * zoom / 16).
 *
 * @param first First sprite index
 * @param count Number of sprites
 * @param x Screen X coordinate of first sprite
 * @param zoom Zoom level (16 = 100%, 8 = 50%)
 */
void hw_sprite_write_xchain(u16 first, u8 count, s16 x, u8 zoom);

/**
 * Hide sprites by setting their SCB3 height to zero.
 *
 * @param first First sprite index
 * @param count Number of sprites to hide
 */
void hw_sprite_hide(u16 first, u8 count);

/**
 * Hide all sprites (clear entire SCB3 region).
 */
void hw_sprite_hide_all(void);

/*
 * ============================================================================
 *  COORDINATE CONVERSION
 * ============================================================================
 *
 * The NeoGeo uses an inverted Y coordinate system:
 *   Hardware Y 496 = screen top (Y=0)
 *   Hardware Y decreases as screen Y increases
 *   Values wrap at 512 (9-bit field)
 */

/**
 * Convert screen Y to hardware Y coordinate.
 */
static inline s16 hw_sprite_screen_to_hw_y(s16 screen_y) {
    s16 hw_y = (s16)(496 - screen_y);
    if (hw_y < 0)
        hw_y = (s16)(hw_y + 512);
    return (s16)(hw_y & 0x1FF);
}

/**
 * Calculate adjusted height for zoom/shrink.
 * When sprites are shrunk, fewer tile rows are needed.
 *
 * @param rows Original height in tiles
 * @param v_shrink Vertical shrink value (0-255, 255 = full size)
 * @return Adjusted height in tiles
 */
static inline u8 hw_sprite_adjusted_height(u8 rows, u8 v_shrink) {
    u16 adjusted = (u16)(((u16)rows * v_shrink + 254) / 255);
    if (adjusted < 1)
        adjusted = 1;
    if (adjusted > 32)
        adjusted = 32;
    return (u8)adjusted;
}

/**
 * Pack Y position and height into SCB3 format.
 */
static inline u16 hw_sprite_pack_scb3(s16 screen_y, u8 height) {
    s16 hw_y = hw_sprite_screen_to_hw_y(screen_y);
    return (u16)(((u16)hw_y << 7) | (height & 0x3F));
}

/**
 * Pack X position into SCB4 format.
 */
static inline u16 hw_sprite_pack_scb4(s16 screen_x) {
    return (u16)((screen_x & 0x1FF) << 7);
}

/**
 * Get the sticky bit value for chained sprites.
 */
static inline u16 hw_sprite_scb3_sticky(void) {
    return 0x40;
}

/**
 * Build attribute word from palette and flip flags.
 */
static inline u16 hw_sprite_attr(u8 palette, u8 h_flip, u8 v_flip) {
    u16 attr = (u16)((u16)palette << 8);
    if (v_flip)
        attr |= 0x02;
    if (!h_flip) /* Default h_flip=1 for correct display */
        attr |= 0x01;
    return attr;
}

#endif /* HW_SPRITE_H */
