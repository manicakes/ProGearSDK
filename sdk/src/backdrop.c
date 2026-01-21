/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <backdrop.h>
#include <camera.h>
#include <hw/sprite.h>
#include <hw/lspc.h>

#define MAX_COLUMNS_PER_BACKDROP 42

/* Internal backdrop data structure */
typedef struct {
    const NGVisualAsset *asset;
    u16 width, height;  /* Display dimensions (0 = asset, 0xFFFF = infinite) */
    fixed parallax_x;   /* Horizontal movement rate (FIX_ONE = 1:1 with camera) */
    fixed parallax_y;   /* Vertical movement rate */
    s16 viewport_x;     /* X offset from camera viewport */
    s16 viewport_y;     /* Y offset from camera viewport */
    fixed anchor_cam_x; /* Camera position when added (for parallax calc) */
    fixed anchor_cam_y;
    u8 z; /* Z-index for render order */
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
} BackdropData;

#define SCROLL_FRAC_BITS 4
#define SCROLL_FIX(x)    ((x) << SCROLL_FRAC_BITS)
#define SCROLL_INT(x)    ((x) >> SCROLL_FRAC_BITS)

static BackdropData backdrop_layers[BACKDROP_MAX];

extern void _SceneMarkRenderQueueDirty(void);

void _BackdropSystemInit(void) {
    for (u8 i = 0; i < BACKDROP_MAX; i++) {
        backdrop_layers[i].active = 0;
        backdrop_layers[i].in_scene = 0;
    }
}

void _BackdropSystemUpdate(void) {}

u8 _BackdropIsInScene(Backdrop handle) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return 0;
    return backdrop_layers[handle].active && backdrop_layers[handle].in_scene;
}

u8 _BackdropGetZ(Backdrop handle) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return 0;
    return backdrop_layers[handle].z;
}

/**
 * Main backdrop rendering function.
 * Uses hw/sprite functions for VRAM access.
 */
static void draw_backdrop(BackdropData *bd, u16 first_sprite) {
    if (!bd->visible || !bd->asset)
        return;

    const NGVisualAsset *asset = bd->asset;

    fixed cam_x = CameraGetX();
    fixed cam_y = CameraGetY();
    fixed delta_x = cam_x - bd->anchor_cam_x;
    fixed delta_y = cam_y - bd->anchor_cam_y;
    fixed parallax_offset_x = FIX_MUL(delta_x, bd->parallax_x);
    fixed parallax_offset_y = FIX_MUL(delta_y, bd->parallax_y);
    s16 base_y = (s16)(bd->viewport_y - FIX_INT(parallax_offset_y));

    u16 disp_w = bd->width;
    if (disp_w == 0)
        disp_w = asset->width_pixels;

    u8 infinite_width = (bd->width == BACKDROP_WIDTH_INFINITE);
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

    u8 zoom = CameraGetZoom();
    u8 zoom_changed = (zoom != bd->last_zoom);

    /* Detect sprite reallocation (render queue changed sprite indices) */
    if (bd->tiles_loaded && bd->hw_sprite_first != first_sprite) {
        bd->tiles_loaded = 0;
    }

    if (!bd->tiles_loaded) {
        /* Write tiles for each column using batched SCB1 operations */
        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;
            hw_sprite_begin_scb1(spr);

            u8 asset_col = col % asset_cols;

            for (u8 row = 0; row < num_rows; row++) {
                u8 asset_row = row % asset_rows;
                u16 tile_idx = (u16)(asset->base_tile + (asset_col * asset_rows) + asset_row);
                u16 attr = ((u16)bd->palette << 8) | 0x01;
                hw_sprite_write_scb1_data(tile_idx, attr);
            }

            for (u8 row = num_rows; row < 32; row++) {
                hw_sprite_write_scb1_data(0, 0);
            }
        }

        /* Write shrink values */
        u16 shrink = CameraGetShrink();
        hw_sprite_write_shrink(first_sprite, num_cols, shrink);

        /* Start one tile left of screen for bidirectional scroll buffer */
        s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
        hw_sprite_write_scb4_range(first_sprite, num_cols, -tile_w, tile_w);

        bd->hw_sprite_first = first_sprite;
        bd->hw_sprite_count = num_cols;
        bd->tiles_loaded = 1;
        bd->last_zoom = zoom;
        bd->last_scb3 = 0xFFFF;

        bd->leftmost = first_sprite;
        s16 tile_width_zoomed = (s16)((TILE_SIZE * zoom) >> 4);
        bd->scroll_offset = SCROLL_FIX(tile_width_zoomed);
        bd->last_scroll_px = FIX_INT(parallax_offset_x);
    }

    if (zoom_changed) {
        u16 shrink = CameraGetShrink();
        hw_sprite_write_shrink(first_sprite, num_cols, shrink);

        if (infinite_width) {
            s16 tile_w = (s16)((TILE_SIZE * zoom) >> 4);
            hw_sprite_write_scb4_range(first_sprite, num_cols, -tile_w, tile_w);
            bd->leftmost = first_sprite;
            bd->scroll_offset = SCROLL_FIX((TILE_SIZE * zoom) >> 4);
            bd->last_scroll_px = FIX_INT(parallax_offset_x);
        }

        bd->last_zoom = zoom;
    }

    /* Adjust height for zoom */
    u16 shrink = CameraGetShrink();
    u8 height_bits = hw_sprite_adjusted_height(num_rows, (u8)(shrink & 0xFF));

    u16 scb3_val = hw_sprite_pack_scb3(base_y, height_bits);

    if (scb3_val != bd->last_scb3) {
        hw_sprite_write_ychain(first_sprite, num_cols, base_y, height_bits);
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

            /* Handle wraps */
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

            /* Calculate X positions mathematically from scroll state */
            s16 base_left_x = (s16)(SCROLL_INT(bd->scroll_offset) - 2 * tile_width_zoomed);
            u8 leftmost_offset = (u8)(bd->leftmost - first_sprite);

            hw_sprite_begin_scb4(first_sprite);
            for (u8 buf = 0; buf < num_cols; buf++) {
                u8 screen_col = (u8)((buf - leftmost_offset + num_cols) % num_cols);
                s16 x = (s16)(base_left_x + screen_col * tile_width_zoomed);
                hw_sprite_write_scb4_data(hw_sprite_pack_scb4(x));
            }
        }
    } else {
        s16 base_x = (s16)(bd->viewport_x - FIX_INT(parallax_offset_x));

        if (base_x != bd->last_base_x || zoom_changed) {
            s16 tile_step = (s16)((TILE_SIZE * zoom) >> 4);
            hw_sprite_write_scb4_range(first_sprite, num_cols, base_x, tile_step);
            bd->last_base_x = base_x;
        }
    }
}

Backdrop BackdropCreate(const VisualAsset *asset, u16 width, u16 height, fixed parallax_x,
                        fixed parallax_y) {
    if (!asset)
        return BACKDROP_INVALID;

    Backdrop handle = BACKDROP_INVALID;
    for (u8 i = 0; i < BACKDROP_MAX; i++) {
        if (!backdrop_layers[i].active) {
            handle = (Backdrop)i;
            break;
        }
    }
    if (handle == BACKDROP_INVALID)
        return BACKDROP_INVALID;

    BackdropData *bd = &backdrop_layers[handle];
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

void BackdropAddToScene(Backdrop handle, s16 viewport_x, s16 viewport_y, u8 z) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    bd->viewport_x = viewport_x;
    bd->viewport_y = viewport_y;
    bd->z = z;

    bd->anchor_cam_x = CameraGetX();
    bd->anchor_cam_y = CameraGetY();

    bd->in_scene = 1;
    bd->tiles_loaded = 0;

    _SceneMarkRenderQueueDirty();
}

void BackdropRemoveFromScene(Backdrop handle) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    u8 was_in_scene = bd->in_scene;
    bd->in_scene = 0;

    /* Hide sprites */
    if (bd->hw_sprite_count > 0) {
        hw_sprite_hide(bd->hw_sprite_first, bd->hw_sprite_count);
    }

    if (was_in_scene) {
        _SceneMarkRenderQueueDirty();
    }
}

void BackdropDestroy(Backdrop handle) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropRemoveFromScene(handle);
    backdrop_layers[handle].active = 0;
}

void BackdropSetViewportPos(Backdrop handle, s16 viewport_x, s16 viewport_y) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    bd->viewport_x = viewport_x;
    bd->viewport_y = viewport_y;

    bd->anchor_cam_x = CameraGetX();
    bd->anchor_cam_y = CameraGetY();
}

void BackdropSetZ(Backdrop handle, u8 z) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    if (bd->z != z) {
        bd->z = z;
        if (bd->in_scene) {
            _SceneMarkRenderQueueDirty();
        }
    }
}

void BackdropSetVisible(Backdrop handle, u8 visible) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    bd->visible = visible ? 1 : 0;
}

void BackdropSetPalette(Backdrop handle, u8 palette) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    bd->palette = palette;
    bd->tiles_loaded = 0;
}

void _BackdropDraw(Backdrop handle, u16 first_sprite) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active || !bd->in_scene)
        return;

    draw_backdrop(bd, first_sprite);
}

u8 _BackdropGetSpriteCount(Backdrop handle) {
    if (handle < 0 || handle >= BACKDROP_MAX)
        return 0;
    BackdropData *bd = &backdrop_layers[handle];
    if (!bd->active || !bd->asset)
        return 0;

    u16 disp_w = bd->width;
    if (disp_w == 0)
        disp_w = bd->asset->width_pixels;

    if (bd->width == BACKDROP_WIDTH_INFINITE) {
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
void _BackdropCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < BACKDROP_MAX; i++) {
        BackdropData *bd = &backdrop_layers[i];
        if (bd->active && bd->in_scene && bd->visible) {
            u8 pal = bd->palette;
            palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
        }
    }
}
