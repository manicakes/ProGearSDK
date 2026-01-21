/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <backdrop.h>
#include <camera.h>
#include <neogeo.h>
#include <sprite.h>

#define TILE_SIZE                16
#define MAX_COLUMNS_PER_BACKDROP 42

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
} Backdrop;

#define SCROLL_FRAC_BITS 4
#define SCROLL_FIX(x)    ((x) << SCROLL_FRAC_BITS)
#define SCROLL_INT(x)    ((x) >> SCROLL_FRAC_BITS)

static Backdrop backdrop_layers[NG_BACKDROP_MAX];

extern void _NGSceneMarkRenderQueueDirty(void);

void _NGBackdropSystemInit(void) {
    for (u8 i = 0; i < NG_BACKDROP_MAX; i++) {
        backdrop_layers[i].active = 0;
        backdrop_layers[i].in_scene = 0;
    }
}

void _NGBackdropSystemUpdate(void) {}

u8 _NGBackdropIsInScene(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return 0;
    return backdrop_layers[handle].active && backdrop_layers[handle].in_scene;
}

u8 _NGBackdropGetZ(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return 0;
    return backdrop_layers[handle].z;
}

/**
 * Main backdrop rendering function.
 * Uses optimized indexed VRAM addressing for faster writes.
 * "move.w X,d(An)" is faster than "move.w X,xxx.L" per NeoGeo dev wiki.
 */
static void draw_backdrop(Backdrop *bd, u16 first_sprite) {
    if (!bd->visible || !bd->asset)
        return;

    /* Declare VRAM base register once - reused for all VRAM operations */
#ifdef __CPPCHECK__
    volatile u16 *vram_base = (volatile u16 *)NG_VRAM_BASE;
#else
    register volatile u16 *vram_base __asm__("a5") = (volatile u16 *)NG_VRAM_BASE;
#endif

    const NGVisualAsset *asset = bd->asset;

    fixed cam_x = NGCameraGetX();
    fixed cam_y = NGCameraGetY();
    fixed delta_x = cam_x - bd->anchor_cam_x;
    fixed delta_y = cam_y - bd->anchor_cam_y;
    fixed parallax_offset_x = FIX_MUL(delta_x, bd->parallax_x);
    fixed parallax_offset_y = FIX_MUL(delta_y, bd->parallax_y);
    s16 base_y = bd->viewport_y - FIX_INT(parallax_offset_y);

    u16 disp_w = bd->width;
    if (disp_w == 0)
        disp_w = asset->width_pixels;

    u8 infinite_width = (bd->width == NG_BACKDROP_WIDTH_INFINITE);
    u8 asset_cols = asset->width_tiles;
    u8 asset_rows = asset->height_tiles;

    u8 num_cols;
    if (infinite_width) {
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;
        num_cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (num_cols > MAX_COLUMNS_PER_BACKDROP)
            num_cols = MAX_COLUMNS_PER_BACKDROP;
    } else {
        num_cols = (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
        if (num_cols > MAX_COLUMNS_PER_BACKDROP)
            num_cols = MAX_COLUMNS_PER_BACKDROP;
    }

    u16 disp_h = bd->height;
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
    u8 zoom_changed = (zoom != bd->last_zoom);

    /* Detect sprite reallocation (render queue changed sprite indices) */
    if (bd->tiles_loaded && bd->hw_sprite_first != first_sprite) {
        bd->tiles_loaded = 0;
    }

    if (!bd->tiles_loaded) {
        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;

            vram_base[0] = NG_SCB1_BASE + (spr * 64); /* VRAMADDR */
            vram_base[2] = 1;                         /* VRAMMOD */

            u8 asset_col = col % asset_cols;

            for (u8 row = 0; row < num_rows; row++) {
                u8 asset_row = row % asset_rows;
                u16 tile_idx = (u16)(asset->base_tile + (asset_col * asset_rows) + asset_row);
                vram_base[1] = tile_idx & 0xFFFF; /* VRAMDATA */
                u16 attr = ((u16)bd->palette << 8) | 0x01;
                vram_base[1] = attr; /* VRAMDATA */
            }

            for (u8 row = num_rows; row < 32; row++) {
                vram_base[1] = 0;
                vram_base[1] = 0;
            }
        }

        u16 shrink = NGCameraGetShrink();
        vram_base[0] = NG_SCB2_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = shrink;
        }

        /* Start one tile left of screen for bidirectional scroll buffer */
        s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
        vram_base[0] = (u16)(NG_SCB4_BASE + first_sprite);
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            s16 x = (s16)((col * tile_w) - tile_w);
            vram_base[1] = NGSpriteSCB4(x);
        }

        bd->hw_sprite_first = first_sprite;
        bd->hw_sprite_count = num_cols;
        bd->tiles_loaded = 1;
        bd->last_zoom = zoom;
        bd->last_scb3 = 0xFFFF;

        bd->leftmost = first_sprite;
        s16 tile_width_zoomed = (s16)((TILE_SIZE * zoom) >> 4);
        /* Start scroll_offset mid-range for immediate bidirectional scrolling */
        bd->scroll_offset = SCROLL_FIX(tile_width_zoomed);
        bd->last_scroll_px = FIX_INT(parallax_offset_x);
    }

    if (zoom_changed) {
        u16 shrink = NGCameraGetShrink();
        vram_base[0] = NG_SCB2_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = shrink;
        }

        if (infinite_width) {
            s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
            vram_base[0] = (u16)(NG_SCB4_BASE + first_sprite);
            vram_base[2] = 1;
            for (u8 col = 0; col < num_cols; col++) {
                s16 x = (s16)((col * tile_w) - tile_w);
                vram_base[1] = NGSpriteSCB4(x);
            }
            bd->leftmost = first_sprite;
            bd->scroll_offset = SCROLL_FIX((TILE_SIZE * zoom) >> 4);
            bd->last_scroll_px = FIX_INT(parallax_offset_x);
        }

        bd->last_zoom = zoom;
    }

    /* Adjust height for zoom: at reduced zoom, shrunk graphics are shorter */
    u16 shrink = NGCameraGetShrink();
    u8 height_bits = NGSpriteAdjustedHeight(num_rows, (u8)(shrink & 0xFF));

    u16 scb3_val = NGSpriteSCB3(base_y, height_bits);

    if (scb3_val != bd->last_scb3) {
        vram_base[0] = NG_SCB3_BASE + first_sprite;
        vram_base[2] = 1;
        for (u8 col = 0; col < num_cols; col++) {
            vram_base[1] = scb3_val;
        }
        bd->last_scb3 = scb3_val;
    }

    if (infinite_width) {
        s16 scroll_px = FIX_INT(parallax_offset_x);
        s16 pixel_diff = (s16)(scroll_px - bd->last_scroll_px);
        bd->last_scroll_px = scroll_px;

        if (pixel_diff != 0) {
            s16 tile_width_zoomed = (s16)((TILE_SIZE * zoom) >> 4);
            s16 tile_width_fixed = SCROLL_FIX(tile_width_zoomed);

            bd->scroll_offset = (s16)(bd->scroll_offset - (pixel_diff << SCROLL_FRAC_BITS));

            /* Handle wraps - just update leftmost pointer, no VRAM reads needed */
            while (bd->scroll_offset <= 0) {
                bd->leftmost++;
                if (bd->leftmost >= first_sprite + num_cols) {
                    bd->leftmost = first_sprite;
                }
                bd->scroll_offset += tile_width_fixed;
            }

            while (bd->scroll_offset > tile_width_fixed * 2) {
                if (bd->leftmost <= first_sprite) {
                    bd->leftmost = first_sprite + num_cols;
                }
                bd->leftmost--;
                bd->scroll_offset -= tile_width_fixed;
            }

            /* Calculate X positions mathematically from scroll state.
             * Eliminates VRAM reads - enables batched sequential writes. */
            s16 base_left_x = (s16)(SCROLL_INT(bd->scroll_offset) - 2 * tile_width_zoomed);
            u8 leftmost_offset = (u8)(bd->leftmost - first_sprite);

            vram_base[0] = (u16)(NG_SCB4_BASE + first_sprite);
            vram_base[2] = 1;
            for (u8 buf = 0; buf < num_cols; buf++) {
                u8 screen_col = (u8)((buf - leftmost_offset + num_cols) % num_cols);
                s16 x = (s16)(base_left_x + screen_col * tile_width_zoomed);
                vram_base[1] = NGSpriteSCB4(x);
            }
        }
    } else {
        s16 base_x = (s16)(bd->viewport_x - FIX_INT(parallax_offset_x));

        if (base_x != bd->last_base_x || zoom_changed) {
            vram_base[0] = (u16)(NG_SCB4_BASE + first_sprite);
            vram_base[2] = 1;
            for (u8 col = 0; col < num_cols; col++) {
                s16 col_offset = (s16)((col * TILE_SIZE * zoom) >> 4);
                s16 x_pos = (s16)(base_x + col_offset);
                vram_base[1] = NGSpriteSCB4(x_pos);
            }
            bd->last_base_x = base_x;
        }
    }
}

NGBackdropHandle NGBackdropCreate(const NGVisualAsset *asset, u16 width, u16 height,
                                  fixed parallax_x, fixed parallax_y) {
    if (!asset)
        return NG_BACKDROP_INVALID;

    NGBackdropHandle handle = NG_BACKDROP_INVALID;
    for (u8 i = 0; i < NG_BACKDROP_MAX; i++) {
        if (!backdrop_layers[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_BACKDROP_INVALID)
        return NG_BACKDROP_INVALID;

    Backdrop *bd = &backdrop_layers[handle];
    bd->asset = asset;
    bd->width = width;
    bd->height = height;
    bd->parallax_x = parallax_x;
    bd->parallax_y = parallax_y;
    bd->viewport_x = 0;
    bd->viewport_y = 0;
    bd->anchor_cam_x = 0;
    bd->anchor_cam_y = 0;
    bd->z = 0;
    bd->palette = asset->palette;
    bd->visible = 1;
    bd->in_scene = 0;
    bd->active = 1;
    bd->hw_sprite_first = 0;
    bd->hw_sprite_count = 0;
    bd->tiles_loaded = 0;
    bd->last_zoom = 0;
    bd->last_scb3 = 0xFFFF;
    bd->leftmost = 0;
    bd->scroll_offset = 0;
    bd->last_scroll_px = 0;
    bd->last_base_x = 0x7FFF;

    return handle;
}

void NGBackdropAddToScene(NGBackdropHandle handle, s16 viewport_x, s16 viewport_y, u8 z) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    bd->viewport_x = viewport_x;
    bd->viewport_y = viewport_y;
    bd->z = z;

    bd->anchor_cam_x = NGCameraGetX();
    bd->anchor_cam_y = NGCameraGetY();

    bd->in_scene = 1;
    bd->tiles_loaded = 0;

    _NGSceneMarkRenderQueueDirty();
}

void NGBackdropRemoveFromScene(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    u8 was_in_scene = bd->in_scene;
    bd->in_scene = 0;

    /* Clear sprite heights to hide sprites */
    if (bd->hw_sprite_count > 0) {
        NGSpriteHideRange(bd->hw_sprite_first, bd->hw_sprite_count);
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGBackdropDestroy(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    NGBackdropRemoveFromScene(handle);
    backdrop_layers[handle].active = 0;
}

void NGBackdropSetViewportPos(NGBackdropHandle handle, s16 viewport_x, s16 viewport_y) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    bd->viewport_x = viewport_x;
    bd->viewport_y = viewport_y;

    bd->anchor_cam_x = NGCameraGetX();
    bd->anchor_cam_y = NGCameraGetY();
}

void NGBackdropSetZ(NGBackdropHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    if (bd->z != z) {
        bd->z = z;
        if (bd->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

void NGBackdropSetVisible(NGBackdropHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    bd->visible = visible ? 1 : 0;
}

void NGBackdropSetPalette(NGBackdropHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    bd->palette = palette;
    bd->tiles_loaded = 0;
}

void NGBackdropDraw(NGBackdropHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active || !bd->in_scene)
        return;

    draw_backdrop(bd, first_sprite);
}

u8 NGBackdropGetSpriteCount(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return 0;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active || !bd->asset)
        return 0;

    u16 disp_w = bd->width;
    if (disp_w == 0)
        disp_w = bd->asset->width_pixels;

    if (bd->width == NG_BACKDROP_WIDTH_INFINITE) {
        // Match the allocation in draw_backdrop
        u8 asset_cols = bd->asset->width_tiles;
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;
        u8 cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (cols > MAX_COLUMNS_PER_BACKDROP)
            cols = MAX_COLUMNS_PER_BACKDROP;
        return cols;
    }

    u8 cols = (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
    if (cols > MAX_COLUMNS_PER_BACKDROP)
        cols = MAX_COLUMNS_PER_BACKDROP;
    return cols;
}

/* Internal: collect palettes from all backdrop layers in scene into bitmask */
void _NGBackdropCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < NG_BACKDROP_MAX; i++) {
        Backdrop *bd = &backdrop_layers[i];
        if (bd->active && bd->in_scene && bd->visible) {
            u8 pal = bd->palette;
            palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
        }
    }
}
