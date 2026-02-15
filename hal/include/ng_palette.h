/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_palette.h
 * @brief NeoGeo palette management.
 *
 * The NeoGeo has 256 palettes of 16 colors each (8KB total).
 * - Palettes 0-15: Typically for fix layer (text)
 * - Palettes 16-255: Typically for sprites
 *
 * @note Update palettes during VBlank to avoid visual artifacts.
 */

#ifndef NG_PALETTE_H
#define NG_PALETTE_H

#include <ng_types.h>
#include <ng_color.h>

/**
 * @defgroup palette Palette Management
 * @ingroup hal
 * @brief NeoGeo palette RAM operations.
 * @{
 */

/** @name Constants */
/** @{ */

#define NG_PAL_COUNT    256      /**< Total number of palettes */
#define NG_PAL_SIZE     16       /**< Colors per palette */
#define NG_PAL_RAM_BASE 0x400000 /**< Palette RAM start address */

#define NG_PAL_BANK_FIX 0  /**< Fix layer palette bank (0-15) */
#define NG_PAL_BANK_SPR 16 /**< Sprite palette bank (16-255) */
#define NG_PAL_FIX      0  /**< Default fix layer palette */
/** @} */

/** @name Single Color Operations */
/** @{ */

/**
 * Set a single color in a palette.
 * @param palette Palette index (0-255)
 * @param index Color index within palette (0-15)
 * @param color Color value
 */
void NGPalSetColor(u8 palette, u8 index, NGColor color);

/**
 * Get a color from a palette.
 * @param palette Palette index (0-255)
 * @param index Color index within palette (0-15)
 * @return Color value
 */
NGColor NGPalGetColor(u8 palette, u8 index);
/** @} */

/** @name Bulk Operations */
/** @{ */

/**
 * Set an entire palette (16 colors).
 * @param palette Palette index (0-255)
 * @param colors Array of 16 colors
 */
void NGPalSet(u8 palette, const NGColor colors[NG_PAL_SIZE]);

/**
 * Copy one palette to another.
 * @param dst_palette Destination palette index
 * @param src_palette Source palette index
 */
void NGPalCopy(u8 dst_palette, u8 src_palette);

/**
 * Fill a range of colors with a single color.
 * @param palette Palette index
 * @param start_idx Starting color index
 * @param count Number of colors to fill
 * @param color Color to fill with
 */
void NGPalFill(u8 palette, u8 start_idx, u8 count, NGColor color);

/**
 * Clear a palette (set all to black, preserve reference).
 * @param palette Palette index
 */
void NGPalClear(u8 palette);
/** @} */

/** @name Gradient Generation */
/** @{ */

/**
 * Generate a gradient between two colors.
 * @param palette Palette index
 * @param start_idx First color index
 * @param end_idx Last color index (inclusive)
 * @param start_color Starting color
 * @param end_color Ending color
 */
void NGPalGradient(u8 palette, u8 start_idx, u8 end_idx, NGColor start_color, NGColor end_color);
/** @} */

/** @name Fade Effects */
/** @{ */

/**
 * Fade palette toward a specific color.
 * @param palette Palette index
 * @param target Target color
 * @param amount Fade amount (0=no change, 31=target)
 */
void NGPalFadeToColor(u8 palette, NGColor target, u8 amount);
/** @} */

/** @name Backup/Restore */
/** @{ */

/**
 * Backup a palette to a buffer.
 * @param palette Palette index
 * @param buffer Buffer to store 16 colors
 */
void NGPalBackup(u8 palette, NGColor buffer[NG_PAL_SIZE]);

/**
 * Restore a palette from a buffer.
 * @param palette Palette index
 * @param buffer Buffer containing 16 colors
 */
void NGPalRestore(u8 palette, const NGColor buffer[NG_PAL_SIZE]);
/** @} */

/** @name Utilities */
/** @{ */

/**
 * Set up a palette with base color and darker shades.
 * @param palette Palette index
 * @param base_color Base color for the palette
 */
void NGPalSetupShaded(u8 palette, NGColor base_color);

/**
 * Set up a grayscale palette.
 * @param palette Palette index
 */
void NGPalSetupGrayscale(u8 palette);

/**
 * Initialize palette 0 with default settings.
 * Sets reference color and white for text.
 */
void NGPalInitDefault(void);
/** @} */

/** @name Backdrop Color */
/** @{ */

/**
 * Set the backdrop/background color.
 * @param color Backdrop color
 */
void NGPalSetBackdrop(NGColor color);

/**
 * Get the current backdrop color.
 * @return Current backdrop color
 */
NGColor NGPalGetBackdrop(void);
/** @} */

/** @name Direct Access */
/** @{ */

/**
 * Get pointer to palette RAM.
 * @param palette Palette index
 * @return Pointer to first color in palette
 */
static inline volatile u16 *NGPalGetPtr(u8 palette) {
    return (volatile u16 *)(NG_PAL_RAM_BASE + (palette * NG_PAL_SIZE * 2));
}

/**
 * Get pointer to specific color in palette RAM.
 * @param palette Palette index
 * @param index Color index
 * @return Pointer to color
 */
static inline volatile u16 *NGPalGetColorPtr(u8 palette, u8 index) {
    return (volatile u16 *)(NG_PAL_RAM_BASE + (palette * NG_PAL_SIZE * 2) + (index * 2));
}
/** @} */

/** @} */ /* end of palette group */

#endif /* NG_PALETTE_H */
