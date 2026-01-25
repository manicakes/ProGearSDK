/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <terrain.h>
#include <camera.h>
#include <graphic.h>

#include "sdk_internal.h"

typedef struct {
    const NGTerrainAsset *asset;
    fixed world_x, world_y;
    u8 z;
    u8 visible;
    u8 in_scene;
    u8 active;

    NGGraphic *graphic;
} Terrain;

static Terrain terrains[NG_TERRAIN_MAX];

void _NGTerrainSystemInit(void) {
    for (u8 i = 0; i < NG_TERRAIN_MAX; i++) {
        terrains[i].active = 0;
        terrains[i].in_scene = 0;
        terrains[i].graphic = NULL;
    }
}

/**
 * Sync terrain graphic with camera state.
 * Updates position and source offset based on camera viewport.
 */
static void sync_terrain_graphic(Terrain *tm) {
    if (!tm->graphic || !tm->asset || !tm->visible)
        return;

    fixed cam_x = NGCameraGetRenderX();
    fixed cam_y = NGCameraGetRenderY();
    u8 zoom = NGCameraGetZoom();

    /* Calculate view bounds in world space */
    s16 view_left = FIX_INT(cam_x - tm->world_x);
    s16 view_top = FIX_INT(cam_y - tm->world_y);

    /* Snap to tile boundaries for source offset */
    s16 tile_offset_x = (view_left / NG_TILE_SIZE) * NG_TILE_SIZE;
    s16 tile_offset_y = (view_top / NG_TILE_SIZE) * NG_TILE_SIZE;

    /* Calculate screen position (where first visible tile appears) */
    s16 screen_x = (s16)((FIX_INT(tm->world_x - cam_x) + tile_offset_x) * zoom / 16);
    s16 screen_y = (s16)((FIX_INT(tm->world_y - cam_y) + tile_offset_y) * zoom / 16);

    NGGraphicSetPosition(tm->graphic, screen_x, screen_y);
    NGGraphicSetSourceOffset(tm->graphic, tile_offset_x, tile_offset_y);

    /* Apply camera zoom as scale */
    u16 scale = (u16)((zoom * NG_GRAPHIC_SCALE_ONE) >> 4);
    NGGraphicSetScale(tm->graphic, scale);
}

NGTerrainHandle NGTerrainCreate(const NGTerrainAsset *asset) {
    if (!asset)
        return NG_TERRAIN_INVALID;

    NGTerrainHandle handle = NG_TERRAIN_INVALID;
    for (u8 i = 0; i < NG_TERRAIN_MAX; i++) {
        if (!terrains[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_TERRAIN_INVALID)
        return NG_TERRAIN_INVALID;

    Terrain *tm = &terrains[handle];

    /* Calculate display dimensions to cover viewport with buffer */
    u16 disp_w = (u16)((NG_TERRAIN_MAX_COLS)*NG_TILE_SIZE);
    u16 disp_h = (u16)((NG_TERRAIN_MAX_ROWS)*NG_TILE_SIZE);

    /* Create graphic with clip mode (don't tile outside terrain bounds) */
    NGGraphicConfig cfg = {.width = disp_w,
                           .height = disp_h,
                           .tile_mode = NG_GRAPHIC_TILE_CLIP,
                           .layer = NG_GRAPHIC_LAYER_WORLD,
                           .z_order = 0};
    tm->graphic = NGGraphicCreate(&cfg);
    if (!tm->graphic) {
        return NG_TERRAIN_INVALID;
    }

    /* Configure graphic source from terrain asset */
    NGGraphicSetSourceTilemap8(tm->graphic, asset->base_tile, asset->tile_data, asset->width_tiles,
                               asset->height_tiles, asset->tile_to_palette, asset->default_palette);

    /* Initially hidden */
    NGGraphicSetVisible(tm->graphic, 0);

    tm->asset = asset;
    tm->world_x = 0;
    tm->world_y = 0;
    tm->z = 0;
    tm->visible = 1;
    tm->in_scene = 0;
    tm->active = 1;

    return handle;
}

void NGTerrainAddToScene(NGTerrainHandle handle, fixed world_x, fixed world_y, u8 z) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;

    tm->world_x = world_x;
    tm->world_y = world_y;
    tm->z = z;
    tm->in_scene = 1;

    /* Update graphic z-order and make visible */
    if (tm->graphic) {
        NGGraphicSetZOrder(tm->graphic, z);
        NGGraphicSetVisible(tm->graphic, tm->visible);
        sync_terrain_graphic(tm);
    }
}

void NGTerrainRemoveFromScene(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;

    tm->in_scene = 0;

    /* Hide graphic */
    if (tm->graphic) {
        NGGraphicSetVisible(tm->graphic, 0);
    }
}

void NGTerrainDestroy(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;

    Terrain *tm = &terrains[handle];

    /* Destroy graphic */
    if (tm->graphic) {
        NGGraphicDestroy(tm->graphic);
        tm->graphic = NULL;
    }

    NGTerrainRemoveFromScene(handle);
    tm->active = 0;
}

void NGTerrainSetPos(NGTerrainHandle handle, fixed world_x, fixed world_y) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;

    tm->world_x = world_x;
    tm->world_y = world_y;
}

void NGTerrainSetZ(NGTerrainHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;
    if (tm->z != z) {
        tm->z = z;
        if (tm->graphic) {
            NGGraphicSetZOrder(tm->graphic, z);
        }
    }
}

void NGTerrainSetVisible(NGTerrainHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;
    tm->visible = visible ? 1 : 0;

    /* Update graphic visibility if in scene */
    if (tm->in_scene && tm->graphic) {
        NGGraphicSetVisible(tm->graphic, tm->visible);
    }
}

void NGTerrainGetDimensions(NGTerrainHandle handle, u16 *width_out, u16 *height_out) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX) {
        if (width_out)
            *width_out = 0;
        if (height_out)
            *height_out = 0;
        return;
    }
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset) {
        if (width_out)
            *width_out = 0;
        if (height_out)
            *height_out = 0;
        return;
    }
    if (width_out)
        *width_out = tm->asset->width_tiles * NG_TILE_SIZE;
    if (height_out)
        *height_out = tm->asset->height_tiles * NG_TILE_SIZE;
}

u8 NGTerrainGetCollision(NGTerrainHandle handle, fixed world_x, fixed world_y) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data)
        return 0;

    s16 tile_x = FIX_INT(world_x - tm->world_x) / NG_TILE_SIZE;
    s16 tile_y = FIX_INT(world_y - tm->world_y) / NG_TILE_SIZE;

    if (tile_x < 0 || tile_x >= tm->asset->width_tiles || tile_y < 0 ||
        tile_y >= tm->asset->height_tiles) {
        return 0;
    }

    u16 idx = (u16)(tile_y * tm->asset->width_tiles + tile_x);
    return tm->asset->collision_data[idx];
}

u8 NGTerrainGetTileAt(NGTerrainHandle handle, u16 tile_x, u16 tile_y) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset)
        return 0;

    if (tile_x >= tm->asset->width_tiles || tile_y >= tm->asset->height_tiles) {
        return 0;
    }

    u16 idx = (u16)(tile_y * tm->asset->width_tiles + tile_x);
    return tm->asset->tile_data[idx];
}

u8 NGTerrainTestAABB(NGTerrainHandle handle, fixed x, fixed y, fixed half_w, fixed half_h,
                     u8 *flags_out) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data)
        return 0;

    s16 left_tile = FIX_INT(x - half_w - tm->world_x) / NG_TILE_SIZE;
    s16 right_tile = FIX_INT(x + half_w - tm->world_x) / NG_TILE_SIZE;
    s16 top_tile = FIX_INT(y - half_h - tm->world_y) / NG_TILE_SIZE;
    s16 bottom_tile = FIX_INT(y + half_h - tm->world_y) / NG_TILE_SIZE;

    if (left_tile < 0)
        left_tile = 0;
    if (right_tile >= tm->asset->width_tiles)
        right_tile = (s16)(tm->asset->width_tiles - 1);
    if (top_tile < 0)
        top_tile = 0;
    if (bottom_tile >= tm->asset->height_tiles)
        bottom_tile = (s16)(tm->asset->height_tiles - 1);

    u8 result = 0;
    for (s16 ty = top_tile; ty <= bottom_tile; ty++) {
        for (s16 tx = left_tile; tx <= right_tile; tx++) {
            u16 idx = (u16)(ty * tm->asset->width_tiles + tx);
            u8 coll = tm->asset->collision_data[idx];
            result |= coll;
        }
    }

    if (flags_out)
        *flags_out = result;
    return (result & NG_TILE_SOLID) ? 1 : 0;
}

u8 NGTerrainResolveAABB(NGTerrainHandle handle, fixed *x, fixed *y, fixed half_w, fixed half_h,
                        fixed *vel_x, fixed *vel_y) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return NG_COLL_NONE;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data)
        return NG_COLL_NONE;

    u8 result = NG_COLL_NONE;
    fixed new_x = *x;
    fixed new_y = *y + *vel_y;

    // Vertical resolution first: allows jumping to clear ground before horizontal check
    if (*vel_y != 0) {
        s16 left_tile = FIX_INT(*x - half_w - tm->world_x) / NG_TILE_SIZE;
        s16 right_tile = FIX_INT(*x + half_w - tm->world_x) / NG_TILE_SIZE;
        s16 top_tile = FIX_INT(new_y - half_h - tm->world_y) / NG_TILE_SIZE;
        s16 bottom_tile = FIX_INT(new_y + half_h - tm->world_y) / NG_TILE_SIZE;

        if (left_tile < 0)
            left_tile = 0;
        if (right_tile >= tm->asset->width_tiles)
            right_tile = (s16)(tm->asset->width_tiles - 1);
        if (top_tile < 0)
            top_tile = 0;
        if (bottom_tile >= tm->asset->height_tiles)
            bottom_tile = (s16)(tm->asset->height_tiles - 1);

        u8 hit = 0;
        for (s16 ty = top_tile; ty <= bottom_tile && !hit; ty++) {
            for (s16 tx = left_tile; tx <= right_tile && !hit; tx++) {
                u16 idx = (u16)(ty * tm->asset->width_tiles + tx);
                u8 coll = tm->asset->collision_data[idx];

                if (coll & NG_TILE_SOLID) {
                    hit = 1;
                } else if ((coll & NG_TILE_PLATFORM) && *vel_y > 0) {
                    s16 old_bottom = FIX_INT(*y + half_h - tm->world_y) / NG_TILE_SIZE;
                    if (old_bottom < ty) {
                        hit = 1;
                    }
                }
            }
        }

        if (hit) {
            if (*vel_y > 0) {
                result |= NG_COLL_BOTTOM;
                s16 tile_top = (s16)((bottom_tile * NG_TILE_SIZE) + FIX_INT(tm->world_y));
                new_y = FIX(tile_top) - half_h - 1;
            } else {
                result |= NG_COLL_TOP;
                s16 tile_bottom = (s16)(((top_tile + 1) * NG_TILE_SIZE) + FIX_INT(tm->world_y));
                new_y = FIX(tile_bottom) + half_h + 1;
            }
            *vel_y = 0;
        }
    }

    // Horizontal resolution uses resolved Y position
    if (*vel_x != 0) {
        new_x = *x + *vel_x;

        // 2px skin avoids catching on edges
        s16 left_tile = FIX_INT(new_x - half_w - tm->world_x) / NG_TILE_SIZE;
        s16 right_tile = FIX_INT(new_x + half_w - tm->world_x) / NG_TILE_SIZE;
        s16 top_tile = FIX_INT(new_y - half_h + FIX(2) - tm->world_y) / NG_TILE_SIZE;
        s16 bottom_tile = FIX_INT(new_y + half_h - FIX(2) - tm->world_y) / NG_TILE_SIZE;

        if (left_tile < 0)
            left_tile = 0;
        if (right_tile >= tm->asset->width_tiles)
            right_tile = (s16)(tm->asset->width_tiles - 1);
        if (top_tile < 0)
            top_tile = 0;
        if (bottom_tile >= tm->asset->height_tiles)
            bottom_tile = (s16)(tm->asset->height_tiles - 1);

        u8 hit = 0;
        for (s16 ty = top_tile; ty <= bottom_tile && !hit; ty++) {
            for (s16 tx = left_tile; tx <= right_tile && !hit; tx++) {
                u16 idx = (u16)(ty * tm->asset->width_tiles + tx);
                if (tm->asset->collision_data[idx] & NG_TILE_SOLID) {
                    hit = 1;
                }
            }
        }

        if (hit) {
            if (*vel_x > 0) {
                result |= NG_COLL_RIGHT;
                s16 tile_left = (s16)((right_tile * NG_TILE_SIZE) + FIX_INT(tm->world_x));
                new_x = FIX(tile_left) - half_w - 1;
            } else {
                result |= NG_COLL_LEFT;
                s16 tile_right = (s16)(((left_tile + 1) * NG_TILE_SIZE) + FIX_INT(tm->world_x));
                new_x = FIX(tile_right) + half_w + 1;
            }
            *vel_x = 0;
        }
    }

    *x = new_x;
    *y = new_y;
    return result;
}

// TODO: Implement runtime tile modification (requires RAM copy support)
void NGTerrainSetTile(NGTerrainHandle handle, u16 tile_x, u16 tile_y, u8 tile_index) {
    (void)handle;
    (void)tile_x;
    (void)tile_y;
    (void)tile_index;
}

// TODO: Implement runtime collision modification (requires RAM copy support)
void NGTerrainSetCollision(NGTerrainHandle handle, u16 tile_x, u16 tile_y, u8 collision) {
    (void)handle;
    (void)tile_x;
    (void)tile_y;
    (void)collision;
}

/**
 * Sync all in-scene terrains to their graphics.
 * Called by scene before graphic system draw.
 */
void _NGTerrainSyncGraphics(void) {
    for (u8 i = 0; i < NG_TERRAIN_MAX; i++) {
        Terrain *tm = &terrains[i];
        if (tm->active && tm->in_scene) {
            sync_terrain_graphic(tm);
        }
    }
}

/* Internal: collect palettes from all terrains in scene into bitmask */
void _NGTerrainCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < NG_TERRAIN_MAX; i++) {
        Terrain *tm = &terrains[i];
        if (!tm->active || !tm->in_scene || !tm->asset)
            continue;

        /* Add default palette */
        u8 pal = tm->asset->default_palette;
        palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));

        /* Add all palettes from tile_to_palette lookup */
        if (tm->asset->tile_to_palette) {
            for (u16 t = 0; t < 256; t++) {
                pal = tm->asset->tile_to_palette[t];
                if (pal > 0) {
                    palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
                }
            }
        }
    }
}
