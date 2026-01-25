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
#include <ng_sprite.h>
#include <ng_palette.h>
#include <ng_hardware.h>

/* ============================================================
 * Constants
 * ============================================================ */

#define TILE_SIZE         16
#define TILE_SHIFT        4 /* log2(TILE_SIZE) for fast division */
#define MAX_SPRITE_HEIGHT 32
#define HW_SPRITE_FIRST   1
#define HW_SPRITE_MAX     380

/* Reserve sprites for UI layer to prevent shifts when entity layer changes.
 * UI sprites are allocated from the back of the pool, entities from the front.
 * This prevents UI redraws when entities are added/removed. */
#define UI_SPRITE_POOL_SIZE 64
#define UI_SPRITE_FIRST     (HW_SPRITE_MAX - UI_SPRITE_POOL_SIZE)

/* Dirty flags */
#define DIRTY_SOURCE 0x01
#define DIRTY_SIZE   0x04
#define DIRTY_SHRINK 0x08
#define DIRTY_ALL    0xFF

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
    const u16 *tilemap; /* Row-major tilemap (u16), or NULL */
    const u8 *tilemap8; /* Row-major tilemap (u8 indices), or NULL */
    const u8 *tile_to_palette;
    u8 palette;
    u16 anim_frame;
    u16 tiles_per_frame;
    s16 src_offset_x; /* Viewport offset into source */
    s16 src_offset_y;

    /* Precomputed values for fast tile lookup (avoids division in inner loop) */
    u8 src_tiles_w;     /* Source width in tiles */
    u8 src_tiles_h;     /* Source height in tiles */
    u16 effective_base; /* base_tile + (anim_frame * tiles_per_frame) */

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

    /* Infinite scroll state (circular buffer) */
    u8 scroll_leftmost;   /* Index of leftmost column (0 to num_cols-1) */
    u8 scroll_topmost;    /* Index of topmost row (0 to num_rows-1) for Y cycling */
    u8 tiles_loaded;      /* Tiles written flag */
    s16 scroll_offset;    /* Sub-tile offset (4 fractional bits) */
    s16 scroll_last_px;   /* Last scroll position for delta calc (X tile column) */
    s16 scroll_last_row;  /* Last Y tile row for delta calc */
    u16 scroll_last_scb3; /* Cached SCB3 value for Y optimization */

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
        u8 last_visible_cols; /* For infinite scroll: columns rendered at current scale */
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
    return (u8)((pixels + TILE_SIZE - 1) >> TILE_SHIFT);
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
 * Calculate combined SCB2 shrink value from scale.
 * Returns (h_shrink_8bit << 8) | v_shrink_8bit for NGSpriteShrinkSet.
 * NGSpriteShrinkSet distributes the 8-bit h_shrink across sprite columns
 * for smooth scaling (see Scaling_sprite_groups on NeoGeo dev wiki).
 */
static u16 scale_to_shrink_val(u16 scale) {
    u8 shrink = scale_to_shrink(scale);
    return (u16)(((u16)shrink << 8) | shrink);
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
 * Uses precomputed src_tiles_w/h and effective_base to avoid division/multiply in inner loop.
 */
static void get_tile_column_major(NGGraphic *g, u8 col, u8 row, u16 *out_tile, u16 *out_attr) {
    u8 src_tiles_w = g->src_tiles_w;
    u8 src_tiles_h = g->src_tiles_h;

    /* Apply source offset (in tiles) with proper negative handling */
    s16 offset_col = g->src_offset_x >> TILE_SHIFT;
    s16 offset_row = g->src_offset_y >> TILE_SHIFT;

    /* Handle tiling/repeat with proper modulo for negative values */
    s16 temp_col = (s16)(((s16)col + offset_col) % (s16)src_tiles_w);
    s16 temp_row = (s16)(((s16)row + offset_row) % (s16)src_tiles_h);
    if (temp_col < 0)
        temp_col += src_tiles_w;
    if (temp_row < 0)
        temp_row += src_tiles_h;
    u8 src_col = (u8)temp_col;
    u8 src_row = (u8)temp_row;

    /* Apply flip to source coordinates */
    if (g->flip & NG_GRAPHIC_FLIP_H) {
        src_col = (u8)(src_tiles_w - 1 - src_col);
    }
    if (g->flip & NG_GRAPHIC_FLIP_V) {
        src_row = (u8)(src_tiles_h - 1 - src_row);
    }

    /* Column-major: tile = effective_base + col * height + row */
    /* effective_base already includes base_tile + frame_offset */
    u16 tile = (u16)(g->effective_base + (src_col * src_tiles_h) + src_row);

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
 * Uses precomputed src_tiles_w/h and effective_base to avoid division/multiply in inner loop.
 */
static void get_tile_row_major(NGGraphic *g, u8 col, u8 row, u16 *out_tile, u16 *out_attr) {
    if (!g->tilemap && !g->tilemap8) {
        get_tile_column_major(g, col, row, out_tile, out_attr);
        return;
    }

    u8 src_tiles_w = g->src_tiles_w;
    u8 src_tiles_h = g->src_tiles_h;

    /* Apply source offset (in tiles) with proper negative handling */
    s16 offset_col = g->src_offset_x >> TILE_SHIFT;
    s16 offset_row = g->src_offset_y >> TILE_SHIFT;

    /* Map to source coordinates */
    s16 temp_col = (s16)col + offset_col;
    s16 temp_row = (s16)row + offset_row;

    /* Clip mode: check bounds */
    if (g->tile_mode == NG_GRAPHIC_TILE_CLIP) {
        if (temp_col < 0 || temp_col >= src_tiles_w || temp_row < 0 || temp_row >= src_tiles_h) {
            *out_tile = 0;
            *out_attr = 0;
            return;
        }
    } else {
        /* Repeat mode with proper modulo for negative values */
        temp_col = temp_col % (s16)src_tiles_w;
        temp_row = temp_row % (s16)src_tiles_h;
        if (temp_col < 0)
            temp_col += src_tiles_w;
        if (temp_row < 0)
            temp_row += src_tiles_h;
    }

    /* Row-major: index = row * width + col */
    u16 idx = (u16)((u16)temp_row * src_tiles_w + (u16)temp_col);

    u16 tile;
    u8 pal;

    if (g->tilemap8) {
        /* 8-bit tilemap (terrain): simple index lookup, no flip flags */
        u8 tile_idx = g->tilemap8[idx];
        tile = g->effective_base + tile_idx;
        pal = g->tile_to_palette ? g->tile_to_palette[tile_idx] : g->palette;

        /* Default h_flip for correct display, no v_flip */
        *out_tile = tile;
        *out_attr = (u16)(((u16)pal << 8) | 0x01);
        return;
    }

    /* 16-bit tilemap with flip flags */
    u16 entry = g->tilemap[idx];

    /* Extract tile offset and add to effective_base (includes frame offset) */
    u16 tile_offset = entry & 0x0FFF; /* NG_TILE_MASK */
    tile = (u16)(g->effective_base + tile_offset);

    /* Build attributes from tilemap flip flags */
    pal = g->tile_to_palette ? g->tile_to_palette[tile_offset] : g->palette;
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
 * Fast path for simple animated sprites with 16-bit tilemaps.
 * Conditions: has tilemap, no source offset, no flip, no tile_to_palette.
 * This covers the common case of animated sprites like the ball.
 */
static void flush_tiles_tilemap_fast(NGGraphic *g) {
    NG_VRAM_DECLARE_BASE();

    u16 first_sprite = g->hw_sprite_first;
    u8 src_tiles_w = g->src_tiles_w;
    u8 src_tiles_h = g->src_tiles_h;
    u16 effective_base = g->effective_base;
    const u16 *tilemap = g->tilemap;
    u16 base_attr = (u16)((u16)g->palette << 8);

    /* Check if wrapping is needed (avoids expensive modulo on 68000) */
    u8 needs_wrap = (g->num_cols > src_tiles_w) || (g->num_rows > src_tiles_h);

    for (u8 col = 0; col < g->num_cols; col++) {
        u8 src_col = needs_wrap ? (col % src_tiles_w) : col;

        /* Set VRAM address for this sprite column (SCB1 base + sprite * 64) */
        NG_VRAM_SETUP_FAST(NG_SCB1_BASE + ((first_sprite + col) * 64), 1);

        for (u8 row = 0; row < g->num_rows; row++) {
            u8 src_row = needs_wrap ? (row % src_tiles_h) : row;

            /* Row-major index: row * width + col */
            u16 idx = (u16)((u16)src_row * src_tiles_w + src_col);
            u16 entry = tilemap[idx];

            /* Extract tile and compute final tile index */
            u16 tile = (u16)(effective_base + (entry & 0x0FFF));

            /* Build attr from tilemap flip flags */
            u16 attr = base_attr;
            if (entry & 0x8000)
                attr |= 0x01; /* h_flip */
            if (entry & 0x4000)
                attr |= 0x02; /* v_flip */

            /* Write tile and attr (auto-increment handles addressing) */
            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
        }

        /* Pad remaining tiles to 32 */
        if (g->num_rows < 32) {
            NG_VRAM_CLEAR_FAST((32 - g->num_rows) * 2);
        }
    }
}

/**
 * Write tiles for standard/repeat mode.
 * Uses fast path when possible, falls back to generic path otherwise.
 */
static void flush_tiles_standard(NGGraphic *g) {
    /* Fast path: simple animated sprites with 16-bit tilemaps */
    if (g->tilemap && !g->tilemap8 && !g->tile_to_palette && g->src_offset_x == 0 &&
        g->src_offset_y == 0 && g->flip == NG_GRAPHIC_FLIP_NONE) {
        flush_tiles_tilemap_fast(g);
        return;
    }

    /* Generic path for complex cases */
    NG_VRAM_DECLARE_BASE();
    u16 first_sprite = g->hw_sprite_first;

    for (u8 col = 0; col < g->num_cols; col++) {
        /* Set VRAM address for this sprite column */
        NG_VRAM_SETUP_FAST(NG_SCB1_BASE + ((first_sprite + col) * 64), 1);

        for (u8 row = 0; row < g->num_rows; row++) {
            u16 tile, attr;

            if (g->tilemap || g->tilemap8) {
                get_tile_row_major(g, col, row, &tile, &attr);
            } else {
                get_tile_column_major(g, col, row, &tile, &attr);
            }

            /* Write tile and attr (auto-increment handles addressing) */
            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
        }

        /* Pad remaining tiles to 32 */
        if (g->num_rows < 32) {
            NG_VRAM_CLEAR_FAST((32 - g->num_rows) * 2);
        }
    }
}

/**
 * Write tiles for 9-slice mode.
 * Optimized to use direct VRAM writes instead of function calls.
 */
static void flush_tiles_9slice(NGGraphic *g) {
    NG_VRAM_DECLARE_BASE();

    u16 first_sprite = g->hw_sprite_first;
    u8 src_tiles_h = g->src_tiles_h;

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
    u8 middle_end = src_tiles_h - bottom_rows;

    for (u8 col = 0; col < g->num_cols; col++) {
        /* Set VRAM address for this sprite column (SCB1 base + sprite * 64) */
        NG_VRAM_SETUP_FAST(NG_SCB1_BASE + ((first_sprite + col) * 64), 1);

        u8 rows_written = 0;

        /* Top rows */
        for (u8 r = 0; r < top_rows && r < src_tiles_h; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
            rows_written++;
        }

        /* Middle rows with stretching */
        for (u8 r = top_rows; r < middle_end; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
            rows_written++;

            /* Repeat stretch row - reuse tile/attr already fetched above */
            if (r == stretch_row) {
                for (u8 e = 0; e < extra_rows; e++) {
                    NG_VRAM_WRITE_FAST(tile);
                    NG_VRAM_WRITE_FAST(attr);
                    rows_written++;
                }
            }
        }

        /* Bottom rows */
        for (u8 r = middle_end; r < src_tiles_h; r++) {
            u16 tile, attr;
            get_tile_row_major(g, col, r, &tile, &attr);
            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
            rows_written++;
        }

        /* Pad remaining tiles to 32 */
        if (rows_written < 32) {
            NG_VRAM_CLEAR_FAST((32 - rows_written) * 2);
        }
    }
}

/* ============================================================
 * Infinite Scroll (Circular Buffer)
 * ============================================================ */

#define SCROLL_FRAC_BITS 4
#define SCROLL_FIX(x)    ((s16)((x) << SCROLL_FRAC_BITS))
#define SCROLL_INT(x)    ((s16)((x) >> SCROLL_FRAC_BITS))

/**
 * Update X positions for infinite scroll using circular buffer.
 * Only writes SCB4 for visible_cols sprites - saves bandwidth at higher zoom.
 *
 * @param g            The graphic
 * @param pixel_diff   Pixels scrolled since last update
 * @param tile_width   Current tile width based on scale
 * @param visible_cols Number of columns to actually render (may be less than num_cols)
 */
static void update_scroll_positions_limited(NGGraphic *g, s16 pixel_diff, s16 tile_width,
                                            u8 visible_cols) {
    s16 tile_width_fixed = SCROLL_FIX(tile_width);

    /* Update scroll offset by pixel delta */
    g->scroll_offset = (s16)(g->scroll_offset - SCROLL_FIX(pixel_diff));

    /* Wrap leftmost pointer when crossing tile boundaries */
    while (g->scroll_offset <= 0) {
        g->scroll_leftmost++;
        if (g->scroll_leftmost >= visible_cols) {
            g->scroll_leftmost = 0;
        }
        g->scroll_offset = (s16)(g->scroll_offset + tile_width_fixed);
    }

    while (g->scroll_offset > tile_width_fixed * 2) {
        if (g->scroll_leftmost == 0) {
            g->scroll_leftmost = visible_cols;
        }
        g->scroll_leftmost--;
        g->scroll_offset = (s16)(g->scroll_offset - tile_width_fixed);
    }

    /* Calculate X positions mathematically from scroll state */
    s16 base_left_x = (s16)(SCROLL_INT(g->scroll_offset) - 2 * tile_width);

    NGSpriteXBegin(g->hw_sprite_first);
    for (u8 i = 0; i < visible_cols; i++) {
        u8 screen_col = (u8)((i - g->scroll_leftmost + visible_cols) % visible_cols);
        s16 x = (s16)(base_left_x + screen_col * tile_width);
        NGSpriteXWriteNext(x);
    }
}

/**
 * Calculate number of visible columns needed at current scale.
 * Always renders complete copies of the asset (no partial repetitions).
 * This ensures seamless tiling without cut-off text/graphics.
 */
static u8 calc_visible_cols(u8 max_cols, u8 src_tiles_w, s16 tile_width) {
    /* Columns needed to fill screen = ceil(screen_width / tile_width) + 2 buffer */
    u8 screen_cols = (u8)(((SCREEN_WIDTH + tile_width - 1) / tile_width) + 2);
    /* Round up to multiple of asset width for complete repetitions */
    u8 repetitions = (u8)((screen_cols + src_tiles_w - 1) / src_tiles_w);
    u8 needed = repetitions * src_tiles_w;
    return (needed > max_cols) ? max_cols : needed;
}

/**
 * Flush infinite scroll graphic - optimized for scrolling backdrops.
 * Tiles are written once, scrolling only updates X positions.
 * Only renders columns needed at current zoom level to save sprites.
 */
static void flush_infinite_scroll(NGGraphic *g) {
    u8 first_draw = (g->hw_sprite_first != g->cache.last_hw_sprite);

    /* Detect sprite reallocation - need to reload tiles */
    if (g->tiles_loaded && first_draw) {
        g->tiles_loaded = 0;
    }

    s16 tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
    if (tile_width < 1)
        tile_width = 1;

    /* Calculate how many columns are actually visible at current scale */
    u8 visible_cols = calc_visible_cols(g->num_cols, g->src_tiles_w, tile_width);

    /* First draw or tiles invalidated - write tiles once */
    if (!g->tiles_loaded) {
        /* SCB1: Write tile data for all columns (needed for zoom out) */
        flush_tiles_standard(g);

        /* SCB2: Shrink values for visible columns only */
        NGSpriteShrinkSet(g->hw_sprite_first, visible_cols, scale_to_shrink_val(g->scale));

        /* SCB3: Set Y/height for visible columns, hide extras */
        u8 shrink = scale_to_shrink(g->scale);
        u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);
        NGSpriteYSetUniform(g->hw_sprite_first, visible_cols, g->screen_y, hw_height);
        if (visible_cols < g->num_cols) {
            NGSpriteHideRange(g->hw_sprite_first + visible_cols, g->num_cols - visible_cols);
        }

        /* SCB4: Initial X positions - start one tile left for buffer */
        NGSpriteXSetSpaced(g->hw_sprite_first, visible_cols, -tile_width, tile_width);

        /* Initialize scroll state */
        g->scroll_leftmost = 0;
        g->scroll_offset = SCROLL_FIX(tile_width); /* Start mid-range */
        g->scroll_last_px = g->src_offset_x;
        g->scroll_last_scb3 = 0xFFFF;
        g->tiles_loaded = 1;
        g->cache.last_visible_cols = visible_cols;

        g->cache.last_scale = g->scale;
        g->cache.last_hw_sprite = g->hw_sprite_first;
    }

    /* Handle scale changes */
    if (g->scale != g->cache.last_scale) {
        tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
        if (tile_width < 1)
            tile_width = 1;
        visible_cols = calc_visible_cols(g->num_cols, g->src_tiles_w, tile_width);

        /* SCB2: Update shrink for visible columns */
        NGSpriteShrinkSet(g->hw_sprite_first, visible_cols, scale_to_shrink_val(g->scale));

        /* SCB3: Update Y/height for visible columns, hide/show as needed */
        u8 shrink = scale_to_shrink(g->scale);
        u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);
        NGSpriteYSetUniform(g->hw_sprite_first, visible_cols, g->screen_y, hw_height);
        if (visible_cols < g->num_cols) {
            NGSpriteHideRange(g->hw_sprite_first + visible_cols, g->num_cols - visible_cols);
        }

        /* SCB4: Reset X positions for new tile width */
        NGSpriteXSetSpaced(g->hw_sprite_first, visible_cols, -tile_width, tile_width);
        g->scroll_leftmost = 0;
        g->scroll_offset = SCROLL_FIX(tile_width);
        g->scroll_last_px = g->src_offset_x;
        g->scroll_last_scb3 = NGSpriteSCB3(g->screen_y, hw_height);
        g->cache.last_visible_cols = visible_cols;

        g->cache.last_scale = g->scale;
    }

    /* SCB3: Y position - only write if changed */
    u8 shrink = scale_to_shrink(g->scale);
    u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);
    u16 scb3_val = NGSpriteSCB3(g->screen_y, hw_height);

    if (scb3_val != g->scroll_last_scb3) {
        NGSpriteYSetUniform(g->hw_sprite_first, visible_cols, g->screen_y, hw_height);
        g->scroll_last_scb3 = scb3_val;
    }

    /* Handle scrolling - only updates X positions, no tile rewrites! */
    s16 scroll_px = g->src_offset_x;
    s16 pixel_diff = (s16)(scroll_px - g->scroll_last_px);

    if (pixel_diff != 0) {
        update_scroll_positions_limited(g, pixel_diff, tile_width, visible_cols);
        g->scroll_last_px = scroll_px;
    }

    g->dirty = 0;
}

/* ============================================================
 * Tilemap Scroll (Cycling Buffer for Terrain)
 * ============================================================ */

/**
 * Load tile data for a single sprite column from tilemap8.
 * Writes tiles in Y cycling order to support efficient vertical scrolling.
 * Used by cycling buffer to update only changed columns.
 */
static void load_tilemap8_column(NGGraphic *g, u16 sprite_idx, s16 src_col) {
    u8 src_tiles_w = g->src_tiles_w;
    u8 src_tiles_h = g->src_tiles_h;
    u16 effective_base = g->effective_base;
    const u8 *tilemap8 = g->tilemap8;
    const u8 *tile_to_palette = g->tile_to_palette;
    u8 default_pal = g->palette;
    s16 src_row_offset = g->src_offset_y >> TILE_SHIFT;
    u8 topmost = g->scroll_topmost;
    u8 num_rows = g->num_rows;

    NGSpriteTileBegin(sprite_idx);

    /* Write tiles in Y cycling order.
     * Slot scroll_topmost contains display_row 0 (top of visible area).
     * Formula: display_row = (slot - topmost + num_rows) % num_rows */
    for (u8 slot = 0; slot < num_rows; slot++) {
        /* Map slot to display row via cycling buffer */
        u8 display_row = (u8)((slot + num_rows - topmost) % num_rows);
        s16 src_row = (s16)display_row + src_row_offset;

        /* Clip mode: check bounds */
        if (src_col < 0 || src_col >= src_tiles_w || src_row < 0 || src_row >= src_tiles_h) {
            NGSpriteTileWriteEmpty();
            continue;
        }

        u16 idx = (u16)((u16)src_row * src_tiles_w + (u16)src_col);
        u8 tile_idx = tilemap8[idx];
        u16 tile = effective_base + tile_idx;
        u8 pal = tile_to_palette ? tile_to_palette[tile_idx] : default_pal;

        /* Default h_flip for correct display */
        NGSpriteTileWrite(tile, pal, 1, 0);
    }

    NGSpriteTilePadTo32(num_rows);
}

/**
 * Update a single row of tiles across all columns.
 * Used for efficient Y-scroll updates instead of reloading entire columns.
 * Only writes one tile per column - O(columns) instead of O(columns × rows).
 *
 * @param g          The graphic
 * @param src_row    Source row in tilemap to read from
 * @param slot       Tile slot within sprite columns to write to (0 to num_rows-1)
 */
static void update_tilemap8_row(NGGraphic *g, s16 src_row, u8 slot) {
    NG_VRAM_DECLARE_BASE();

    u8 src_tiles_w = g->src_tiles_w;
    u8 src_tiles_h = g->src_tiles_h;
    u16 effective_base = g->effective_base;
    const u8 *tilemap8 = g->tilemap8;
    const u8 *tile_to_palette = g->tile_to_palette;
    u8 default_pal = g->palette;

    s16 cur_tile_col = g->src_offset_x >> TILE_SHIFT;

    /* For each screen column, update the tile at the specified slot */
    for (u8 col = 0; col < g->num_cols; col++) {
        /* Map screen column to sprite index (account for X cycling) */
        u8 sprite_offset = (u8)((g->scroll_leftmost + col) % g->num_cols);
        u16 sprite_idx = g->hw_sprite_first + sprite_offset;

        /* Calculate source column */
        s16 src_col = cur_tile_col + col;

        /* Set VRAM address for this specific tile slot */
        /* SCB1: sprite * 64 words + slot * 2 words */
        NG_VRAM_SETUP_FAST(NG_SCB1_BASE + (sprite_idx * 64) + (slot * 2), 1);

        /* Clip mode: check bounds */
        if (src_col < 0 || src_col >= src_tiles_w || src_row < 0 || src_row >= src_tiles_h) {
            /* Write empty tile */
            NG_VRAM_WRITE_FAST(0);
            NG_VRAM_WRITE_FAST(0);
        } else {
            u16 idx = (u16)((u16)src_row * src_tiles_w + (u16)src_col);
            u8 tile_idx = tilemap8[idx];
            u16 tile = effective_base + tile_idx;
            u8 pal = tile_to_palette ? tile_to_palette[tile_idx] : default_pal;
            u16 attr = (u16)(((u16)pal << 8) | 0x01); /* Default h_flip */

            NG_VRAM_WRITE_FAST(tile);
            NG_VRAM_WRITE_FAST(attr);
        }
    }
}

/**
 * Flush tilemap with cycling buffer - optimized for scrolling terrain.
 * Uses cycling buffers for both X and Y to minimize tile updates:
 * - X scroll: only updates the column(s) that enter view
 * - Y scroll: only updates the row(s) that enter view
 */
static void flush_tilemap_scroll(NGGraphic *g) {
    u8 first_draw = (g->hw_sprite_first != g->cache.last_hw_sprite);

    /* Detect sprite reallocation - need to reload all tiles */
    if (g->tiles_loaded && first_draw) {
        g->tiles_loaded = 0;
    }

    s16 tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
    if (tile_width < 1)
        tile_width = 1;

    /* Current tile offsets */
    s16 cur_tile_col = g->src_offset_x >> TILE_SHIFT;
    s16 cur_tile_row = g->src_offset_y >> TILE_SHIFT;

    /* First draw or tiles invalidated - write all tiles and initialize state */
    if (!g->tiles_loaded) {
        /* Initialize Y cycling state before loading columns */
        g->scroll_topmost = 0;
        g->scroll_leftmost = 0;

        /* Load tiles for all columns */
        for (u8 col = 0; col < g->num_cols; col++) {
            s16 src_col = cur_tile_col + col;
            load_tilemap8_column(g, g->hw_sprite_first + col, src_col);
        }

        /* SCB2: Shrink values */
        NGSpriteShrinkSet(g->hw_sprite_first, g->num_cols, scale_to_shrink_val(g->scale));

        /* Initialize cycling state */
        g->scroll_last_px = cur_tile_col;
        g->scroll_last_row = cur_tile_row;
        g->scroll_last_scb3 = 0xFFFF;
        g->tiles_loaded = 1;

        g->cache.last_scale = g->scale;
        g->cache.last_src_offset_x = g->src_offset_x;
        g->cache.last_src_offset_y = g->src_offset_y;
        g->cache.last_hw_sprite = g->hw_sprite_first;
    }

    /* Handle scale changes - reload all tiles */
    if (g->scale != g->cache.last_scale) {
        NGSpriteShrinkSet(g->hw_sprite_first, g->num_cols, scale_to_shrink_val(g->scale));

        /* Recalculate tile width */
        tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
        if (tile_width < 1)
            tile_width = 1;

        /* Reset cycling state and reload all tiles */
        g->scroll_topmost = 0;
        g->scroll_leftmost = 0;
        for (u8 col = 0; col < g->num_cols; col++) {
            s16 src_col = cur_tile_col + col;
            load_tilemap8_column(g, g->hw_sprite_first + col, src_col);
        }

        g->scroll_last_px = cur_tile_col;
        g->scroll_last_row = cur_tile_row;
        g->scroll_last_scb3 = 0xFFFF;
        g->cache.last_scale = g->scale;
    }

    /* Handle vertical scrolling with Y cycling buffer */
    /* Only update the specific row(s) that enter view - O(columns) per row */
    s16 last_tile_row = g->scroll_last_row;
    s16 row_delta = cur_tile_row - last_tile_row;

    if (row_delta != 0) {
        u8 num_rows = g->num_rows;

        if (row_delta > 0) {
            /* Scrolling down: new rows appear at the bottom */
            for (s16 i = 0; i < row_delta && i < (s16)num_rows; i++) {
                /* Source row that just came into view at the bottom */
                s16 src_row = (s16)(last_tile_row + (s16)num_rows + i);

                /* The topmost slot will become the new bottom slot */
                u8 slot = g->scroll_topmost;

                /* Update just this row across all columns */
                update_tilemap8_row(g, src_row, slot);

                /* Advance topmost (row that was at top scrolled out) */
                g->scroll_topmost = (u8)((g->scroll_topmost + 1) % num_rows);
            }
        } else {
            /* Scrolling up: new rows appear at the top */
            for (s16 i = 0; i > row_delta && i > -(s16)num_rows; i--) {
                /* Move topmost back to make room for new row */
                if (g->scroll_topmost == 0) {
                    g->scroll_topmost = num_rows;
                }
                g->scroll_topmost--;

                /* Source row that just came into view at the top */
                s16 src_row = cur_tile_row - i;

                /* Update just this row across all columns */
                update_tilemap8_row(g, src_row, g->scroll_topmost);
            }
        }
        g->scroll_last_row = cur_tile_row;
        g->cache.last_src_offset_y = g->src_offset_y;
    }

    /* Handle horizontal scrolling with X cycling buffer */
    s16 last_tile_col = g->scroll_last_px;
    s16 col_delta = cur_tile_col - last_tile_col;

    if (col_delta != 0) {
        if (col_delta > 0) {
            /* Scrolling right: load new columns on the right */
            for (s16 i = 0; i < col_delta && i < (s16)g->num_cols; i++) {
                /* The leftmost sprite will become the new rightmost */
                u8 sprite_offset = g->scroll_leftmost;
                u16 spr = g->hw_sprite_first + sprite_offset;
                s16 new_col = (s16)(last_tile_col + (s16)g->num_cols + i);

                load_tilemap8_column(g, spr, new_col);

                g->scroll_leftmost = (u8)((g->scroll_leftmost + 1) % g->num_cols);
            }
        } else {
            /* Scrolling left: load new columns on the left */
            for (s16 i = 0; i > col_delta && i > -(s16)g->num_cols; i--) {
                /* Move leftmost back, then load new column */
                if (g->scroll_leftmost == 0) {
                    g->scroll_leftmost = g->num_cols;
                }
                g->scroll_leftmost--;

                u8 sprite_offset = g->scroll_leftmost;
                u16 spr = g->hw_sprite_first + sprite_offset;
                s16 new_col = cur_tile_col - i;

                load_tilemap8_column(g, spr, new_col);
            }
        }
        g->scroll_last_px = cur_tile_col;
    }

    /* SCB3: Y position adjusted for Y cycling buffer and sub-tile scrolling.
     * The cycling buffer rotates tile slots, so we adjust Y position to compensate:
     * - scroll_topmost slots are "above" the visible area
     * - Sub-tile offset provides smooth pixel-level scrolling */
    s16 tile_height = tile_width; /* Tiles are square */
    s16 sub_tile_y = (s16)(((g->src_offset_y & (TILE_SIZE - 1)) * g->scale) >> 8);
    s16 adjusted_screen_y = (s16)(g->screen_y - (s16)g->scroll_topmost * tile_height - sub_tile_y);

    u8 shrink = scale_to_shrink(g->scale);
    u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);
    u16 scb3_val = NGSpriteSCB3(adjusted_screen_y, hw_height);

    if (scb3_val != g->scroll_last_scb3) {
        NGSpriteYSetUniform(g->hw_sprite_first, g->num_cols, adjusted_screen_y, hw_height);
        g->scroll_last_scb3 = scb3_val;
    }

    /* SCB4: X positions - account for cycling offset */
    /* Write positions so sprite at scroll_leftmost appears at screen_x */
    NGSpriteXBegin(g->hw_sprite_first);
    for (u8 spr_idx = 0; spr_idx < g->num_cols; spr_idx++) {
        /* screen_col: which visual column (0=leftmost) this sprite index represents */
        u8 screen_col = (u8)((spr_idx + g->num_cols - g->scroll_leftmost) % g->num_cols);
        s16 x = (s16)(g->screen_x + screen_col * tile_width);
        NGSpriteXWriteNext(x);
    }

    g->cache.last_src_offset_x = g->src_offset_x;
    g->dirty = 0;
}

/**
 * Flush a single graphic to hardware.
 */
static void flush_graphic(NGGraphic *g) {
    if (!g->hw_allocated || !g->visible) {
        return;
    }

    /* Infinite scroll mode has its own optimized path */
    if (g->tile_mode == NG_GRAPHIC_TILE_INFINITE) {
        flush_infinite_scroll(g);
        return;
    }

    /* Tilemap8 with CLIP mode uses cycling buffer for efficient scrolling */
    if (g->tilemap8 && g->tile_mode == NG_GRAPHIC_TILE_CLIP) {
        flush_tilemap_scroll(g);
        return;
    }

    u8 first_draw = (g->hw_sprite_first != g->cache.last_hw_sprite);

    /* Fast path: first draw updates everything, skip change detection */
    if (first_draw) {
        /* SCB1: Write tile data */
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

        /* SCB2: Shrink values */
        NGSpriteShrinkSet(g->hw_sprite_first, g->num_cols, scale_to_shrink_val(g->scale));
        g->cache.last_scale = g->scale;

        /* SCB3: Y Position */
        u8 shrink = scale_to_shrink(g->scale);
        u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);
        u8 use_chain = (g->layer == NG_GRAPHIC_LAYER_ENTITY);
        if (use_chain) {
            NGSpriteYSetChain(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        } else {
            NGSpriteYSetUniform(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        }
        g->cache.last_screen_y = g->screen_y;

        /* SCB4: X Position
         * For chained sprites, hardware auto-positions columns to the right
         * of the driving sprite using their horizontal shrink values.
         * Only need to write X for the first (driving) sprite. */
        if (use_chain) {
            NGSpriteXSet(g->hw_sprite_first, g->screen_x);
        } else {
            s16 tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
            if (tile_width < 1)
                tile_width = 1;
            NGSpriteXSetSpaced(g->hw_sprite_first, g->num_cols, g->screen_x, tile_width);
        }
        g->cache.last_screen_x = g->screen_x;

        g->cache.last_display_width = g->display_width;
        g->cache.last_display_height = g->display_height;
        g->cache.last_hw_sprite = g->hw_sprite_first;
        g->dirty = 0;
        return;
    }

    /* Incremental update: check what changed */
    /* Compare tile offsets, not pixel offsets, to avoid rewriting tiles for sub-tile movement */
    s16 cur_tile_off_x = g->src_offset_x >> TILE_SHIFT;
    s16 cur_tile_off_y = g->src_offset_y >> TILE_SHIFT;
    s16 last_tile_off_x = g->cache.last_src_offset_x >> TILE_SHIFT;
    s16 last_tile_off_y = g->cache.last_src_offset_y >> TILE_SHIFT;

    u8 source_changed = (g->dirty & DIRTY_SOURCE) || g->base_tile != g->cache.last_base_tile ||
                        g->anim_frame != g->cache.last_anim_frame ||
                        g->palette != g->cache.last_palette || (u8)g->flip != g->cache.last_flip ||
                        cur_tile_off_x != last_tile_off_x || cur_tile_off_y != last_tile_off_y;

    u8 size_changed = (g->dirty & DIRTY_SIZE) || g->display_width != g->cache.last_display_width ||
                      g->display_height != g->cache.last_display_height;

    u8 scale_changed = (g->dirty & DIRTY_SHRINK) || g->scale != g->cache.last_scale;

    /* Track X and Y changes separately to minimize VRAM writes */
    u8 x_changed = g->screen_x != g->cache.last_screen_x;
    u8 y_changed = g->screen_y != g->cache.last_screen_y;

    /* SCB1: Write tile data */
    if (source_changed || size_changed) {
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
    if (scale_changed) {
        NGSpriteShrinkSet(g->hw_sprite_first, g->num_cols, scale_to_shrink_val(g->scale));
        g->cache.last_scale = g->scale;
    }

    /* SCB3: Y Position - only write if Y changed */
    if (y_changed || scale_changed || size_changed) {
        u8 shrink = scale_to_shrink(g->scale);
        u8 hw_height = NGSpriteAdjustedHeight(g->num_rows, shrink);

        /* Determine column mode based on layer */
        u8 use_chain = (g->layer == NG_GRAPHIC_LAYER_ENTITY);

        if (use_chain) {
            NGSpriteYSetChain(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        } else {
            NGSpriteYSetUniform(g->hw_sprite_first, g->num_cols, g->screen_y, hw_height);
        }

        g->cache.last_screen_y = g->screen_y;
    }

    /* SCB4: X Position - only write if X changed
     * For chained sprites (entity layer), hardware auto-positions columns
     * to the right of the driving sprite. Only need to write first sprite's X. */
    if (x_changed || scale_changed || size_changed) {
        u8 use_chain = (g->layer == NG_GRAPHIC_LAYER_ENTITY);

        if (use_chain) {
            NGSpriteXSet(g->hw_sprite_first, g->screen_x);
        } else {
            s16 tile_width = (s16)((TILE_SIZE * g->scale) >> 8);
            if (tile_width < 1)
                tile_width = 1;
            NGSpriteXSetSpaced(g->hw_sprite_first, g->num_cols, g->screen_x, tile_width);
        }

        g->cache.last_screen_x = g->screen_x;
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
    g->tilemap8 = NULL;
    g->tile_to_palette = NULL;
    g->palette = 0;
    g->anim_frame = 0;
    g->tiles_per_frame = 0;
    g->src_offset_x = 0;
    g->src_offset_y = 0;

    /* Precomputed values (will be set properly by NGGraphicSetSource) */
    g->src_tiles_w = pixels_to_tiles(config->width);
    g->src_tiles_h = pixels_to_tiles(config->height);
    g->effective_base = 0;

    g->tile_mode = config->tile_mode;
    g->slice_top = 16;
    g->slice_bottom = 16;
    g->slice_left = 16;
    g->slice_right = 16;

    g->hw_sprite_first = 0;
    g->hw_sprite_count = 0;
    g->hw_allocated = 0;

    /* Infinite scroll state */
    g->scroll_leftmost = 0;
    g->scroll_topmost = 0;
    g->tiles_loaded = 0;
    g->scroll_offset = 0;
    g->scroll_last_px = 0;
    g->scroll_last_row = 0;
    g->scroll_last_scb3 = 0xFFFF;

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
    g->tilemap8 = NULL;
    g->palette = palette;
    g->tiles_per_frame = asset->tiles_per_frame;

    /* Precompute tile dimensions and effective base (avoids division/multiply in inner loop) */
    g->src_tiles_w = pixels_to_tiles(asset->width_pixels);
    g->src_tiles_h = pixels_to_tiles(asset->height_pixels);
    g->effective_base = (u16)(asset->base_tile + g->anim_frame * asset->tiles_per_frame);

    g->dirty |= DIRTY_SOURCE;

    /* Load palette data to ensure fresh colors (e.g., after lighting effects) */
    if (asset->palette_data && palette == asset->palette) {
        NGPalSet(palette, asset->palette_data);
    }
}

void NGGraphicSetSourceRaw(NGGraphic *g, u16 base_tile, u16 src_width, u16 src_height, u8 palette) {
    if (!g)
        return;

    g->base_tile = base_tile;
    g->src_width = src_width;
    g->src_height = src_height;
    g->tilemap = NULL;
    g->tilemap8 = NULL;
    g->palette = palette;

    /* Precompute tile dimensions and effective base */
    g->src_tiles_w = pixels_to_tiles(src_width);
    g->src_tiles_h = pixels_to_tiles(src_height);
    g->tiles_per_frame = (u16)(g->src_tiles_w * g->src_tiles_h);
    g->effective_base = (u16)(base_tile + g->anim_frame * g->tiles_per_frame);

    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceTilemap(NGGraphic *g, u16 base_tile, const u16 *tilemap, u16 map_width,
                               u16 map_height, const u8 *tile_to_palette, u8 palette) {
    if (!g)
        return;

    g->base_tile = base_tile;
    g->tilemap = tilemap;
    g->tilemap8 = NULL;
    g->src_width = tiles_to_pixels((u8)map_width);
    g->src_height = tiles_to_pixels((u8)map_height);
    g->tile_to_palette = tile_to_palette;
    g->palette = palette;
    g->tiles_per_frame = 0;

    /* Precompute tile dimensions */
    g->src_tiles_w = (u8)map_width;
    g->src_tiles_h = (u8)map_height;
    g->effective_base = base_tile; /* No animation for tilemaps */

    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceTilemap8(NGGraphic *g, u16 base_tile, const u8 *tilemap, u16 map_width,
                                u16 map_height, const u8 *tile_to_palette, u8 palette) {
    if (!g)
        return;

    g->base_tile = base_tile;
    g->tilemap = NULL;
    g->tilemap8 = tilemap;
    g->src_width = tiles_to_pixels((u8)map_width);
    g->src_height = tiles_to_pixels((u8)map_height);
    g->tile_to_palette = tile_to_palette;
    g->palette = palette;
    g->tiles_per_frame = 0;

    /* Precompute tile dimensions */
    g->src_tiles_w = (u8)map_width;
    g->src_tiles_h = (u8)map_height;
    g->effective_base = base_tile; /* No animation for tilemaps */

    g->dirty |= DIRTY_SOURCE;
}

void NGGraphicSetSourceOffset(NGGraphic *g, s16 x, s16 y) {
    if (!g)
        return;

    if (g->src_offset_x != x || g->src_offset_y != y) {
        /* Only mark source dirty if tile offset changed (not just pixel offset) */
        /* This avoids rewriting all tiles when scrolling sub-tile amounts */
        s16 old_tile_x = g->src_offset_x >> TILE_SHIFT;
        s16 old_tile_y = g->src_offset_y >> TILE_SHIFT;
        s16 new_tile_x = x >> TILE_SHIFT;
        s16 new_tile_y = y >> TILE_SHIFT;

        g->src_offset_x = x;
        g->src_offset_y = y;

        if (old_tile_x != new_tile_x || old_tile_y != new_tile_y) {
            g->dirty |= DIRTY_SOURCE;
        }
    }
}

void NGGraphicSetFrame(NGGraphic *g, u16 frame) {
    if (!g)
        return;

    if (g->anim_frame != frame) {
        g->anim_frame = frame;
        /* Precompute effective base tile (avoids multiply in inner loop) */
        g->effective_base = (u16)(g->base_tile + frame * g->tiles_per_frame);
        g->dirty |= DIRTY_SOURCE;
    }
}

void NGGraphicInvalidateSource(NGGraphic *g) {
    if (!g)
        return;
    g->dirty |= DIRTY_SOURCE;
    g->tiles_loaded = 0; /* Force tile reload for infinite scroll */
}

/* ============================================================
 * Transform
 * ============================================================ */

void NGGraphicSetPosition(NGGraphic *g, s16 x, s16 y) {
    if (!g)
        return;

    /* Position changes detected via cache comparison in flush_graphic */
    g->screen_x = x;
    g->screen_y = y;
}

void NGGraphicSetSize(NGGraphic *g, u16 width, u16 height) {
    if (!g)
        return;

    if (g->display_width != width || g->display_height != height) {
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
}

void NGGraphicSetScale(NGGraphic *g, u16 scale) {
    if (!g)
        return;

    if (g->scale != scale) {
        g->scale = scale;
        g->dirty |= DIRTY_SHRINK;
    }
}

void NGGraphicSetFlip(NGGraphic *g, NGGraphicFlip flip) {
    if (!g)
        return;

    if (g->flip != flip) {
        g->flip = flip;
        g->dirty |= DIRTY_SOURCE;
    }
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

void NGGraphicSystemInit(void) {
    for (u8 i = 0; i < NG_GRAPHIC_MAX; i++) {
        graphics[i].active = 0;
        graphics[i].hw_allocated = 0;
    }
    render_count = 0;
    render_order_dirty = 1;
    graphics_initialized = 1;
}

void NGGraphicSystemDraw(void) {
    if (!graphics_initialized) {
        return;
    }

    /* Rebuild render order if needed */
    if (render_order_dirty) {
        rebuild_render_order();
    }

    /* Two-pool allocation: UI sprites from back, others from front.
     * This prevents UI graphics from being redrawn when entities change. */
    u16 entity_idx = HW_SPRITE_FIRST;
    u16 ui_idx = UI_SPRITE_FIRST;

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

        u8 needed = g->num_cols;
        u16 sprite_idx;

        /* UI layer uses reserved pool at end, others use main pool */
        if (g->layer == NG_GRAPHIC_LAYER_UI) {
            if (ui_idx + needed > HW_SPRITE_MAX) {
                continue; /* Out of UI sprites */
            }
            sprite_idx = ui_idx;
            ui_idx += needed;
        } else {
            if (entity_idx + needed > UI_SPRITE_FIRST) {
                continue; /* Out of entity sprites */
            }
            sprite_idx = entity_idx;
            entity_idx += needed;
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

        /* Flush to hardware */
        flush_graphic(g);
    }

    /* Hide unused entity sprites (between entity_idx and UI_SPRITE_FIRST) */
    if (entity_idx < UI_SPRITE_FIRST) {
        u16 remaining = (u16)(UI_SPRITE_FIRST - entity_idx);
        while (remaining > 0) {
            u8 batch = (remaining > 255) ? 255 : (u8)remaining;
            NGSpriteHideRange(entity_idx, batch);
            entity_idx += batch;
            remaining -= batch;
        }
    }

    /* Hide unused UI sprites (between ui_idx and HW_SPRITE_MAX) */
    if (ui_idx < HW_SPRITE_MAX) {
        u16 remaining = (u16)(HW_SPRITE_MAX - ui_idx);
        while (remaining > 0) {
            u8 batch = (remaining > 255) ? 255 : (u8)remaining;
            NGSpriteHideRange(ui_idx, batch);
            ui_idx += batch;
            remaining -= batch;
        }
    }
}

void NGGraphicSystemReset(void) {
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
