/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/palette.h
 * @brief Palette service.
 * @internal This is an internal header - not for game developers.
 *
 * Efficient palette manipulation using optimized memory access patterns.
 */

#ifndef HW_PALETTE_H
#define HW_PALETTE_H

#include <types.h>
#include <hw/palram.h>

/**
 * Set all 16 colors of a palette.
 * Uses unrolled writes for maximum speed.
 *
 * @param index Palette index (0-255)
 * @param colors Array of 16 colors
 */
void hw_palette_set(u8 index, const u16 *colors);

/**
 * Copy palette to RAM buffer.
 *
 * @param index Palette index
 * @param buffer Output buffer (16 colors)
 */
void hw_palette_backup(u8 index, u16 *buffer);

/**
 * Restore palette from RAM buffer.
 *
 * @param index Palette index
 * @param buffer Input buffer (16 colors)
 */
void hw_palette_restore(u8 index, const u16 *buffer);

/**
 * Copy one palette to another.
 *
 * @param dst Destination palette index
 * @param src Source palette index
 */
void hw_palette_copy(u8 dst, u8 src);

/**
 * Fill palette with a single color.
 *
 * @param index Palette index
 * @param start First color index to fill
 * @param count Number of colors to fill
 * @param color Color value
 */
void hw_palette_fill(u8 index, u8 start, u8 count, u16 color);

/**
 * Clear palette (black with transparent reference).
 *
 * @param index Palette index
 */
void hw_palette_clear(u8 index);

/*
 * ============================================================================
 *  INLINE ACCESSORS
 * ============================================================================
 */

/**
 * Set a single color.
 */
static inline void hw_palette_set_color(u8 pal, u8 idx, u16 color) {
    palram_set(pal, idx, color);
}

/**
 * Get a single color.
 */
static inline u16 hw_palette_get_color(u8 pal, u8 idx) {
    return palram_get(pal, idx);
}

/**
 * Set backdrop color.
 */
static inline void hw_palette_set_backdrop(u16 color) {
    palram_set_backdrop(color);
}

/**
 * Get backdrop color.
 */
static inline u16 hw_palette_get_backdrop(void) {
    return palram_get_backdrop();
}

/**
 * Get pointer to palette for direct access.
 */
static inline vu16 *hw_palette_ptr(u8 index) {
    return palram_ptr(index);
}

#endif /* HW_PALETTE_H */
