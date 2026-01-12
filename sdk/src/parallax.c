/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <parallax.h>
#include <camera.h>
#include <neogeo.h>

#define SCREEN_WIDTH             320
#define SCREEN_HEIGHT            224
#define TILE_SIZE                16
#define SCB1_BASE                0x0000
#define SCB2_BASE                0x8000
#define SCB3_BASE                0x8200
#define SCB4_BASE                0x8400
#define MAX_COLUMNS_PER_PARALLAX 42

typedef struct {
    const NGVisualAsset *asset;
    u16 width, height;  // Display dimensions (0 = asset, 0xFFFF = infinite)
    fixed parallax_x;   // Horizontal movement rate (FIX_ONE = 1:1 with camera)
    fixed parallax_y;   // Vertical movement rate
    s16 viewport_x;     // X offset from camera viewport
    s16 viewport_y;     // Y offset from camera viewport
    fixed anchor_cam_x; // Camera position when added (for parallax calc)
    fixed anchor_cam_y;
    u8 z; // Z-index for render order
    u8 palette;
    u8 visible;
    u8 in_scene;
    u8 active;

    u16 hw_sprite_first;
    u8 hw_sprite_count;
    u8 tiles_loaded;
    u8 last_zoom;
    u16 last_scb3;
    u16 leftmost;
    s16 scroll_offset;
    s16 last_scroll_px;
    s16 last_base_x;
} Parallax;

#define SCROLL_FRAC_BITS 4
#define SCROLL_FIX(x)    ((x) << SCROLL_FRAC_BITS)
#define SCROLL_INT(x)    ((x) >> SCROLL_FRAC_BITS)

static Parallax parallax_layers[NG_PARALLAX_MAX];

extern void _NGSceneMarkRenderQueueDirty(void);

void _NGParallaxSystemInit(void) {
    for (u8 i = 0; i < NG_PARALLAX_MAX; i++) {
        parallax_layers[i].active = 0;
        parallax_layers[i].in_scene = 0;
    }
}

void _NGParallaxSystemUpdate(void) {}

u8 _NGParallaxIsInScene(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return 0;
    return parallax_layers[handle].active && parallax_layers[handle].in_scene;
}

u8 _NGParallaxGetZ(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return 0;
    return parallax_layers[handle].z;
}

/**
 * Main parallax rendering function.
 * Uses optimized indexed VRAM addressing for faster writes.
 * "move.w X,d(An)" is faster than "move.w X,xxx.L" per NeoGeo dev wiki.
 */
static void draw_parallax(Parallax *plx, u16 first_sprite) {
    if (!plx->visible || !plx->asset)
        return;

    /* Declare VRAM base register once - reused for all VRAM operations */
#ifdef __CPPCHECK__
    volatile u16 *vram_base = (volatile u16 *)NG_VRAM_BASE;
#else
    register volatile u16 *vram_base __asm__("a5") = (volatile u16 *)NG_VRAM_BASE;
#endif

    const NGVisualAsset *asset = plx->asset;

    fixed cam_x = NGCameraGetX();
    fixed cam_y = NGCameraGetY();
    fixed delta_x = cam_x - plx->anchor_cam_x;
    fixed delta_y = cam_y - plx->anchor_cam_y;
    fixed parallax_offset_x = FIX_MUL(delta_x, plx->parallax_x);
    fixed parallax_offset_y = FIX_MUL(delta_y, plx->parallax_y);
    s16 base_y = plx->viewport_y - FIX_INT(parallax_offset_y);

    u16 disp_w = plx->width;
    if (disp_w == 0)
        disp_w = asset->width_pixels;

    u8 infinite_width = (plx->width == NG_PARALLAX_WIDTH_INFINITE);
    u8 asset_cols = asset->width_tiles;
    u8 asset_rows = asset->height_tiles;

    u8 num_cols;
    if (infinite_width) {
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;
        num_cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (num_cols > MAX_COLUMNS_PER_PARALLAX)
            num_cols = MAX_COLUMNS_PER_PARALLAX;
    } else {
        num_cols = (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
        if (num_cols > MAX_COLUMNS_PER_PARALLAX)
            num_cols = MAX_COLUMNS_PER_PARALLAX;
    }

    u16 disp_h = plx->height;
    if (disp_h == 0)
        disp_h = asset->height_pixels;

    u8 num_rows;
    if (!infinite_width && disp_h > asset->height_pixels) {
        num_rows = (u8)((disp_h + TILE_SIZE - 1) / TILE_SIZE);
    } else {
        num_rows = asset_rows;
    }
    if (num_rows > 32)
        num_rows = 32;

    u8 zoom = NGCameraGetZoom();
    u8 zoom_changed = (zoom != plx->last_zoom);

    /* Detect sprite reallocation (render queue changed sprite indices) */
    if (plx->tiles_loaded && plx->hw_sprite_first != first_sprite) {
        plx->tiles_loaded = 0;
    }

    if (!plx->tiles_loaded) {
        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;

            vram_base[0] = SCB1_BASE + (spr * 64); /* VRAMADDR */
            vram_base[2] = 1;                      /* VRAMMOD */

            u8 asset_col = col % asset_cols;

            for (u8 row = 0; row < num_rows; row++) {
                u8 asset_row = row % asset_rows;
                u16 tile_idx = (u16)(asset->base_tile + (asset_col * asset_rows) + asset_row);
                vram_base[1] = tile_idx & 0xFFFF; /* VRAMDATA */
                u16 attr = ((u16)plx->palette << 8) | 0x01;
                vram_base[1] = attr; /* VRAMDATA */
            }

            for (u8 row = num_rows; row < 32; row++) {
                vram_base[1] = 0;
                vram_base[1] = 0;
            }
        }

        u16 shrink = NGCameraGetShrink();
        vram_base[0] = SCB2_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = shrink;
        }

        /* Start one tile left of screen for bidirectional scroll buffer */
        s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
        vram_base[0] = (u16)(SCB4_BASE + first_sprite);
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            s16 x = (s16)((col * tile_w) - tile_w);
            vram_base[1] = (u16)((x & 0x1FF) << 7);
        }

        plx->hw_sprite_first = first_sprite;
        plx->hw_sprite_count = num_cols;
        plx->tiles_loaded = 1;
        plx->last_zoom = zoom;
        plx->last_scb3 = 0xFFFF;

        plx->leftmost = first_sprite;
        s16 tile_width_zoomed = (s16)((TILE_SIZE * zoom) >> 4);
        /* Start scroll_offset mid-range for immediate bidirectional scrolling */
        plx->scroll_offset = SCROLL_FIX(tile_width_zoomed);
        plx->last_scroll_px = FIX_INT(parallax_offset_x);
    }

    if (zoom_changed) {
        u16 shrink = NGCameraGetShrink();
        vram_base[0] = SCB2_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = shrink;
        }

        if (infinite_width) {
            s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
            vram_base[0] = (u16)(SCB4_BASE + first_sprite);
            vram_base[2] = 1;
            for (u8 col = 0; col < num_cols; col++) {
                s16 x = (s16)((col * tile_w) - tile_w);
                vram_base[1] = (u16)((x & 0x1FF) << 7);
            }
            plx->leftmost = first_sprite;
            plx->scroll_offset = SCROLL_FIX((TILE_SIZE * zoom) >> 4);
            plx->last_scroll_px = FIX_INT(parallax_offset_x);
        }

        plx->last_zoom = zoom;
    }

    /* Adjust height_bits for zoom: at reduced zoom, shrunk graphics are shorter */
    u16 shrink = NGCameraGetShrink();
    u8 v_shrink = (u8)(shrink & 0xFF);
    u16 adjusted_rows = (u16)(((u16)num_rows * v_shrink + 254) / 255);
    if (adjusted_rows < 1)
        adjusted_rows = 1;
    if (adjusted_rows > 32)
        adjusted_rows = 32;
    u8 height_bits = (u8)adjusted_rows;

    /* NeoGeo Y: 496 at top of screen, decreasing goes down */
    s16 y_val = 496 - base_y;
    if (y_val < 0)
        y_val += 512;
    y_val &= 0x1FF;

    u16 scb3_val = ((u16)y_val << 7) | height_bits;

    if (scb3_val != plx->last_scb3) {
        vram_base[0] = SCB3_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = scb3_val;
        }
        plx->last_scb3 = scb3_val;
    }

    if (infinite_width) {
        s16 scroll_px = FIX_INT(parallax_offset_x);
        s16 pixel_diff = (s16)(scroll_px - plx->last_scroll_px);
        plx->last_scroll_px = scroll_px;

        if (pixel_diff != 0) {
            s16 tile_width_zoomed = (s16)((TILE_SIZE * zoom) >> 4);
            s16 total_width = num_cols * tile_width_zoomed;
            s16 tile_width_fixed = SCROLL_FIX(tile_width_zoomed);

            plx->scroll_offset = (s16)(plx->scroll_offset - (pixel_diff << SCROLL_FRAC_BITS));

            /* Scrolling RIGHT: wrap leftmost sprite to right */
            while (plx->scroll_offset <= 0) {
                vram_base[0] = (u16)(SCB4_BASE + plx->leftmost);
                s16 x = (s16)(vram_base[1] >> 7);
                x = (s16)(x + total_width);
                vram_base[0] = (u16)(SCB4_BASE + plx->leftmost);
                vram_base[1] = (u16)((x & 0x1FF) << 7);
                plx->leftmost++;
                if (plx->leftmost >= first_sprite + num_cols) {
                    plx->leftmost = first_sprite;
                }
                plx->scroll_offset += tile_width_fixed;
            }

            /* Scrolling LEFT: wrap rightmost sprite to left */
            while (plx->scroll_offset > tile_width_fixed * 2) {
                if (plx->leftmost <= first_sprite) {
                    plx->leftmost = first_sprite + num_cols;
                }
                plx->leftmost--;
                vram_base[0] = (u16)(SCB4_BASE + plx->leftmost);
                s16 x = (s16)(vram_base[1] >> 7);
                x = (s16)(x - total_width);
                vram_base[0] = (u16)(SCB4_BASE + plx->leftmost);
                vram_base[1] = (u16)((x & 0x1FF) << 7);
                plx->scroll_offset -= tile_width_fixed;
            }

            vram_base[2] = 1;
            for (u8 col = 0; col < num_cols; col++) {
                u16 spr = first_sprite + col;
                vram_base[0] = (u16)(SCB4_BASE + spr);
                s16 x = (s16)(vram_base[1] >> 7);
                x = (s16)(x - pixel_diff);
                vram_base[0] = (u16)(SCB4_BASE + spr);
                vram_base[1] = (u16)((x & 0x1FF) << 7);
            }
        }
    } else {
        s16 base_x = (s16)(plx->viewport_x - FIX_INT(parallax_offset_x));

        if (base_x != plx->last_base_x || zoom_changed) {
            vram_base[0] = (u16)(SCB4_BASE + first_sprite);
            vram_base[2] = 1;
            for (u8 col = 0; col < num_cols; col++) {
                s16 col_offset = (s16)((col * TILE_SIZE * zoom) >> 4);
                s16 x_pos = (s16)(base_x + col_offset);
                vram_base[1] = (u16)((x_pos & 0x1FF) << 7);
            }
            plx->last_base_x = base_x;
        }
    }
}

NGParallaxHandle NGParallaxCreate(const NGVisualAsset *asset, u16 width, u16 height,
                                  fixed parallax_x, fixed parallax_y) {
    if (!asset)
        return NG_PARALLAX_INVALID;

    NGParallaxHandle handle = NG_PARALLAX_INVALID;
    for (u8 i = 0; i < NG_PARALLAX_MAX; i++) {
        if (!parallax_layers[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_PARALLAX_INVALID)
        return NG_PARALLAX_INVALID;

    Parallax *plx = &parallax_layers[handle];
    plx->asset = asset;
    plx->width = width;
    plx->height = height;
    plx->parallax_x = parallax_x;
    plx->parallax_y = parallax_y;
    plx->viewport_x = 0;
    plx->viewport_y = 0;
    plx->anchor_cam_x = 0;
    plx->anchor_cam_y = 0;
    plx->z = 0;
    plx->palette = asset->palette;
    plx->visible = 1;
    plx->in_scene = 0;
    plx->active = 1;
    plx->hw_sprite_first = 0;
    plx->hw_sprite_count = 0;
    plx->tiles_loaded = 0;
    plx->last_zoom = 0;
    plx->last_scb3 = 0xFFFF;
    plx->leftmost = 0;
    plx->scroll_offset = 0;
    plx->last_scroll_px = 0;
    plx->last_base_x = 0x7FFF;

    return handle;
}

void NGParallaxAddToScene(NGParallaxHandle handle, s16 viewport_x, s16 viewport_y, u8 z) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;

    plx->viewport_x = viewport_x;
    plx->viewport_y = viewport_y;
    plx->z = z;

    plx->anchor_cam_x = NGCameraGetX();
    plx->anchor_cam_y = NGCameraGetY();

    plx->in_scene = 1;
    plx->tiles_loaded = 0;

    _NGSceneMarkRenderQueueDirty();
}

void NGParallaxRemoveFromScene(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;

    u8 was_in_scene = plx->in_scene;
    plx->in_scene = 0;

    /* Clear sprite heights using optimized indexed VRAM addressing */
    if (plx->hw_sprite_count > 0) {
        NG_VRAM_DECLARE_BASE();
        NG_VRAM_SETUP_FAST(SCB3_BASE + plx->hw_sprite_first, 1);
        NG_VRAM_CLEAR_FAST(plx->hw_sprite_count);
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGParallaxDestroy(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    NGParallaxRemoveFromScene(handle);
    parallax_layers[handle].active = 0;
}

void NGParallaxSetViewportPos(NGParallaxHandle handle, s16 viewport_x, s16 viewport_y) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;

    plx->viewport_x = viewport_x;
    plx->viewport_y = viewport_y;

    plx->anchor_cam_x = NGCameraGetX();
    plx->anchor_cam_y = NGCameraGetY();
}

void NGParallaxSetZ(NGParallaxHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;
    if (plx->z != z) {
        plx->z = z;
        if (plx->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

void NGParallaxSetVisible(NGParallaxHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;
    plx->visible = visible ? 1 : 0;
}

void NGParallaxSetPalette(NGParallaxHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active)
        return;
    plx->palette = palette;
    plx->tiles_loaded = 0;
}

void NGParallaxDraw(NGParallaxHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active || !plx->in_scene)
        return;

    draw_parallax(plx, first_sprite);
}

u8 NGParallaxGetSpriteCount(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX)
        return 0;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active || !plx->asset)
        return 0;

    u16 disp_w = plx->width;
    if (disp_w == 0)
        disp_w = plx->asset->width_pixels;

    if (plx->width == NG_PARALLAX_WIDTH_INFINITE) {
        // Match the allocation in draw_parallax
        u8 asset_cols = plx->asset->width_tiles;
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;
        u8 cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (cols > MAX_COLUMNS_PER_PARALLAX)
            cols = MAX_COLUMNS_PER_PARALLAX;
        return cols;
    }

    u8 cols = (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
    if (cols > MAX_COLUMNS_PER_PARALLAX)
        cols = MAX_COLUMNS_PER_PARALLAX;
    return cols;
}

/* Internal: collect palettes from all parallax layers in scene into bitmask */
void _NGParallaxCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < NG_PARALLAX_MAX; i++) {
        Parallax *plx = &parallax_layers[i];
        if (plx->active && plx->in_scene && plx->visible) {
            u8 pal = plx->palette;
            palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
        }
    }
}
