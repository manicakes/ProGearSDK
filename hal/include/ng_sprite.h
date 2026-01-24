/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_sprite.h
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

#ifndef _NG_SPRITE_H_
#define _NG_SPRITE_H_

#include <ng_types.h>
#include <ng_hardware.h>

/**
 * @defgroup sprite Sprite Hardware
 * @ingroup hal
 * @brief Low-level Sprite Control Block (SCB) operations.
 * @{
 */

/** @name SCB Register Addresses */
/** @{ */

/** SCB1: Tile index (16-bit) + attributes (16-bit) per row, 64 bytes per sprite */
#define NG_SCB1_BASE 0x0000

/** SCB2: Shrink value (upper nibble=H, lower byte=V), 1 word per sprite */
#define NG_SCB2_BASE 0x8000

/** SCB3: Y position (bits 15-7) | sticky (bit 6) | height (bits 5-0) */
#define NG_SCB3_BASE 0x8200

/** SCB4: X position (bits 15-7), 1 word per sprite */
#define NG_SCB4_BASE 0x8400
/** @} */

/** @name Constants */
/** @{ */

/** Full-size shrink value (no scaling) */
#define NG_SPRITE_SHRINK_NONE 0x0FFF

/** Sticky bit for SCB3 (sprite inherits Y/height from previous sprite) */
#define NG_SPRITE_STICKY_BIT 0x40

/** Maximum sprite height in tiles */
#define NG_SPRITE_MAX_HEIGHT 32
/** @} */

/** @name Inline Utility Functions */
/** @{ */

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

/** @name SCB1: Tile Column Writing */
/** @{ */

/**
 * Begin writing tiles to a sprite's SCB1 column.
 * Sets VRAMADDR to the sprite's tile base and VRAMMOD to 1.
 * Follow with NGSpriteTileWrite() or NGSpriteTileWriteEmpty() calls.
 *
 * @param sprite_idx Hardware sprite index (0-380)
 */
void NGSpriteTileBegin(u16 sprite_idx);

/**
 * Write one tile entry (index + attributes) to current column.
 * Auto-advances to next row. Requires prior NGSpriteTileBegin().
 *
 * @param tile_idx  C-ROM tile index (0-65535)
 * @param palette   Palette index (0-255)
 * @param h_flip    Horizontal flip (0=normal, 1=flipped)
 * @param v_flip    Vertical flip (0=normal, 1=flipped)
 */
void NGSpriteTileWrite(u16 tile_idx, u8 palette, u8 h_flip, u8 v_flip);

/**
 * Write one raw tile entry (pre-computed index and attributes).
 * For cases where attributes are already calculated (e.g., tilemap lookup).
 *
 * @param tile_idx  C-ROM tile index
 * @param attr      Pre-computed attribute word (palette<<8 | flip bits)
 */
void NGSpriteTileWriteRaw(u16 tile_idx, u16 attr);

/**
 * Write an empty tile slot (tile 0, no attributes).
 */
void NGSpriteTileWriteEmpty(void);

/**
 * Pad remaining rows to 32 with empty tiles.
 * Call after writing all visible rows to clear unused slots.
 *
 * @param rows_written Number of rows already written (0-32)
 */
void NGSpriteTilePadTo32(u8 rows_written);
/** @} */

/** @name SCB2: Shrink Values */
/** @{ */

/**
 * Set shrink value for a range of consecutive sprites.
 * Efficiently batches writes using VRAMMOD auto-increment.
 *
 * @param first_sprite First hardware sprite index
 * @param count        Number of sprites to set
 * @param shrink       Shrink value (0x0FFF = full size, 0x0000 = invisible)
 */
void NGSpriteShrinkSet(u16 first_sprite, u8 count, u16 shrink);
/** @} */

/** @name SCB3: Y Position and Height */
/** @{ */

/**
 * Set Y position and height for a single sprite.
 *
 * @param sprite_idx Hardware sprite index
 * @param screen_y   Screen Y coordinate (0 = top of screen)
 * @param height     Sprite height in tiles (1-32)
 */
void NGSpriteYSet(u16 sprite_idx, s16 screen_y, u8 height);

/**
 * Set Y/height for a chained multi-column sprite.
 * First sprite is "driving" (has Y and height).
 * Subsequent sprites are "sticky" (inherit from previous).
 * Used by actors where columns share the same vertical position.
 *
 * @param first_sprite First hardware sprite index
 * @param count        Number of columns in the chain
 * @param screen_y     Screen Y coordinate
 * @param height       Sprite height in tiles
 */
void NGSpriteYSetChain(u16 first_sprite, u8 count, s16 screen_y, u8 height);

/**
 * Set same Y/height for a range of independent sprites.
 * Each sprite gets its own Y position (no sticky bit).
 * Used by backdrops and terrain where columns move independently.
 *
 * @param first_sprite First hardware sprite index
 * @param count        Number of sprites
 * @param screen_y     Screen Y coordinate
 * @param height       Sprite height in tiles
 */
void NGSpriteYSetUniform(u16 first_sprite, u8 count, s16 screen_y, u8 height);
/** @} */

/** @name SCB4: X Positions */
/** @{ */

/**
 * Set X position for a single sprite.
 *
 * @param sprite_idx Hardware sprite index
 * @param screen_x   Screen X coordinate
 */
void NGSpriteXSet(u16 sprite_idx, s16 screen_x);

/**
 * Set X positions for a range with regular spacing.
 * Computes: x[col] = base_x + col * spacing
 *
 * @param first_sprite First hardware sprite index
 * @param count        Number of sprites
 * @param base_x       X position of first sprite
 * @param spacing      Pixel spacing between sprite columns
 */
void NGSpriteXSetSpaced(u16 first_sprite, u8 count, s16 base_x, s16 spacing);

/**
 * Begin batch X position writing.
 * Sets up VRAMADDR for SCB4 and VRAMMOD for auto-increment.
 * Follow with NGSpriteXWriteNext() calls.
 *
 * @param first_sprite First hardware sprite index
 */
void NGSpriteXBegin(u16 first_sprite);

/**
 * Write next X position in batch sequence.
 * Requires prior NGSpriteXBegin(). Auto-advances to next sprite.
 *
 * @param screen_x Screen X coordinate
 */
void NGSpriteXWriteNext(s16 screen_x);
/** @} */

/** @name Combined High-Level Operations */
/** @{ */

/**
 * Setup all position registers for a multi-column sprite strip.
 * Sets shrink (SCB2), Y position as chain (SCB3), and spaced X (SCB4).
 * Does NOT write tile data (SCB1) - call NGSpriteTile* separately.
 *
 * @param first_sprite First hardware sprite index
 * @param num_cols     Number of columns
 * @param screen_x     Screen X of leftmost column
 * @param screen_y     Screen Y position
 * @param height       Sprite height in tiles
 * @param tile_width   Pixel width per column (for X spacing)
 * @param shrink       Shrink value (0x0FFF = full size)
 */
void NGSpriteSetupStrip(u16 first_sprite, u8 num_cols, s16 screen_x, s16 screen_y, u8 height,
                        s16 tile_width, u16 shrink);

/**
 * Setup position registers for independent columns (non-chained).
 * Like NGSpriteSetupStrip but uses uniform Y instead of chained.
 * Used by backdrops and terrain.
 *
 * @param first_sprite First hardware sprite index
 * @param num_cols     Number of columns
 * @param screen_x     Screen X of leftmost column
 * @param screen_y     Screen Y position
 * @param height       Sprite height in tiles
 * @param tile_width   Pixel width per column
 * @param shrink       Shrink value
 */
void NGSpriteSetupGrid(u16 first_sprite, u8 num_cols, s16 screen_x, s16 screen_y, u8 height,
                       s16 tile_width, u16 shrink);
/** @} */

/** @} */ /* end of sprite group */

#endif /* _NG_SPRITE_H_ */
