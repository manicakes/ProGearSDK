/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <terrain.h>
#include <camera.h>
#include <sprite.h>

typedef struct {
    const NGTerrainAsset *asset;
    fixed world_x, world_y;
    u8 z;
    u8 visible;
    u8 in_scene;
    u8 active;

    s16 viewport_col;
    s16 viewport_row;
    u8 viewport_cols;
    u8 viewport_rows;

    u16 hw_sprite_first;
    u8 hw_sprite_count;
    u8 tiles_loaded;

    u8 last_zoom;
    s16 last_viewport_col;
    s16 last_viewport_row;
    u16 last_scb3;

    // Cycling offset for efficient horizontal scroll (sprite reuse)
    u8 leftmost_sprite_offset;
} Terrain;

static Terrain terrains[NG_TERRAIN_MAX];

extern void _NGSceneMarkRenderQueueDirty(void);

void _NGTerrainSystemInit(void) {
    for (u8 i = 0; i < NG_TERRAIN_MAX; i++) {
        terrains[i].active = 0;
        terrains[i].in_scene = 0;
    }
}

u8 _NGTerrainIsInScene(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    return terrains[handle].active && terrains[handle].in_scene;
}

u8 _NGTerrainGetZ(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    return terrains[handle].z;
}

/**
 * Load tile data for a single column into VRAM using the sprite API.
 */
static void load_column_tiles(Terrain *tm, u16 sprite_idx, s16 terrain_col, u8 num_rows) {
    const NGTerrainAsset *asset = tm->asset;

    NGSpriteTileBegin(sprite_idx);

    for (u8 row = 0; row < num_rows; row++) {
        s16 terrain_row = tm->viewport_row + row;

        if (terrain_col < 0 || terrain_col >= asset->width_tiles || terrain_row < 0 ||
            terrain_row >= asset->height_tiles) {
            /* Out of bounds - empty tile */
            NGSpriteTileWriteEmpty();
            continue;
        }

        u16 tile_array_idx = (u16)(terrain_row * asset->width_tiles + terrain_col);
        u8 tile_idx = asset->tile_data[tile_array_idx];
        u16 crom_tile = asset->base_tile + tile_idx;

        u8 palette = asset->default_palette;
        if (asset->tile_to_palette) {
            palette = asset->tile_to_palette[tile_idx];
        }

        NGSpriteTileWrite(crom_tile, palette, 1, 0);
    }

    NGSpriteTilePadTo32(num_rows);
}

/**
 * Main terrain rendering function using the sprite abstraction API.
 */
static void draw_terrain(Terrain *tm, u16 first_sprite) {
    if (!tm->visible || !tm->asset)
        return;

    fixed cam_x = NGCameraGetRenderX();
    fixed cam_y = NGCameraGetRenderY();
    u8 zoom = NGCameraGetZoom();

    u16 view_width = (SCREEN_WIDTH * 16) / zoom;
    u16 view_height = (SCREEN_HEIGHT * 16) / zoom;

    s16 view_left = FIX_INT(cam_x - tm->world_x);
    s16 view_top = FIX_INT(cam_y - tm->world_y);

    s16 first_col = view_left / NG_TILE_SIZE;
    s16 first_row = view_top / NG_TILE_SIZE;

    u8 num_cols = (u8)((view_width / NG_TILE_SIZE) + 2);
    u8 num_rows = (u8)((view_height / NG_TILE_SIZE) + 2);

    if (num_cols > NG_TERRAIN_MAX_COLS)
        num_cols = NG_TERRAIN_MAX_COLS;
    if (num_rows > NG_TERRAIN_MAX_ROWS)
        num_rows = NG_TERRAIN_MAX_ROWS;

    u8 zoom_changed = (zoom != tm->last_zoom);

    if (tm->tiles_loaded && tm->hw_sprite_first != first_sprite) {
        tm->tiles_loaded = 0;
    }

    if (!tm->tiles_loaded) {
        tm->viewport_col = first_col;
        tm->viewport_row = first_row;
        tm->viewport_cols = num_cols;
        tm->viewport_rows = num_rows;
        tm->leftmost_sprite_offset = 0;

        /* SCB1: Load tile data for all columns */
        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;
            s16 terrain_col = first_col + col;
            load_column_tiles(tm, spr, terrain_col, num_rows);
        }

        /* SCB2: Shrink values */
        u16 shrink = NGCameraGetShrink();
        NGSpriteShrinkSet(first_sprite, num_cols, shrink);

        /* SCB4: X positions */
        s16 tile_w = (s16)((NG_TILE_SIZE * zoom) >> 4);
        s16 base_screen_x = (s16)(FIX_INT(tm->world_x - cam_x) + (first_col * NG_TILE_SIZE));
        base_screen_x = (s16)((base_screen_x * zoom) >> 4);
        NGSpriteXSetSpaced(first_sprite, num_cols, base_screen_x, tile_w);

        tm->hw_sprite_first = first_sprite;
        tm->hw_sprite_count = num_cols;
        tm->tiles_loaded = 1;
        tm->last_zoom = zoom;
        tm->last_scb3 = 0xFFFF; /* Force SCB3 write */
        tm->last_viewport_col = first_col;
        tm->last_viewport_row = first_row;
    }

    if (zoom_changed) {
        u16 shrink = NGCameraGetShrink();
        NGSpriteShrinkSet(first_sprite, num_cols, shrink);
        tm->last_zoom = zoom;
    }

    s16 col_delta = first_col - tm->last_viewport_col;
    if (col_delta != 0) {
        if (col_delta > 0) {
            for (s16 i = 0; i < col_delta && i < num_cols; i++) {
                u8 sprite_offset = tm->leftmost_sprite_offset;
                u16 spr = first_sprite + sprite_offset;
                s16 new_col = (s16)(tm->viewport_col + num_cols + i);

                load_column_tiles(tm, spr, new_col, num_rows);

                tm->leftmost_sprite_offset = (u8)((tm->leftmost_sprite_offset + 1) % num_cols);
            }
        } else {
            for (s16 i = 0; i > col_delta && i > -num_cols; i--) {
                if (tm->leftmost_sprite_offset == 0) {
                    tm->leftmost_sprite_offset = num_cols;
                }
                tm->leftmost_sprite_offset--;

                u8 sprite_offset = tm->leftmost_sprite_offset;
                u16 spr = first_sprite + sprite_offset;
                s16 new_col = (s16)(first_col - i);
                load_column_tiles(tm, spr, new_col, num_rows);
            }
        }
        tm->viewport_col = first_col;
        tm->last_viewport_col = first_col;
    }

    s16 row_delta = first_row - tm->last_viewport_row;
    if (row_delta != 0) {
        tm->viewport_row = first_row;
        for (u8 col = 0; col < num_cols; col++) {
            u8 sprite_offset = (u8)((tm->leftmost_sprite_offset + col) % num_cols);
            u16 spr = first_sprite + sprite_offset;
            s16 terrain_col = first_col + col;
            load_column_tiles(tm, spr, terrain_col, num_rows);
        }
        tm->last_viewport_row = first_row;
    }

    /* SCB3: Y position and height */
    u16 shrink = NGCameraGetShrink();
    u8 height_bits = NGSpriteAdjustedHeight(num_rows, (u8)(shrink & 0xFF));

    s16 base_screen_y = (s16)(FIX_INT(tm->world_y - cam_y) + (first_row * NG_TILE_SIZE));
    base_screen_y = (s16)((base_screen_y * zoom) >> 4);

    u16 scb3_val = NGSpriteSCB3(base_screen_y, height_bits);

    if (scb3_val != tm->last_scb3) {
        NGSpriteYSetUniform(first_sprite, num_cols, base_screen_y, height_bits);
        tm->last_scb3 = scb3_val;
    }

    /* SCB4: X positions - calculated with circular buffer offset */
    s16 tile_w = (s16)((NG_TILE_SIZE * zoom) >> 4);
    s16 base_screen_x = (s16)(FIX_INT(tm->world_x - cam_x) + (first_col * NG_TILE_SIZE));
    base_screen_x = (s16)((base_screen_x * zoom) >> 4);

    NGSpriteXBegin(first_sprite);
    for (u8 spr_idx = 0; spr_idx < num_cols; spr_idx++) {
        u8 screen_col = (u8)((spr_idx + num_cols - tm->leftmost_sprite_offset) % num_cols);
        s16 x = (s16)(base_screen_x + (screen_col * tile_w));
        NGSpriteXWriteNext(x);
    }
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
    tm->asset = asset;
    tm->world_x = 0;
    tm->world_y = 0;
    tm->z = 0;
    tm->visible = 1;
    tm->in_scene = 0;
    tm->active = 1;
    tm->viewport_col = 0;
    tm->viewport_row = 0;
    tm->viewport_cols = 0;
    tm->viewport_rows = 0;
    tm->hw_sprite_first = 0;
    tm->hw_sprite_count = 0;
    tm->tiles_loaded = 0;
    tm->last_zoom = 0;
    tm->last_viewport_col = 0;
    tm->last_viewport_row = 0;
    tm->last_scb3 = 0xFFFF;
    tm->leftmost_sprite_offset = 0;

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
    tm->tiles_loaded = 0;

    _NGSceneMarkRenderQueueDirty();
}

void NGTerrainRemoveFromScene(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active)
        return;

    u8 was_in_scene = tm->in_scene;
    tm->in_scene = 0;

    /* Clear sprite heights to hide sprites */
    if (tm->hw_sprite_count > 0) {
        NGSpriteHideRange(tm->hw_sprite_first, tm->hw_sprite_count);
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGTerrainDestroy(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    NGTerrainRemoveFromScene(handle);
    terrains[handle].active = 0;
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
        if (tm->in_scene) {
            _NGSceneMarkRenderQueueDirty();
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

void NGTerrainDraw(NGTerrainHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->in_scene)
        return;

    draw_terrain(tm, first_sprite);
}

u8 NGTerrainGetSpriteCount(NGTerrainHandle handle) {
    if (handle < 0 || handle >= NG_TERRAIN_MAX)
        return 0;
    Terrain *tm = &terrains[handle];
    if (!tm->active || !tm->asset)
        return 0;

    u8 zoom = NGCameraGetZoom();
    u16 view_width = (SCREEN_WIDTH * 16) / zoom;
    u8 num_cols = (u8)((view_width / NG_TILE_SIZE) + 2);
    if (num_cols > NG_TERRAIN_MAX_COLS)
        num_cols = NG_TERRAIN_MAX_COLS;

    return num_cols;
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
