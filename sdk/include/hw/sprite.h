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

/**
 * Get the full-size (no shrink) value.
 * Use this instead of hardcoding platform-specific constants.
 */
static inline u16 hw_sprite_full_shrink(void) {
    return 0x0FFF;
}

/*
 * ============================================================================
 *  BATCHED VRAM OPERATIONS
 * ============================================================================
 *
 * These functions handle batched writes to VRAM for complex rendering.
 * They abstract away the NeoGeo-specific VRAM layout.
 */

/**
 * Write a column of tiles directly to sprite VRAM.
 * This is a lower-level function for custom rendering that needs
 * more control than hw_sprite_write_tiles().
 *
 * @param sprite Sprite index
 * @param num_rows Number of tile rows to write
 * @param tiles Array of tile indices (can be NULL to write zeros)
 * @param attrs Array of attributes (can be NULL to write zeros)
 * @param clear_remaining If true, clears rows from num_rows to 32
 */
void hw_sprite_write_column(u16 sprite, u8 num_rows, const u16 *tiles, const u16 *attrs,
                            u8 clear_remaining);

/**
 * Write Y position to SCB3 for a range of sprites (non-sticky).
 * Unlike hw_sprite_write_ychain, all sprites get the same Y/height.
 *
 * @param first First sprite index
 * @param count Number of sprites
 * @param scb3_value Pre-packed SCB3 value from hw_sprite_pack_scb3()
 */
void hw_sprite_write_scb3_range(u16 first, u8 count, u16 scb3_value);

/**
 * Write X positions to SCB4 for sprites at fixed intervals.
 *
 * @param first First sprite index
 * @param count Number of sprites
 * @param start_x Starting X coordinate
 * @param x_step Pixel step between each sprite
 */
void hw_sprite_write_scb4_range(u16 first, u8 count, s16 start_x, s16 x_step);

/**
 * Begin a batched SCB1 write session.
 * Call this before multiple hw_sprite_write_scb1_data() calls.
 *
 * @param sprite Sprite index to start writing at
 */
void hw_sprite_begin_scb1(u16 sprite);

/**
 * Write tile and attribute data during a batched SCB1 session.
 * Must be called after hw_sprite_begin_scb1().
 *
 * @param tile Tile index
 * @param attr Attribute word
 */
void hw_sprite_write_scb1_data(u16 tile, u16 attr);

/**
 * Begin a batched SCB2 write session.
 * @param first First sprite index
 */
void hw_sprite_begin_scb2(u16 first);

/**
 * Write shrink data during a batched SCB2 session.
 * @param shrink Shrink value
 */
void hw_sprite_write_scb2_data(u16 shrink);

/**
 * Begin a batched SCB3 write session.
 * @param first First sprite index
 */
void hw_sprite_begin_scb3(u16 first);

/**
 * Write SCB3 data during a batched session.
 * @param scb3 SCB3 value from hw_sprite_pack_scb3()
 */
void hw_sprite_write_scb3_data(u16 scb3);

/**
 * Begin a batched SCB4 write session.
 * @param first First sprite index
 */
void hw_sprite_begin_scb4(u16 first);

/**
 * Write SCB4 data during a batched session.
 * @param scb4 SCB4 value from hw_sprite_pack_scb4()
 */
void hw_sprite_write_scb4_data(u16 scb4);

#endif /* HW_SPRITE_H */
