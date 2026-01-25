/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <backdrop.h>
#include <camera.h>
#include <graphic.h>

#define TILE_SIZE                16
#define MAX_COLUMNS_PER_BACKDROP 48

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

    NGGraphic *graphic;
} Backdrop;

static Backdrop backdrop_layers[NG_BACKDROP_MAX];

void _NGBackdropSystemInit(void) {
    for (u8 i = 0; i < NG_BACKDROP_MAX; i++) {
        backdrop_layers[i].active = 0;
        backdrop_layers[i].in_scene = 0;
        backdrop_layers[i].graphic = NULL;
    }
}

void _NGBackdropSystemUpdate(void) {}

/**
 * Sync backdrop graphic with camera/parallax state.
 */
static void sync_backdrop_graphic(Backdrop *bd) {
    if (!bd->graphic || !bd->asset || !bd->visible)
        return;

    fixed cam_x = NGCameraGetX();
    fixed cam_y = NGCameraGetY();
    fixed delta_x = cam_x - bd->anchor_cam_x;
    fixed delta_y = cam_y - bd->anchor_cam_y;
    fixed parallax_offset_x = FIX_MUL(delta_x, bd->parallax_x);
    fixed parallax_offset_y = FIX_MUL(delta_y, bd->parallax_y);

    s16 screen_x, screen_y;
    u8 infinite_width = (bd->width == NG_BACKDROP_WIDTH_INFINITE);

    if (infinite_width) {
        /* For infinite scroll, pass raw pixel offset - graphic handles circular buffer */
        s16 pixel_offset_x = FIX_INT(parallax_offset_x);
        screen_x = 0; /* X positioning handled internally by graphic */
        screen_y = bd->viewport_y - FIX_INT(parallax_offset_y);
        NGGraphicSetSourceOffset(bd->graphic, pixel_offset_x, 0);
    } else {
        /* For fixed-width backdrops, position directly */
        screen_x = (s16)(bd->viewport_x - FIX_INT(parallax_offset_x));
        screen_y = bd->viewport_y - FIX_INT(parallax_offset_y);
        NGGraphicSetSourceOffset(bd->graphic, 0, 0);
    }

    NGGraphicSetPosition(bd->graphic, screen_x, screen_y);

    /* Apply camera zoom to backdrop scale */
    /* Camera zoom: 16 = 100%, 8 = 50%. Graphic scale: 256 = 100%, 128 = 50% */
    u8 zoom = NGCameraGetZoom();
    u16 scale = (u16)(zoom * 16); /* Convert: zoom * 256 / 16 = zoom * 16 */
    NGGraphicSetScale(bd->graphic, scale);
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

    /* Determine display dimensions */
    u16 disp_w = width;
    u16 disp_h = height;
    u8 infinite_width = (width == NG_BACKDROP_WIDTH_INFINITE);

    if (infinite_width) {
        /* For infinite scroll, need enough columns to show complete asset repetitions
         * at max zoom out (50%). Calculate columns needed to fill screen, then round
         * up to multiple of asset width for seamless tiling. */
        u16 screen_cols_at_50pct = (u16)((SCREEN_WIDTH * 2 + TILE_SIZE - 1) / TILE_SIZE + 2);
        u16 asset_cols = (u16)((asset->width_pixels + TILE_SIZE - 1) / TILE_SIZE);
        u16 repetitions = (u16)((screen_cols_at_50pct + asset_cols - 1) / asset_cols);
        u16 total_cols = repetitions * asset_cols;
        if (total_cols > MAX_COLUMNS_PER_BACKDROP)
            total_cols = MAX_COLUMNS_PER_BACKDROP;
        disp_w = total_cols * TILE_SIZE;
    } else if (disp_w == 0) {
        disp_w = asset->width_pixels;
    }

    if (disp_h == 0) {
        disp_h = asset->height_pixels;
    }

    /* Create graphic - use INFINITE mode for scrolling backdrops */
    NGGraphicTileMode tile_mode =
        infinite_width ? NG_GRAPHIC_TILE_INFINITE : NG_GRAPHIC_TILE_REPEAT;
    NGGraphicConfig cfg = {.width = disp_w,
                           .height = disp_h,
                           .tile_mode = tile_mode,
                           .layer = NG_GRAPHIC_LAYER_BACKGROUND,
                           .z_order = 0};
    bd->graphic = NGGraphicCreate(&cfg);
    if (!bd->graphic) {
        return NG_BACKDROP_INVALID;
    }

    /* Configure graphic source */
    NGGraphicSetSource(bd->graphic, asset, asset->palette);

    /* Initially hidden */
    NGGraphicSetVisible(bd->graphic, 0);

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

    /* Update graphic z-order and make visible */
    if (bd->graphic) {
        NGGraphicSetZOrder(bd->graphic, z);
        NGGraphicSetVisible(bd->graphic, bd->visible);
        sync_backdrop_graphic(bd);
    }
}

void NGBackdropRemoveFromScene(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;

    bd->in_scene = 0;

    /* Hide graphic */
    if (bd->graphic) {
        NGGraphicSetVisible(bd->graphic, 0);
    }
}

void NGBackdropDestroy(NGBackdropHandle handle) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;

    Backdrop *bd = &backdrop_layers[handle];

    /* Destroy graphic */
    if (bd->graphic) {
        NGGraphicDestroy(bd->graphic);
        bd->graphic = NULL;
    }

    NGBackdropRemoveFromScene(handle);
    bd->active = 0;
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
        if (bd->graphic) {
            NGGraphicSetZOrder(bd->graphic, z);
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

    /* Update graphic visibility if in scene */
    if (bd->in_scene && bd->graphic) {
        NGGraphicSetVisible(bd->graphic, bd->visible);
    }
}

void NGBackdropSetPalette(NGBackdropHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_BACKDROP_MAX)
        return;
    Backdrop *bd = &backdrop_layers[handle];
    if (!bd->active)
        return;
    if (bd->palette != palette) {
        bd->palette = palette;
        /* Update graphic source with new palette */
        if (bd->graphic && bd->asset) {
            NGGraphicSetSource(bd->graphic, bd->asset, palette);
        }
    }
}

/**
 * Sync all in-scene backdrops to their graphics.
 * Called by scene before graphic system draw.
 */
void _NGBackdropSyncGraphics(void) {
    for (u8 i = 0; i < NG_BACKDROP_MAX; i++) {
        Backdrop *bd = &backdrop_layers[i];
        if (bd->active && bd->in_scene) {
            sync_backdrop_graphic(bd);
        }
    }
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
