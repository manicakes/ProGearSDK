/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sprite.h
 * @brief Low-level NeoGeo hardware sprite utilities.
 *
 * This module provides shared constants and inline functions for
 * manipulating NeoGeo sprite hardware. Used internally by actor,
 * tilemap, parallax, and UI modules.
 *
 * @section scb Sprite Control Blocks (SCB)
 * NeoGeo sprites are controlled via four SCB regions in VRAM:
 * - SCB1: Tile indices and attributes (palette, flip bits)
 * - SCB2: Shrink values (horizontal and vertical scaling)
 * - SCB3: Y position and sprite height
 * - SCB4: X position
 *
 * @section coords Coordinate System
 * The NeoGeo uses an inverted Y coordinate for sprites:
 * - Hardware Y 496 = screen top (Y=0)
 * - Hardware Y decreases as screen Y increases
 * - Values wrap at 512 (9-bit field)
 */

#ifndef _SPRITE_H_
#define _SPRITE_H_

#include <types.h>
#include <neogeo.h>

/** @defgroup scbregs SCB Register Addresses
 *  @brief VRAM base addresses for Sprite Control Blocks.
 *  @{
 */

/** SCB1: Tile index (16-bit) + attributes (16-bit) per row, 64 bytes per sprite */
#define NG_SCB1_BASE 0x0000

/** SCB2: Shrink value (upper nibble=H, lower byte=V), 1 word per sprite */
#define NG_SCB2_BASE 0x8000

/** SCB3: Y position (bits 15-7) | sticky (bit 6) | height (bits 5-0) */
#define NG_SCB3_BASE 0x8200

/** SCB4: X position (bits 15-7), 1 word per sprite */
#define NG_SCB4_BASE 0x8400

/** @} */

/** @defgroup spriteconst Sprite Constants
 *  @{
 */

/** Full-size shrink value (no scaling) */
#define NG_SPRITE_SHRINK_NONE 0x0FFF

/** Sticky bit for SCB3 (sprite inherits Y/height from previous sprite) */
#define NG_SPRITE_STICKY_BIT 0x40

/** Maximum sprite height in tiles */
#define NG_SPRITE_MAX_HEIGHT 32

/** @} */

/** @defgroup spritefunc Sprite Utility Functions
 *  @brief Inline functions for common sprite operations.
 *
 *  All functions are static inline for zero call overhead.
 *  @{
 */

/**
 * Convert screen Y coordinate to NeoGeo hardware Y value.
 *
 * NeoGeo sprites use inverted Y: 496 at screen top, decreasing downward.
 * Values wrap at 512 (9-bit field).
 *
 * @param screen_y Screen Y coordinate (0 = top of screen)
 * @return Hardware Y value (9-bit, 0-511)
 */
static inline s16 NGSpriteScreenToHardwareY(s16 screen_y) {
    s16 y_val = (s16)(496 - screen_y);
    if (y_val < 0)
        y_val = (s16)(y_val + 512);
    return (s16)(y_val & 0x1FF);
}

/**
 * Calculate adjusted sprite height for zoom/shrink.
 *
 * When sprites are shrunk vertically, fewer tile rows are visible.
 * This calculates the actual number of rows needed to fill the
 * display area without garbage pixels.
 *
 * @param rows Original height in tiles (1-32)
 * @param v_shrink Vertical shrink value (0-255, 255 = full size)
 * @return Adjusted height in tiles (1-32)
 */
static inline u8 NGSpriteAdjustedHeight(u8 rows, u8 v_shrink) {
    /* Ceiling division: (rows * v_shrink + 254) / 255 */
    u16 adjusted = (u16)(((u16)rows * v_shrink + 254) / 255);
    if (adjusted < 1)
        adjusted = 1;
    if (adjusted > NG_SPRITE_MAX_HEIGHT)
        adjusted = NG_SPRITE_MAX_HEIGHT;
    return (u8)adjusted;
}

/**
 * Calculate SCB3 value for a sprite.
 *
 * SCB3 format: Y position (bits 15-7) | sticky (bit 6) | height (bits 5-0)
 *
 * @param screen_y Screen Y coordinate (0 = top of screen)
 * @param height Sprite height in tiles (1-32)
 * @return SCB3 value ready to write to VRAM
 */
static inline u16 NGSpriteSCB3(s16 screen_y, u8 height) {
    s16 hw_y = NGSpriteScreenToHardwareY(screen_y);
    return (u16)(((u16)hw_y << 7) | (height & 0x3F));
}

/**
 * Get SCB3 sticky value for chained sprites.
 *
 * Sprites with the sticky bit set inherit Y position and height
 * from the previous sprite. Used for multi-column actors.
 *
 * @return SCB3 value with only sticky bit set
 */
static inline u16 NGSpriteSCB3Sticky(void) {
    return NG_SPRITE_STICKY_BIT;
}

/**
 * Calculate SCB4 value for a sprite.
 *
 * SCB4 format: X position in bits 15-7 (9-bit value shifted left 7)
 *
 * @param screen_x Screen X coordinate
 * @return SCB4 value ready to write to VRAM
 */
static inline u16 NGSpriteSCB4(s16 screen_x) {
    return (u16)((screen_x & 0x1FF) << 7);
}

/**
 * Hide a range of sprites by zeroing their SCB3 values.
 *
 * Setting SCB3 to 0 makes a sprite invisible (height = 0).
 * Uses optimized VRAM macros for fast clearing.
 *
 * @param first_sprite First sprite index to hide
 * @param count Number of sprites to hide
 */
static inline void NGSpriteHideRange(u16 first_sprite, u8 count) {
    if (count == 0)
        return;
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB3_BASE + first_sprite, 1);
    NG_VRAM_CLEAR_FAST(count);
}

/** @} */

#endif /* _SPRITE_H_ */
