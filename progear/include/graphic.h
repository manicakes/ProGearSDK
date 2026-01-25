/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file graphic.h
 * @brief Platform-agnostic graphics abstraction API.
 *
 * NGGraphic provides a portable abstraction for 2D sprite rendering that can
 * be implemented for different backends (NeoGeo hardware, SDL, OpenGL, etc.).
 *
 * Key design principles:
 * - All dimensions in pixels (not hardware-specific tiles)
 * - No hardware resource management exposed (sprites, VRAM, etc.)
 * - Backend handles allocation and optimization internally
 * - Automatic dirty tracking for efficient updates
 *
 * @section usage Basic Usage
 * @code
 * // Create a graphic
 * NGGraphicConfig cfg = {
 *     .width = 64,
 *     .height = 64,
 *     .tile_mode = NG_GRAPHIC_TILE_REPEAT,
 *     .layer = NG_GRAPHIC_LAYER_ENTITY
 * };
 * NGGraphic *g = NGGraphicCreate(&cfg);
 *
 * // Set source from visual asset
 * NGGraphicSetSource(g, &NGVisualAsset_Player, palette);
 *
 * // Position and the system handles rendering
 * NGGraphicSetPosition(g, screen_x, screen_y);
 * @endcode
 *
 * Graphics are automatically rendered by NGGraphicSystemDraw() which is
 * called by the engine. Manual NGGraphicCommit() is only needed for
 * immediate updates.
 */

#ifndef _GRAPHIC_H_
#define _GRAPHIC_H_

#include <ng_types.h>
#include <visual.h>

/**
 * @defgroup graphic Graphics API
 * @ingroup sdk
 * @brief Platform-agnostic 2D sprite rendering.
 * @{
 */

/** @name Types and Enums */
/** @{ */

/** Maximum graphics that can be active simultaneously */
#define NG_GRAPHIC_MAX 64

/** Scale value representing 1.0x (no scaling) */
#define NG_GRAPHIC_SCALE_ONE 256

/** Opaque graphic handle */
typedef struct NGGraphic NGGraphic;

/**
 * Tile mapping mode.
 * Determines how source pixels map to display dimensions.
 */
typedef enum {
    /** Clip at source bounds (no repeat) */
    NG_GRAPHIC_TILE_CLIP,
    /** Repeat/tile source when display exceeds source size */
    NG_GRAPHIC_TILE_REPEAT,
    /** 9-slice stretching for resizable UI panels */
    NG_GRAPHIC_TILE_9SLICE,
    /** Infinite horizontal scroll with circular buffer (backdrops) */
    NG_GRAPHIC_TILE_INFINITE
} NGGraphicTileMode;

/**
 * Flip flags for rendering.
 */
typedef enum {
    NG_GRAPHIC_FLIP_NONE = 0,
    NG_GRAPHIC_FLIP_H = 1,
    NG_GRAPHIC_FLIP_V = 2
} NGGraphicFlip;

/**
 * Render layer.
 * Determines rendering order and backend optimization hints.
 * Lower layers render first (behind higher layers).
 */
typedef enum {
    /** Background layer - renders first, independent columns */
    NG_GRAPHIC_LAYER_BACKGROUND = 0,
    /** World layer - game terrain and objects */
    NG_GRAPHIC_LAYER_WORLD = 1,
    /** Entity layer - game actors, chained columns for smooth movement */
    NG_GRAPHIC_LAYER_ENTITY = 2,
    /** Foreground layer - overlays, effects */
    NG_GRAPHIC_LAYER_FOREGROUND = 3,
    /** UI layer - screen-space, renders last */
    NG_GRAPHIC_LAYER_UI = 4
} NGGraphicLayer;

/**
 * Configuration for creating a graphic.
 * All dimensions are in pixels.
 */
typedef struct {
    u16 width;                   /**< Display width in pixels */
    u16 height;                  /**< Display height in pixels */
    NGGraphicTileMode tile_mode; /**< How to handle source vs display size */
    NGGraphicLayer layer;        /**< Render layer */
    u8 z_order;                  /**< Sort order within layer (0=back, 255=front) */
} NGGraphicConfig;
/** @} */

/** @name Lifecycle */
/** @{ */

/**
 * Create a graphic with given configuration.
 * The graphic is automatically registered with the system for rendering.
 *
 * @param config Configuration parameters
 * @return New graphic, or NULL if allocation fails or limit reached
 */
NGGraphic *NGGraphicCreate(const NGGraphicConfig *config);

/**
 * Destroy a graphic and release all resources.
 *
 * @param g Graphic to destroy (NULL safe)
 */
void NGGraphicDestroy(NGGraphic *g);
/** @} */

/** @name Source Configuration */
/** @{ */

/**
 * Set source from a visual asset.
 *
 * @param g Graphic
 * @param asset Visual asset (provides tile data and dimensions)
 * @param palette Palette index (0-255)
 */
void NGGraphicSetSource(NGGraphic *g, const NGVisualAsset *asset, u8 palette);

/**
 * Set source from raw tile data.
 * For procedural content or when not using NGVisualAsset.
 *
 * @param g Graphic
 * @param base_tile First tile index in tile ROM
 * @param src_width Source width in pixels
 * @param src_height Source height in pixels
 * @param palette Palette index (0-255)
 */
void NGGraphicSetSourceRaw(NGGraphic *g, u16 base_tile, u16 src_width, u16 src_height, u8 palette);

/**
 * Set source from a tilemap (row-major layout).
 * Used for terrain, UI panels, and other tilemap-based content.
 *
 * @param g Graphic
 * @param base_tile Base tile index (added to tilemap offsets)
 * @param tilemap Tilemap data (row-major, may include flip flags)
 * @param map_width Map width in tiles
 * @param map_height Map height in tiles
 * @param tile_to_palette Per-tile palette lookup (NULL for uniform palette)
 * @param palette Uniform palette (used if tile_to_palette is NULL)
 */
void NGGraphicSetSourceTilemap(NGGraphic *g, u16 base_tile, const u16 *tilemap, u16 map_width,
                               u16 map_height, const u8 *tile_to_palette, u8 palette);

/**
 * Set source from a byte-indexed tilemap (row-major layout).
 * Used for terrain where tile indices are 8-bit.
 *
 * @param g Graphic
 * @param base_tile Base tile index (added to tilemap offsets)
 * @param tilemap Tilemap data (row-major, 8-bit tile indices)
 * @param map_width Map width in tiles
 * @param map_height Map height in tiles
 * @param tile_to_palette Per-tile palette lookup (NULL for uniform palette)
 * @param palette Uniform palette (used if tile_to_palette is NULL)
 */
void NGGraphicSetSourceTilemap8(NGGraphic *g, u16 base_tile, const u8 *tilemap, u16 map_width,
                                u16 map_height, const u8 *tile_to_palette, u8 palette);

/**
 * Set viewport offset into source.
 * For scrolling large tilemaps - only the region starting at (x, y) is rendered.
 *
 * @param g Graphic
 * @param x Source X offset in pixels
 * @param y Source Y offset in pixels
 */
void NGGraphicSetSourceOffset(NGGraphic *g, s16 x, s16 y);

/**
 * Set animation frame.
 *
 * @param g Graphic
 * @param frame Frame index (0-based)
 */
void NGGraphicSetFrame(NGGraphic *g, u16 frame);

/**
 * Mark source as changed, forcing reload on next commit.
 */
void NGGraphicInvalidateSource(NGGraphic *g);
/** @} */

/** @name Transform */
/** @{ */

/**
 * Set screen position (top-left corner).
 *
 * @param g Graphic
 * @param x Screen X coordinate
 * @param y Screen Y coordinate
 */
void NGGraphicSetPosition(NGGraphic *g, s16 x, s16 y);

/**
 * Set display size.
 * May differ from source size for scaling or dynamic resizing.
 *
 * @param g Graphic
 * @param width Display width in pixels
 * @param height Display height in pixels
 */
void NGGraphicSetSize(NGGraphic *g, u16 width, u16 height);

/**
 * Set uniform scale factor.
 * Automatically adjusts display size based on source size.
 *
 * @param g Graphic
 * @param scale Scale factor (256 = 1.0x, 128 = 0.5x, 512 = 2.0x)
 */
void NGGraphicSetScale(NGGraphic *g, u16 scale);

/**
 * Set flip flags.
 *
 * @param g Graphic
 * @param flip Flip flags (can be OR'd together)
 */
void NGGraphicSetFlip(NGGraphic *g, NGGraphicFlip flip);

/**
 * Set Z-order within layer.
 *
 * @param g Graphic
 * @param z Z-order (0 = back, 255 = front within layer)
 */
void NGGraphicSetZOrder(NGGraphic *g, u8 z);

/**
 * Set render layer.
 *
 * @param g Graphic
 * @param layer New layer
 */
void NGGraphicSetLayer(NGGraphic *g, NGGraphicLayer layer);
/** @} */

/** @name 9-Slice Configuration */
/** @{ */

/**
 * Configure 9-slice border sizes in pixels.
 * Only used when tile_mode is NG_GRAPHIC_TILE_9SLICE.
 *
 * @param g Graphic
 * @param top Top border height in pixels
 * @param bottom Bottom border height in pixels
 * @param left Left border width in pixels
 * @param right Right border width in pixels
 */
void NGGraphicSet9SliceBorders(NGGraphic *g, u8 top, u8 bottom, u8 left, u8 right);
/** @} */

/** @name Visibility */
/** @{ */

/**
 * Set visibility.
 *
 * @param g Graphic
 * @param visible 1 = visible, 0 = hidden
 */
void NGGraphicSetVisible(NGGraphic *g, u8 visible);

/**
 * Check if visible.
 *
 * @param g Graphic
 * @return 1 if visible, 0 if hidden
 */
u8 NGGraphicIsVisible(const NGGraphic *g);
/** @} */

/** @name Rendering */
/** @{ */

/**
 * Commit pending changes to backend immediately.
 * Normally not needed - NGGraphicSystemDraw() handles this automatically.
 * Use only when immediate visual update is required.
 *
 * @param g Graphic
 */
void NGGraphicCommit(NGGraphic *g);

/**
 * Force full redraw on next commit.
 */
void NGGraphicInvalidate(NGGraphic *g);
/** @} */

/** @name Queries */
/** @{ */

/**
 * Get current display width.
 *
 * @param g Graphic
 * @return Width in pixels
 */
u16 NGGraphicGetWidth(const NGGraphic *g);

/**
 * Get current display height.
 *
 * @param g Graphic
 * @return Height in pixels
 */
u16 NGGraphicGetHeight(const NGGraphic *g);

/**
 * Get current screen X position.
 *
 * @param g Graphic
 * @return Screen X coordinate
 */
s16 NGGraphicGetX(const NGGraphic *g);

/**
 * Get current screen Y position.
 *
 * @param g Graphic
 * @return Screen Y coordinate
 */
s16 NGGraphicGetY(const NGGraphic *g);
/** @} */

/** @name System Functions (Internal) */
/** @{ */

/**
 * Initialize graphics system.
 * Called by NGEngineInit().
 */
void NGGraphicSystemInit(void);

/**
 * Draw all active graphics in layer/z-order.
 * Handles resource allocation and optimization internally.
 * Called by NGSceneDraw().
 */
void NGGraphicSystemDraw(void);

/**
 * Reset graphics system, destroying all graphics.
 * Called on scene transitions.
 */
void NGGraphicSystemReset(void);
/** @} */

/** @} */ /* end of graphic group */

#endif /* _GRAPHIC_H_ */
