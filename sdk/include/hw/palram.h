/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/palram.h
 * @brief Palette RAM hardware access.
 * @internal This is an internal header - not for game developers.
 *
 * The NeoGeo has 256 palettes of 16 colors each (8KB total).
 * Color 0 of each palette is transparent (reference color).
 */

#ifndef HW_PALRAM_H
#define HW_PALRAM_H

#include <types.h>

/*
 * ============================================================================
 *  PALETTE RAM ADDRESSES
 * ============================================================================
 */

#define PALRAM_BASE     ((vu16 *)0x400000)
#define PALRAM_BACKDROP ((vu16 *)0x401FFE)

/*
 * ============================================================================
 *  PALETTE CONSTANTS
 * ============================================================================
 */

#define PALETTE_SIZE  16  /* Colors per palette */
#define PALETTE_COUNT 256 /* Total palettes */

/*
 * ============================================================================
 *  COLOR FORMAT
 * ============================================================================
 *
 * NeoGeo colors are 16-bit with the following format:
 *   [15] Dark bit (reduces brightness by 50%)
 *   [14-10] Red (0-31)
 *   [9-5] Green (0-31)
 *   [4-0] Blue (0-31)
 *
 * Note: The hardware actually uses a non-standard bit arrangement,
 * but this is handled transparently by the color creation macros.
 */

#define COLOR_DARK_BIT 0x8000

/*
 * ============================================================================
 *  PALETTE ACCESS
 * ============================================================================
 */

/* Get pointer to start of palette N */
#define palram_ptr(n) (PALRAM_BASE + ((n) * PALETTE_SIZE))

/* Get pointer to specific color in palette */
#define palram_color_ptr(pal, idx) (PALRAM_BASE + ((pal) * PALETTE_SIZE) + (idx))

/* Direct read/write */
#define palram_set(pal, idx, color) (palram_color_ptr(pal, idx)[0] = (color))
#define palram_get(pal, idx)        (palram_color_ptr(pal, idx)[0])

/* Backdrop color (special location) */
#define palram_set_backdrop(color) (*PALRAM_BACKDROP = (color))
#define palram_get_backdrop()      (*PALRAM_BACKDROP)

#endif /* HW_PALRAM_H */
