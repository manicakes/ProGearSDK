/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file graphic.h
 * @brief Sprite-group manager for NeoGeo hardware sprites.
 *
 * NGGraphic owns a rectangular group of NeoGeo hardware sprites and presents it
 * in pixel terms instead of tiles and Sprite Control Block indices. It handles
 * allocating the hardware sprites, packing tile data into VRAM, scaling via the
 * hardware shrink registers, and dirty-tracking so only changed registers are
 * rewritten each frame.
 *
 * Actors, backdrops, terrain, and UI panels are all built on top of NGGraphic,
 * so sprite allocation and the SCB write patterns live in one place.
 *
 * Design notes:
 * - Dimensions are in pixels; the tile/SCB layout is internal.
 * - Hardware sprite allocation is automatic; callers never see sprite indices.
 * - The layer selects how columns are positioned: entity columns are chained
 *   with the hardware sticky bit, others are placed independently.
 * - Dirty tracking minimizes per-frame VRAM writes.
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

#ifndef NG_GRAPHIC_H
#define NG_GRAPHIC_H

#include <ng_types.h>
#include <visual.h>

/**
 * @defgroup graphic Graphics API
 * @ingroup sdk
 * @brief NeoGeo sprite-group management.
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
 * Determines rendering order and how columns are positioned.
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
 * Commit pending changes to sprite hardware immediately.
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

/**
 * Get the hardware sprite range currently backing a graphic.
 *
 * Advanced accessor for raster effects: a timer interrupt handler can
 * rewrite the SCB registers of these sprites mid-frame (e.g. per-scanline
 * X displacement for heat-haze or water effects).
 *
 * The range is only valid until the next NGGraphicSystemDraw() — sprites
 * are reallocated whenever the set of visible graphics changes, so query
 * the range every frame rather than caching it.
 *
 * @param g Graphic
 * @param first_sprite Out: first hardware sprite index (may be NULL)
 * @param count Out: number of consecutive sprites (may be NULL)
 * @return 1 if the graphic has sprites allocated, 0 otherwise
 */
u8 NGGraphicGetSpriteRange(const NGGraphic *g, u16 *first_sprite, u8 *count);

/**
 * Snapshot of the SCB4 X layout a graphic last flushed to hardware.
 *
 * A raster interrupt can re-create the graphic's column X positions with
 * NGSpriteXSetSpaced(first_sprite, count, base_x + offset, spacing) to
 * displace it per scanline band.
 */
typedef struct {
    u16 first_sprite; /**< First hardware sprite index */
    u8 count;         /**< X values to write (1 for chained entity strips) */
    s16 base_x;       /**< Screen X of the first column */
    s16 spacing;      /**< X spacing between columns (scaled tile width) */
} NGGraphicRasterXInfo;

/**
 * Get the info needed to displace a graphic's columns mid-frame.
 *
 * Advanced accessor for raster effects on scene-managed graphics (actors,
 * backdrops). Mirrors the X layout the flush path writes: chained entity
 * strips need a single head write, other layers one write per column at the
 * scaled tile spacing.
 *
 * The values reflect the graphic's last committed state and are refreshed
 * by NGGraphicSystemDraw(), so query every frame and expect them to lag a
 * frame behind mid-update positions.
 *
 * @param g Graphic
 * @param info Out: raster displacement info
 * @return 1 if the graphic has sprites allocated, 0 otherwise
 */
u8 NGGraphicGetRasterXInfo(const NGGraphic *g, NGGraphicRasterXInfo *info);
/** @} */

/** @} */ /* end of graphic group */

#endif /* NG_GRAPHIC_H */
