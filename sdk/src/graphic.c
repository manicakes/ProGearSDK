/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file graphic.c
 * @brief NeoGeo backend implementation for graphics abstraction.
 *
 * This file implements the platform-agnostic graphic.h API using NeoGeo
 * hardware sprites via sprite.h. All hardware-specific details are
 * encapsulated here.
 *
 * Key implementation details:
 * - Graphics are stored in a static array and sorted for rendering
 * - Sprite indices are allocated sequentially during draw
 * - Layer determines chained vs independent column mode
 * - Dirty tracking minimizes VRAM writes
 */

#include <graphic.h>
#include <visual.h>
#include <sprite.h>

/* ============================================================
 * Constants
 * ============================================================ */

#define TILE_SIZE         16
#define MAX_SPRITE_HEIGHT 32
#define HW_SPRITE_FIRST   1
#define HW_SPRITE_MAX     380

/* Dirty flags */
#define DIRTY_SOURCE   0x01
#define DIRTY_POSITION 0x02
#define DIRTY_SIZE     0x04
#define DIRTY_SHRINK   0x08
#define DIRTY_ALL      0xFF

/* ============================================================
 * Internal Structure
 * ============================================================ */

struct NGGraphic {
    /* === User-visible state === */

    /* Transform */
    s16 screen_x;
    s16 screen_y;
    u16 display_width;
    u16 display_height;
    u16 scale; /* 256 = 1.0x */
    NGGraphicFlip flip;

    /* Layer and ordering */
    NGGraphicLayer layer;
    u8 z_order;
    u8 visible;

    /* Source configuration */
    u16 base_tile;
    u16 src_width;      /* Source width in pixels */
    u16 src_height;     /* Source height in pixels */
    const u16 *tilemap; /* Row-major tilemap, or NULL */
    const u8 *tile_to_palette;
    u8 palette;
    u16 anim_frame;
    u16 tiles_per_frame;
    s16 src_offset_x; /* Viewport offset into source */
    s16 src_offset_y;

    /* Tile mode and 9-slice */
    NGGraphicTileMode tile_mode;
    u8 slice_top;
    u8 slice_bottom;
    u8 slice_left;
    u8 slice_right;

    /* === Backend-managed state (hidden from user) === */

    /* Hardware resources */
    u16 hw_sprite_first;
    u8 hw_sprite_count;
    u8 hw_allocated;

    /* Computed values (derived from display size and scale) */
    u8 num_cols; /* Sprite columns needed */
    u8 num_rows; /* Tile rows per column */

    /* Dirty tracking */
    u8 dirty;
    u8 active; /* Slot in use? */

    /* Cache for change detection */
    struct {
        u16 last_base_tile;
        u16 last_anim_frame;
        u8 last_palette;
        u8 last_flip;
        s16 last_src_offset_x;
        s16 last_src_offset_y;
        s16 last_screen_x;
        s16 last_screen_y;
        u16 last_display_width;
        u16 last_display_height;
        u16 last_scale;
        u16 last_hw_sprite;
    } cache;
};

/* ============================================================
 * Static State
 * ============================================================ */

static NGGraphic graphics[NG_GRAPHIC_MAX];
static u8 graphics_initialized;

/* Sorted list of active graphic indices for rendering */
static u8 render_order[NG_GRAPHIC_MAX];
static u8 render_count;
static u8 render_order_dirty;

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static u8 pixels_to_tiles(u16 pixels) {
    return (u8)((pixels + TILE_SIZE - 1) / TILE_SIZE);
}

static u16 tiles_to_pixels(u8 tiles) {
    return (u16)(tiles * TILE_SIZE);
}

/**
 * Calculate hardware shrink value from scale.
 * NeoGeo shrink: 255 = full size, 0 = invisible
 * Our scale: 256 = 1.0x, so shrink = scale - 1 (clamped)
 */
static u8 scale_to_shrink(u16 scale) {
    if (scale >= 256)
        return 255;
    if (scale == 0)
        return 0;
    return (u8)(scale - 1);
}

/**
 * Compare function for sorting graphics by layer and z_order.
 * Returns <0 if a should render before b.
 */
static s8 compare_graphics(NGGraphic *a, NGGraphic *b) {
    if (a->layer != b->layer) {
        return (s8)(a->layer - b->layer);
    }
    return (s8)(a->z_order - b->z_order);
}

/**
 * Insertion sort render_order by layer and z.
 */
static void sort_render_order(void) {
    for (u8 i = 1; i < render_count; i++) {
        u8 key = render_order[i];
        NGGraphic *key_g = &graphics[key];

        s8 j = (s8)(i - 1);
        while (j >= 0 && compare_graphics(&graphics[render_order[j]], key_g) > 0) {
            render_order[j + 1] = render_order[j];
            j--;
        }
        render_order[j + 1] = key;
    }
}

/**
 * Rebuild render order from active graphics.
 */
static void rebuild_render_order(void) {
    render_count = 0;
    for (u8 i = 0; i < NG_GRAPHIC_MAX; i++) {
        if (graphics[i].active) {
            render_order[render_count++] = i;
        }
    }
    sort_render_order();
    render_order_dirty = 0;
}

/* ============================================================
 * Tile Writing (NeoGeo-specific)
 * ============================================================ */

/**
 * Get tile and attributes for column-major source (actors, backdrops).
 */
static void get_tile_column_major(NGGraphic *g, u8 col, u8 row, u16 *out_tile, u16 *out_attr) {
    u8 src_tiles_w = pixels_to_tiles(g->src_width);
    u8 src_tiles_h = pixels_to_tiles(g->src_height);

    /* Apply source offset (in tiles) */
    u8 offset_col = (u8)(g->src_offset_x / TILE_SIZE);
    u8 offset_row = (u8)(g->src_offset_y / TILE_SIZE);

    /* Handle tiling/repeat */
    u8 src_col = (u8)((col + offset_col) % src_tiles_w);
    u8 src_row = (u8)((row + offset_row) % src_tiles_h);

    /* Apply flip to source coordinates */
    if (g->flip & NG_GRAPHIC_FLIP_H) {
        src_col = (u8)(src_tiles_w - 1 - src_col);
    }
    if (g->flip & NG_GRAPHIC_FLIP_V) {
        src_row = (u8)(src_tiles_h - 1 - src_row);
    }

    /* Column-major: tile = base + frame_offset + col * height + row */
    u16 frame_offset = g->anim_frame * g->tiles_per_frame;
    u16 tile = (u16)(g->base_tile + frame_offset + (src_col * src_tiles_h) + src_row);

    /* Build attributes: palette + flip bits */
    /* Default h_flip=1 for correct NeoGeo display, invert when user flips */
    u8 hw_h_flip = (g->flip & NG_GRAPHIC_FLIP_H) ? 0 : 1;
    u8 hw_v_flip = (g->flip & NG_GRAPHIC_FLIP_V) ? 1 : 0;

    u8 pal = g->tile_to_palette ? g->tile_to_palette[tile & 0xFFF] : g->palette;
    u16 attr = (u16)(((u16)pal << 8) | (hw_v_flip << 1) | hw_h_flip);

    *out_tile = tile;
    *out_attr = attr;
}

/**
 * Get tile and attributes for row-major tilemap (terrain, UI).
 */
static void get_tile_row_major(NGGraphic *g, u8 col, u8 row, u16 *out_tile, u16 *out_attr) {
    if (!g->tilemap) {
        get_tile_column_major(g, col, row, out_tile, out_attr);
        return;
    }

    u8 src_tiles_w = pixels_to_tiles(g->src_width);
    u8 src_tiles_h = pixels_to_tiles(g->src_height);

    /* Apply source offset (in tiles) */
    u16 offset_col = (u16)(g->src_offset_x / TILE_SIZE);
    u16 offset_row = (u16)(g->src_offset_y / TILE_SIZE);

    /* Map to source coordinates */
    u16 src_col = (u16)(col + offset_col);
    u16 src_row = (u16)(row + offset_row);

    /* Clip mode: check bounds */
    if (g->tile_mode == NG_GRAPHIC_TILE_CLIP) {
        if (src_col >= src_tiles_w || src_row >= src_tiles_h) {
            *out_tile = 0;
            *out_attr = 0;
            return;
        }
    } else {
        /* Repeat mode */
        src_col = src_col % src_tiles_w;
        src_row = src_row % src_tiles_h;
    }

    /* Row-major: index = row * width + col */
    u16 idx = (u16)(src_row * src_tiles_w + src_col);
    u16 entry = g->tilemap[idx];

    /* Extract tile offset and add to base */
    u16 tile_offset = entry & 0x0FFF; /* NG_TILE_MASK */
    u16 tile = g->base_tile + tile_offset;

    /* Build attributes from tilemap flip flags */
    u8 pal = g->tile_to_palette ? g->tile_to_palette[tile_offset] : g->palette;
    u16 attr = (u16)((u16)pal << 8);

    /* Apply tilemap flip flags */
    if (entry & 0x8000)
        attr |= 0x01; /* NG_TILE_HFLIP -> hw h_flip */
    if (entry & 0x4000)
        attr |= 0x02; /* NG_TILE_VFLIP -> hw v_flip */

    *out_tile = tile;
    *out_attr = attr;
}

/**
 * Write tiles for standard/repeat mode.
 */
static void flush_tiles_standard(NGGraphic *g) {
    u16 first_sprite = g->hw_sprite_first;

    for (u8 col = 0; col < g->num_cols; col++) {
        u16 spr = first_sprite + col;

        NGSpriteTileBegin(spr);

        for (u8 row = 0; row < g->num_rows; row++) {
            u16 tile, attr;

            if (g->tilemap) {
                get_tile_row_major(g, col, row, &tile, &attr);
            } else {
                get_tile_column_major(g, col, row, &tile, &attr);
            }

            NGSpriteTileWriteRaw(tile, attr);
        }

        NGSpriteTilePadTo32(g->num_rows);
    }
}

/**
 * Write tiles for 9-slice mode.
 */
static void flush_tiles_9slice(NGGraphic *g) {
    u16 first_sprite = g->hw_sprite_first;
    u8 src_tiles_h = pixels_to_tiles(g->src_height);

    /* Convert pixel borders to tile borders */
    u8 top_rows = pixels_to_tiles(g->slice_top);
    u8 bottom_rows = pixels_to_tiles(g->slice_bottom);

    /* Calculate stretching */
    u8 extra_rows = 0;
    if (g->num_rows > src_tiles_h) {
        extra_rows = g->num_rows - src_tiles_h;
    }

    /* Middle row to repeat (default to center of middle section) */
    u8 stretch_row = top_rows;

    for (u8 col = 0; col < g->num_cols; col++) {
        u16 spr = first_sprite + col;

        NGSpriteTileBegin(spr);

        u8 rows_written = 0;

        /* Top rows */
        for (u8 r = 0; r < top_rows && r < src_tiles_h; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NGSpriteTileWriteRaw(tile, attr);
            rows_written++;
        }

        /* Middle rows with stretching */
        u8 middle_end = src_tiles_h - bottom_rows;
        for (u8 r = top_rows; r < middle_end; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NGSpriteTileWriteRaw(tile, attr);
            rows_written++;

            /* Repeat stretch row */
            if (r == stretch_row) {
                u16 repeat_tile, repeat_attr;
                get_tile_row_major(g, col, stretch_row, &repeat_tile, &repeat_attr);
                for (u8 e = 0; e < extra_rows; e++) {
                    NGSpriteTileWriteRaw(repeat_tile, repeat_attr);
                    rows_written++;
                }
            }
        }

        /* Bottom rows */
        for (u8 r = middle_end; r < src_tiles_h; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NGSpriteTileWriteRaw(tile, attr);
            rows_written++;
        }

        NGSpriteTilePadTo32(rows_written);
    }
}

/**
 * Flush a single graphic to hardware.
 */
static void flush_graphic(NGGraphic *g) {
    if (!g->hw_allocated || !g->visible) {
        return;
    }

    u8 first_draw = (g->hw_sprite_first != g->cache.last_hw_sprite);

    /* Check what needs updating */
    u8 source_changed = (g->dirty & DIRTY_SOURCE) || g->base_tile != g->cache.last_base_tile ||
                        g->anim_frame != g->cache.last_anim_frame ||
                        g->palette != g->cache.last_palette || (u8)g->flip != g->cache.last_flip ||
                        g->src_offset_x != g->cache.last_src_offset_x ||
                        g->src_offset_y != g->cache.last_src_offset_y;

    u8 size_changed = (g->dirty & DIRTY_SIZE) || g->display_width != g->cache.last_display_width ||
                      g->display_height != g->cache.last_display_height;

    u8 scale_changed = (g->dirty & DIRTY_SHRINK) || g->scale != g->cache.last_scale;

    u8 position_changed = (g->dirty & DIRTY_POSITION) || g->screen_x != g->cache.last_screen_x ||
                          g->screen_y != g->cache.last_screen_y;

    /* SCB1: Write tile data */
    if (first_draw || source_changed || size_changed) {
        if (g->tile_mode == NG_GRAPHIC_TILE_9SLICE) {
            flush_tiles_9slice(g);
        } else {
            flush_tiles_standard(g);
        }

        g->cache.last_base_tile = g->base_tile;
        g->cache.last_anim_frame = g->anim_frame;
        g->cache.last_palette = g->palette;
        g->cache.last_flip = (u8)g->flip;
        g->cache.last_src_offset_x = g->src_offset_x;
        g->cache.last_src_offset_y = g->src_offset_y;
    }

    /* SCB2: Shrink values */
    if (first_draw || scale_changed) {
        u8 shrink = scale_to_shrink(g->scale);
        u16 shrink_val = (u16)(((shrink & 0x0F) << 8) | shrink);
        NGSpriteShrinkSet(g->hw_sprite_first, g->num_cols, shrink_val);
        g->cache.last_scale = g->scale;
    }

    /* SCB3/SCB4: Position */
    if (first_draw || position_changed || scale_changed || size_changed) {
        u8 shrink = scale_to_shrink(g->scale);
        u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);

        /* Determine column mode based on layer */
        u8 use_chain = (g->layer == NG_GRAPHIC_LAYER_ENTITY);

        if (use_chain) {
            NGSpriteYSetChain(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        } else {
            NGSpriteYSetUniform(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        }

        /* Calculate tile width based on scale */
        s16 tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
        if (tile_width < 1)
            tile_width = 1;

        NGSpriteXSetSpaced(g->hw_sprite_first, g->num_cols, g->screen_x, tile_width);

        g->cache.last_screen_x = g->screen_x;
        g->cache.last_screen_y = g->screen_y;
    }

    g->cache.last_display_width = g->display_width;
    g->cache.last_display_height = g->display_height;
    g->cache.last_hw_sprite = g->hw_sprite_first;
    g->dirty = 0;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

NGGraphic *NGGraphicCreate(const NGGraphicConfig *config) {
    if (!config || config->width == 0 || config->height == 0) {
        return NULL;
    }

    /* Find free slot */
    NGGraphic *g = NULL;
    for (u8 i = 0; i < NG_GRAPHIC_MAX; i++) {
        if (!graphics[i].active) {
            g = &graphics[i];
            break;
        }
    }

    if (!g) {
        return NULL; /* No free slots */
    }

    /* Initialize to defaults */
    g->screen_x = 0;
    g->screen_y = 0;
    g->display_width = config->width;
    g->display_height = config->height;
    g->scale = NG_GRAPHIC_SCALE_ONE;
    g->flip = NG_GRAPHIC_FLIP_NONE;

    g->layer = config->layer;
    g->z_order = config->z_order;
    g->visible = 1;

    g->base_tile = 0;
    g->src_width = config->width;
    g->src_height = config->height;
    g->tilemap = NULL;
    g->tile_to_palette = NULL;
    g->palette = 0;
    g->anim_frame = 0;
    g->tiles_per_frame = 0;
    g->src_offset_x = 0;
    g->src_offset_y = 0;

    g->tile_mode = config->tile_mode;
    g->slice_top = 16;
    g->slice_bottom = 16;
    g->slice_left = 16;
    g->slice_right = 16;

    g->hw_sprite_first = 0;
    g->hw_sprite_count = 0;
    g->hw_allocated = 0;

    /* Calculate sprite requirements */
    g->num_cols = pixels_to_tiles(config->width);
    g->num_rows = pixels_to_tiles(config->height);
    if (g->num_rows > MAX_SPRITE_HEIGHT) {
        g->num_rows = MAX_SPRITE_HEIGHT;
    }

    g->dirty = DIRTY_ALL;
    g->active = 1;

    /* Invalidate cache */
    g->cache.last_base_tile = 0xFFFF;
    g->cache.last_anim_frame = 0xFFFF;
    g->cache.last_palette = 0xFF;
    g->cache.last_flip = 0xFF;
    g->cache.last_src_offset_x = 0x7FFF;
    g->cache.last_src_offset_y = 0x7FFF;
    g->cache.last_screen_x = 0x7FFF;
    g->cache.last_screen_y = 0x7FFF;
    g->cache.last_display_width = 0xFFFF;
    g->cache.last_display_height = 0xFFFF;
    g->cache.last_scale = 0xFFFF;
    g->cache.last_hw_sprite = 0xFFFF;

    render_order_dirty = 1;

    return g;
}

void NGGraphicDestroy(NGGraphic *g) {
    if (!g || !g->active) {
        return;
    }

    /* Hide sprites if allocated */
    if (g->hw_allocated && g->hw_sprite_count > 0) {
        NGSpriteHideRange(g->hw_sprite_first, g->hw_sprite_count);
    }

    g->active = 0;
    g->hw_allocated = 0;
    render_order_dirty = 1;
}

/* ============================================================
 * Source Configuration
 * ============================================================ */

void NGGraphicSetSource(NGGraphic *g, const NGVisualAsset *asset, u8 palette) {
    if (!g || !asset)
        return;

    g->base_tile = asset->base_tile;
    g->src_width = asset->width_pixels;
    g->src_height = asset->height_pixels;
    g->tilemap = asset->tilemap;
    g->palette = palette;
    g->tiles_per_frame = asset->tiles_per_frame;
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceRaw(NGGraphic *g, u16 base_tile, u16 src_width, u16 src_height, u8 palette) {
    if (!g)
        return;

    g->base_tile = base_tile;
    g->src_width = src_width;
    g->src_height = src_height;
    g->tilemap = NULL;
    g->palette = palette;
    g->tiles_per_frame = (u16)(pixels_to_tiles(src_width) * pixels_to_tiles(src_height));
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceTilemap(NGGraphic *g, u16 base_tile, const u16 *tilemap, u16 map_width,
                               u16 map_height, const u8 *tile_to_palette, u8 palette) {
    if (!g)
        return;

    g->base_tile = base_tile;
    g->tilemap = tilemap;
    g->src_width = tiles_to_pixels((u8)map_width);
    g->src_height = tiles_to_pixels((u8)map_height);
    g->tile_to_palette = tile_to_palette;
    g->palette = palette;
    g->tiles_per_frame = 0;
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceOffset(NGGraphic *g, s16 x, s16 y) {
    if (!g)
        return;

    g->src_offset_x = x;
    g->src_offset_y = y;
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetFrame(NGGraphic *g, u16 frame) {
    if (!g)
        return;

    g->anim_frame = frame;
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicInvalidateSource(NGGraphic *g) {
    if (!g)
        return;
    g->dirty |= DIRTY_SOURCE;
}

/* ============================================================
 * Transform
 * ============================================================ */

void NGGraphicSetPosition(NGGraphic *g, s16 x, s16 y) {
    if (!g)
        return;

    g->screen_x = x;
    g->screen_y = y;
    g->dirty |= DIRTY_POSITION;
}

void NGGraphicSetSize(NGGraphic *g, u16 width, u16 height) {
    if (!g)
        return;

    g->display_width = width;
    g->display_height = height;

    /* Recalculate sprite requirements */
    g->num_cols = pixels_to_tiles(width);
    g->num_rows = pixels_to_tiles(height);
    if (g->num_rows > MAX_SPRITE_HEIGHT) {
        g->num_rows = MAX_SPRITE_HEIGHT;
    }

    g->dirty |= DIRTY_SIZE;
}

void NGGraphicSetScale(NGGraphic *g, u16 scale) {
    if (!g)
        return;

    g->scale = scale;
    g->dirty |= DIRTY_SHRINK;
}

void NGGraphicSetFlip(NGGraphic *g, NGGraphicFlip flip) {
    if (!g)
        return;

    g->flip = flip;
    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetZOrder(NGGraphic *g, u8 z) {
    if (!g)
        return;

    if (g->z_order != z) {
        g->z_order = z;
        render_order_dirty = 1;
    }
}

void NGGraphicSetLayer(NGGraphic *g, NGGraphicLayer layer) {
    if (!g)
        return;

    if (g->layer != layer) {
        g->layer = layer;
        render_order_dirty = 1;
    }
}

/* ============================================================
 * 9-Slice
 * ============================================================ */

void NGGraphicSet9SliceBorders(NGGraphic *g, u8 top, u8 bottom, u8 left, u8 right) {
    if (!g)
        return;

    g->slice_top = top;
    g->slice_bottom = bottom;
    g->slice_left = left;
    g->slice_right = right;
    g->dirty |= DIRTY_SOURCE;
}

/* ============================================================
 * Visibility
 * ============================================================ */

void NGGraphicSetVisible(NGGraphic *g, u8 visible) {
    if (!g)
        return;

    u8 was_visible = g->visible;
    g->visible = visible ? 1 : 0;

    /* Hide sprites when becoming invisible */
    if (was_visible && !g->visible && g->hw_allocated) {
        NGSpriteHideRange(g->hw_sprite_first, g->hw_sprite_count);
    }

    if (!was_visible && g->visible) {
        g->dirty = DIRTY_ALL;
    }
}

u8 NGGraphicIsVisible(const NGGraphic *g) {
    return g ? g->visible : 0;
}

/* ============================================================
 * Rendering
 * ============================================================ */

void NGGraphicCommit(NGGraphic *g) {
    if (!g || !g->active || !g->visible) {
        return;
    }

    if (!g->hw_allocated) {
        return; /* Can't commit without sprite allocation */
    }

    flush_graphic(g);
}

void NGGraphicInvalidate(NGGraphic *g) {
    if (!g)
        return;
    g->dirty = DIRTY_ALL;
}

/* ============================================================
 * Queries
 * ============================================================ */

u16 NGGraphicGetWidth(const NGGraphic *g) {
    return g ? g->display_width : 0;
}

u16 NGGraphicGetHeight(const NGGraphic *g) {
    return g ? g->display_height : 0;
}

s16 NGGraphicGetX(const NGGraphic *g) {
    return g ? g->screen_x : 0;
}

s16 NGGraphicGetY(const NGGraphic *g) {
    return g ? g->screen_y : 0;
}

/* ============================================================
 * System Functions
 * ============================================================ */

void _NGGraphicSystemInit(void) {
    for (u8 i = 0; i < NG_GRAPHIC_MAX; i++) {
        graphics[i].active = 0;
        graphics[i].hw_allocated = 0;
    }
    render_count = 0;
    render_order_dirty = 1;
    graphics_initialized = 1;
}

void _NGGraphicSystemDraw(void) {
    if (!graphics_initialized) {
        return;
    }

    /* Rebuild render order if needed */
    if (render_order_dirty) {
        rebuild_render_order();
    }

    /* Allocate sprites in render order (back to front) */
    u16 sprite_idx = HW_SPRITE_FIRST;

    for (u8 i = 0; i < render_count; i++) {
        NGGraphic *g = &graphics[render_order[i]];

        if (!g->visible) {
            /* Hide previously allocated sprites */
            if (g->hw_allocated && g->hw_sprite_count > 0) {
                NGSpriteHideRange(g->hw_sprite_first, g->hw_sprite_count);
                g->hw_allocated = 0;
            }
            continue;
        }

        /* Check if we have room */
        u8 needed = g->num_cols;
        if (sprite_idx + needed > HW_SPRITE_MAX) {
            /* Out of sprites - skip this graphic */
            continue;
        }

        /* Allocate sprites */
        u8 sprite_moved = (g->hw_sprite_first != sprite_idx);
        u8 count_changed = (g->hw_sprite_count != needed);

        if (sprite_moved || count_changed || !g->hw_allocated) {
            g->hw_sprite_first = sprite_idx;
            g->hw_sprite_count = needed;
            g->hw_allocated = 1;
            g->dirty = DIRTY_ALL; /* Force full redraw */
        }

        sprite_idx += needed;

        /* Flush to hardware */
        flush_graphic(g);
    }

    /* Hide unused sprites */
    if (sprite_idx < HW_SPRITE_MAX) {
        u16 remaining = (u16)(HW_SPRITE_MAX - sprite_idx);
        while (remaining > 0) {
            u8 batch = (remaining > 255) ? 255 : (u8)remaining;
            NGSpriteHideRange(sprite_idx, batch);
            sprite_idx += batch;
            remaining -= batch;
        }
    }
}

void _NGGraphicSystemReset(void) {
    /* Hide all sprites */
    NGSpriteHideRange(0, 255);
    NGSpriteHideRange(255, (u8)(HW_SPRITE_MAX - 255));

    /* Reset all graphics */
    for (u8 i = 0; i < NG_GRAPHIC_MAX; i++) {
        graphics[i].active = 0;
        graphics[i].hw_allocated = 0;
    }

    render_count = 0;
    render_order_dirty = 1;
}
