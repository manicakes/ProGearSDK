/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <tilemap.h>
#include <camera.h>
#include <neogeo.h>

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  224

#define SCB1_BASE  0x0000
#define SCB2_BASE  0x8000
#define SCB3_BASE  0x8200
#define SCB4_BASE  0x8400

typedef struct {
    const NGTilemapAsset *asset;
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
} Tilemap;

static Tilemap tilemaps[NG_TILEMAP_MAX];

extern void _NGSceneMarkRenderQueueDirty(void);

void _NGTilemapSystemInit(void) {
    for (u8 i = 0; i < NG_TILEMAP_MAX; i++) {
        tilemaps[i].active = 0;
        tilemaps[i].in_scene = 0;
    }
}

u8 _NGTilemapIsInScene(NGTilemapHandle handle) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    return tilemaps[handle].active && tilemaps[handle].in_scene;
}

u8 _NGTilemapGetZ(NGTilemapHandle handle) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    return tilemaps[handle].z;
}

static void load_column_tiles(Tilemap *tm, u16 sprite_idx, s16 tilemap_col, u8 num_rows) {
    const NGTilemapAsset *asset = tm->asset;

    NG_REG_VRAMADDR = SCB1_BASE + (sprite_idx * 64);
    NG_REG_VRAMMOD = 1;

    for (u8 row = 0; row < num_rows; row++) {
        s16 tilemap_row = tm->viewport_row + row;

        if (tilemap_col < 0 || tilemap_col >= asset->width_tiles ||
            tilemap_row < 0 || tilemap_row >= asset->height_tiles) {
            // Out of bounds - empty tile
            NG_REG_VRAMDATA = 0;
            NG_REG_VRAMDATA = 0;
            continue;
        }

        u16 tile_array_idx = tilemap_row * asset->width_tiles + tilemap_col;
        u8 tile_idx = asset->tile_data[tile_array_idx];
        u16 crom_tile = asset->base_tile + tile_idx;

        u8 palette = asset->default_palette;
        if (asset->tile_to_palette) {
            palette = asset->tile_to_palette[tile_idx];
        }

        NG_REG_VRAMDATA = crom_tile & 0xFFFF;
        u16 attr = ((u16)palette << 8) | 0x01;
        NG_REG_VRAMDATA = attr;
    }

    for (u8 row = num_rows; row < 32; row++) {
        NG_REG_VRAMDATA = 0;
        NG_REG_VRAMDATA = 0;
    }
}

static void draw_tilemap(Tilemap *tm, u16 first_sprite) {
    if (!tm->visible || !tm->asset) return;

    fixed cam_x = NGCameraGetX();
    fixed cam_y = NGCameraGetY();
    u8 zoom = NGCameraGetZoom();

    u16 view_width = (SCREEN_WIDTH * 16) / zoom;
    u16 view_height = (SCREEN_HEIGHT * 16) / zoom;

    s16 view_left = FIX_INT(cam_x - tm->world_x);
    s16 view_top = FIX_INT(cam_y - tm->world_y);

    s16 first_col = view_left / NG_TILE_SIZE;
    s16 first_row = view_top / NG_TILE_SIZE;

    u8 num_cols = (view_width / NG_TILE_SIZE) + 2;
    u8 num_rows = (view_height / NG_TILE_SIZE) + 2;

    if (num_cols > NG_TILEMAP_MAX_COLS) num_cols = NG_TILEMAP_MAX_COLS;
    if (num_rows > NG_TILEMAP_MAX_ROWS) num_rows = NG_TILEMAP_MAX_ROWS;

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

        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;
            s16 tilemap_col = first_col + col;
            load_column_tiles(tm, spr, tilemap_col, num_rows);
        }

        u16 shrink = NGCameraGetShrink();
        NG_REG_VRAMADDR = SCB2_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = shrink;
        }

        s16 tile_w = (NG_TILE_SIZE * zoom) >> 4;
        s16 base_screen_x = FIX_INT(tm->world_x - cam_x) + (first_col * NG_TILE_SIZE);
        base_screen_x = (base_screen_x * zoom) >> 4;

        NG_REG_VRAMADDR = SCB4_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            s16 x = base_screen_x + (col * tile_w);
            NG_REG_VRAMDATA = (x & 0x1FF) << 7;
        }

        tm->hw_sprite_first = first_sprite;
        tm->hw_sprite_count = num_cols;
        tm->tiles_loaded = 1;
        tm->last_zoom = zoom;
        tm->last_scb3 = 0xFFFF;  // Force SCB3 write
        tm->last_viewport_col = first_col;
        tm->last_viewport_row = first_row;
    }

    if (zoom_changed) {
        u16 shrink = NGCameraGetShrink();
        NG_REG_VRAMADDR = SCB2_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = shrink;
        }
        tm->last_zoom = zoom;
    }

    s16 col_delta = first_col - tm->last_viewport_col;
    if (col_delta != 0) {
        if (col_delta > 0) {
            for (s16 i = 0; i < col_delta && i < num_cols; i++) {
                u8 sprite_offset = tm->leftmost_sprite_offset;
                u16 spr = first_sprite + sprite_offset;
                s16 new_col = tm->viewport_col + num_cols + i;

                load_column_tiles(tm, spr, new_col, num_rows);

                tm->leftmost_sprite_offset = (tm->leftmost_sprite_offset + 1) % num_cols;
            }
        } else {
            for (s16 i = 0; i > col_delta && i > -num_cols; i--) {
                if (tm->leftmost_sprite_offset == 0) {
                    tm->leftmost_sprite_offset = num_cols;
                }
                tm->leftmost_sprite_offset--;

                u8 sprite_offset = tm->leftmost_sprite_offset;
                u16 spr = first_sprite + sprite_offset;
                s16 new_col = first_col - i;
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
            u8 sprite_offset = (tm->leftmost_sprite_offset + col) % num_cols;
            u16 spr = first_sprite + sprite_offset;
            s16 tilemap_col = first_col + col;
            load_column_tiles(tm, spr, tilemap_col, num_rows);
        }
        tm->last_viewport_row = first_row;
    }

    u16 shrink = NGCameraGetShrink();
    u8 v_shrink = shrink & 0xFF;
    u16 adjusted_rows = ((u16)num_rows * v_shrink + 254) / 255;
    if (adjusted_rows < 1) adjusted_rows = 1;
    if (adjusted_rows > 32) adjusted_rows = 32;
    u8 height_bits = (u8)adjusted_rows;

    s16 base_screen_y = FIX_INT(tm->world_y - cam_y) + (first_row * NG_TILE_SIZE);
    base_screen_y = (base_screen_y * zoom) >> 4;

    s16 y_val = 496 - base_screen_y;
    if (y_val < 0) y_val += 512;
    y_val &= 0x1FF;

    u16 scb3_val = ((u16)y_val << 7) | height_bits;

    if (scb3_val != tm->last_scb3) {
        NG_REG_VRAMADDR = SCB3_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = scb3_val;
        }
        tm->last_scb3 = scb3_val;
    }

    s16 tile_w = (NG_TILE_SIZE * zoom) >> 4;
    s16 base_screen_x = FIX_INT(tm->world_x - cam_x) + (first_col * NG_TILE_SIZE);
    base_screen_x = (base_screen_x * zoom) >> 4;

    NG_REG_VRAMADDR = SCB4_BASE + first_sprite;
    NG_REG_VRAMMOD = 1;
    for (u8 col = 0; col < num_cols; col++) {
        u8 sprite_offset = (tm->leftmost_sprite_offset + col) % num_cols;
        s16 x = base_screen_x + (col * tile_w);

        NG_REG_VRAMADDR = SCB4_BASE + first_sprite + sprite_offset;
        NG_REG_VRAMDATA = (x & 0x1FF) << 7;
    }
}

NGTilemapHandle NGTilemapCreate(const NGTilemapAsset *asset) {
    if (!asset) return NG_TILEMAP_INVALID;

    NGTilemapHandle handle = NG_TILEMAP_INVALID;
    for (u8 i = 0; i < NG_TILEMAP_MAX; i++) {
        if (!tilemaps[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_TILEMAP_INVALID) return NG_TILEMAP_INVALID;

    Tilemap *tm = &tilemaps[handle];
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

void NGTilemapAddToScene(NGTilemapHandle handle, fixed world_x, fixed world_y, u8 z) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active) return;

    tm->world_x = world_x;
    tm->world_y = world_y;
    tm->z = z;
    tm->in_scene = 1;
    tm->tiles_loaded = 0;

    _NGSceneMarkRenderQueueDirty();
}

void NGTilemapRemoveFromScene(NGTilemapHandle handle) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active) return;

    u8 was_in_scene = tm->in_scene;
    tm->in_scene = 0;

    if (tm->hw_sprite_count > 0) {
        for (u8 i = 0; i < tm->hw_sprite_count; i++) {
            NG_REG_VRAMADDR = SCB3_BASE + tm->hw_sprite_first + i;
            NG_REG_VRAMDATA = 0;
        }
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGTilemapDestroy(NGTilemapHandle handle) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    NGTilemapRemoveFromScene(handle);
    tilemaps[handle].active = 0;
}

void NGTilemapSetPos(NGTilemapHandle handle, fixed world_x, fixed world_y) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active) return;

    tm->world_x = world_x;
    tm->world_y = world_y;
}

void NGTilemapSetZ(NGTilemapHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active) return;
    if (tm->z != z) {
        tm->z = z;
        if (tm->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

void NGTilemapSetVisible(NGTilemapHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active) return;
    tm->visible = visible ? 1 : 0;
}

void NGTilemapGetDimensions(NGTilemapHandle handle, u16 *width_out, u16 *height_out) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) {
        if (width_out) *width_out = 0;
        if (height_out) *height_out = 0;
        return;
    }
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset) {
        if (width_out) *width_out = 0;
        if (height_out) *height_out = 0;
        return;
    }
    if (width_out) *width_out = tm->asset->width_tiles * NG_TILE_SIZE;
    if (height_out) *height_out = tm->asset->height_tiles * NG_TILE_SIZE;
}

u8 NGTilemapGetCollision(NGTilemapHandle handle, fixed world_x, fixed world_y) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data) return 0;

    s16 tile_x = FIX_INT(world_x - tm->world_x) / NG_TILE_SIZE;
    s16 tile_y = FIX_INT(world_y - tm->world_y) / NG_TILE_SIZE;

    if (tile_x < 0 || tile_x >= tm->asset->width_tiles ||
        tile_y < 0 || tile_y >= tm->asset->height_tiles) {
        return 0;
    }

    u16 idx = tile_y * tm->asset->width_tiles + tile_x;
    return tm->asset->collision_data[idx];
}

u8 NGTilemapGetTileAt(NGTilemapHandle handle, u16 tile_x, u16 tile_y) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset) return 0;

    if (tile_x >= tm->asset->width_tiles || tile_y >= tm->asset->height_tiles) {
        return 0;
    }

    u16 idx = tile_y * tm->asset->width_tiles + tile_x;
    return tm->asset->tile_data[idx];
}

u8 NGTilemapTestAABB(NGTilemapHandle handle,
                     fixed x, fixed y,
                     fixed half_w, fixed half_h,
                     u8 *flags_out) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data) return 0;

    s16 left_tile   = FIX_INT(x - half_w - tm->world_x) / NG_TILE_SIZE;
    s16 right_tile  = FIX_INT(x + half_w - tm->world_x) / NG_TILE_SIZE;
    s16 top_tile    = FIX_INT(y - half_h - tm->world_y) / NG_TILE_SIZE;
    s16 bottom_tile = FIX_INT(y + half_h - tm->world_y) / NG_TILE_SIZE;

    if (left_tile < 0) left_tile = 0;
    if (right_tile >= tm->asset->width_tiles) right_tile = tm->asset->width_tiles - 1;
    if (top_tile < 0) top_tile = 0;
    if (bottom_tile >= tm->asset->height_tiles) bottom_tile = tm->asset->height_tiles - 1;

    u8 result = 0;
    for (s16 ty = top_tile; ty <= bottom_tile; ty++) {
        for (s16 tx = left_tile; tx <= right_tile; tx++) {
            u16 idx = ty * tm->asset->width_tiles + tx;
            u8 coll = tm->asset->collision_data[idx];
            result |= coll;
        }
    }

    if (flags_out) *flags_out = result;
    return (result & NG_TILE_SOLID) ? 1 : 0;
}

u8 NGTilemapResolveAABB(NGTilemapHandle handle,
                        fixed *x, fixed *y,
                        fixed half_w, fixed half_h,
                        fixed *vel_x, fixed *vel_y) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return NG_COLL_NONE;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset || !tm->asset->collision_data) return NG_COLL_NONE;

    u8 result = NG_COLL_NONE;
    fixed new_x = *x;
    fixed new_y = *y + *vel_y;

    // Vertical resolution first: allows jumping to clear ground before horizontal check
    if (*vel_y != 0) {
        s16 left_tile   = FIX_INT(*x - half_w - tm->world_x) / NG_TILE_SIZE;
        s16 right_tile  = FIX_INT(*x + half_w - tm->world_x) / NG_TILE_SIZE;
        s16 top_tile    = FIX_INT(new_y - half_h - tm->world_y) / NG_TILE_SIZE;
        s16 bottom_tile = FIX_INT(new_y + half_h - tm->world_y) / NG_TILE_SIZE;

        if (left_tile < 0) left_tile = 0;
        if (right_tile >= tm->asset->width_tiles) right_tile = tm->asset->width_tiles - 1;
        if (top_tile < 0) top_tile = 0;
        if (bottom_tile >= tm->asset->height_tiles) bottom_tile = tm->asset->height_tiles - 1;

        u8 hit = 0;
        for (s16 ty = top_tile; ty <= bottom_tile && !hit; ty++) {
            for (s16 tx = left_tile; tx <= right_tile && !hit; tx++) {
                u16 idx = ty * tm->asset->width_tiles + tx;
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
                s16 tile_top = (bottom_tile * NG_TILE_SIZE) + FIX_INT(tm->world_y);
                new_y = FIX(tile_top) - half_h - 1;
            } else {
                result |= NG_COLL_TOP;
                s16 tile_bottom = ((top_tile + 1) * NG_TILE_SIZE) + FIX_INT(tm->world_y);
                new_y = FIX(tile_bottom) + half_h + 1;
            }
            *vel_y = 0;
        }
    }

    // Horizontal resolution uses resolved Y position
    if (*vel_x != 0) {
        new_x = *x + *vel_x;

        // 2px skin avoids catching on edges
        s16 left_tile   = FIX_INT(new_x - half_w - tm->world_x) / NG_TILE_SIZE;
        s16 right_tile  = FIX_INT(new_x + half_w - tm->world_x) / NG_TILE_SIZE;
        s16 top_tile    = FIX_INT(new_y - half_h + FIX(2) - tm->world_y) / NG_TILE_SIZE;
        s16 bottom_tile = FIX_INT(new_y + half_h - FIX(2) - tm->world_y) / NG_TILE_SIZE;

        if (left_tile < 0) left_tile = 0;
        if (right_tile >= tm->asset->width_tiles) right_tile = tm->asset->width_tiles - 1;
        if (top_tile < 0) top_tile = 0;
        if (bottom_tile >= tm->asset->height_tiles) bottom_tile = tm->asset->height_tiles - 1;

        u8 hit = 0;
        for (s16 ty = top_tile; ty <= bottom_tile && !hit; ty++) {
            for (s16 tx = left_tile; tx <= right_tile && !hit; tx++) {
                u16 idx = ty * tm->asset->width_tiles + tx;
                if (tm->asset->collision_data[idx] & NG_TILE_SOLID) {
                    hit = 1;
                }
            }
        }

        if (hit) {
            if (*vel_x > 0) {
                result |= NG_COLL_RIGHT;
                s16 tile_left = (right_tile * NG_TILE_SIZE) + FIX_INT(tm->world_x);
                new_x = FIX(tile_left) - half_w - 1;
            } else {
                result |= NG_COLL_LEFT;
                s16 tile_right = ((left_tile + 1) * NG_TILE_SIZE) + FIX_INT(tm->world_x);
                new_x = FIX(tile_right) + half_w + 1;
            }
            *vel_x = 0;
        }
    }

    *x = new_x;
    *y = new_y;
    return result;
}

void NGTilemapSetTile(NGTilemapHandle handle, u16 tile_x, u16 tile_y, u8 tile_index) {
    (void)handle;
    (void)tile_x;
    (void)tile_y;
    (void)tile_index;
}

void NGTilemapSetCollision(NGTilemapHandle handle, u16 tile_x, u16 tile_y, u8 collision) {
    (void)handle;
    (void)tile_x;
    (void)tile_y;
    (void)collision;
}

void NGTilemapDraw(NGTilemapHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->in_scene) return;

    draw_tilemap(tm, first_sprite);
}

u8 NGTilemapGetSpriteCount(NGTilemapHandle handle) {
    if (handle < 0 || handle >= NG_TILEMAP_MAX) return 0;
    Tilemap *tm = &tilemaps[handle];
    if (!tm->active || !tm->asset) return 0;

    u8 zoom = NGCameraGetZoom();
    u16 view_width = (SCREEN_WIDTH * 16) / zoom;
    u8 num_cols = (view_width / NG_TILE_SIZE) + 2;
    if (num_cols > NG_TILEMAP_MAX_COLS) num_cols = NG_TILEMAP_MAX_COLS;

    return num_cols;
}
