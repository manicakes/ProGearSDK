/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <actor.h>
#include <camera.h>
#include <neogeo.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224
#define TILE_SIZE     16

#define SCB1_BASE 0x0000
#define SCB2_BASE 0x8000
#define SCB3_BASE 0x8200
#define SCB4_BASE 0x8400

typedef struct {
    const NGVisualAsset *asset;
    fixed x, y;        // Scene position
    u8 z;              // Z-index (render order)
    u16 width, height; // Display dimensions (0 = asset size)
    u8 palette;
    u8 visible;
    u8 h_flip, v_flip;
    u8 in_scene;     // Added to scene?
    u8 active;       // Slot in use?
    u8 screen_space; // If set, ignore camera (UI elements)

    u8 anim_index;
    u16 anim_frame;
    u8 anim_counter;

    u16 hw_sprite_first;
    u8 hw_sprite_count;

    // Dirty tracking: flags indicate which SCB registers need VRAM updates
    u8 tiles_dirty;
    u8 shrink_dirty;
    u8 position_dirty;

    // Cached values to detect changes
    u16 last_anim_frame;
    u8 last_h_flip;
    u8 last_v_flip;
    u8 last_palette;
    u8 last_zoom;
    s16 last_screen_x;
    s16 last_screen_y;
    u8 last_cols;
} Actor;

static Actor actors[NG_ACTOR_MAX];

static u8 str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}

extern void _NGSceneMarkRenderQueueDirty(void);

void _NGActorSystemInit(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        actors[i].active = 0;
        actors[i].in_scene = 0;
    }
}

void _NGActorSystemUpdate(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        Actor *actor = &actors[i];
        if (!actor->active || !actor->in_scene || !actor->asset)
            continue;
        if (!actor->asset->anims || actor->anim_index >= actor->asset->anim_count)
            continue;
        const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];

        actor->anim_counter++;
        if (actor->anim_counter >= anim->speed) {
            actor->anim_counter = 0;

            u16 old_frame = actor->anim_frame;
            actor->anim_frame++;
            if (actor->anim_frame >= anim->frame_count) {
                if (anim->loop) {
                    actor->anim_frame = 0;
                } else {
                    actor->anim_frame = anim->frame_count - 1;
                }
            }

            if (actor->anim_frame != old_frame) {
                actor->tiles_dirty = 1;
            }
        }
    }
}

static void draw_actor(Actor *actor, u16 first_sprite) {
    if (!actor->visible || !actor->asset)
        return;

    const NGVisualAsset *asset = actor->asset;

    u16 disp_w = actor->width ? actor->width : asset->width_pixels;
    u16 disp_h = actor->height ? actor->height : asset->height_pixels;

    u8 cols = (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
    u8 rows = (u8)((disp_h + TILE_SIZE - 1) / TILE_SIZE);
    if (rows > 32)
        rows = 32;

    u16 frame_offset = 0;
    if (asset->anims && actor->anim_index < asset->anim_count) {
        const NGAnimDef *anim = &asset->anims[actor->anim_index];
        u16 actual_frame = anim->first_frame + actor->anim_frame;
        frame_offset = actual_frame * asset->tiles_per_frame;
    }

    s16 screen_x, screen_y;
    u8 zoom;
    u16 shrink;

    if (actor->screen_space) {
        screen_x = FIX_INT(actor->x);
        screen_y = FIX_INT(actor->y);
        zoom = 16;
        shrink = 0x0FFF;
    } else {
        NGCameraWorldToScreen(actor->x, actor->y, &screen_x, &screen_y);
        zoom = NGCameraGetZoom();
        shrink = NGCameraGetShrink();
    }

    u8 first_draw = (actor->hw_sprite_first != first_sprite) || (actor->last_cols != cols);
    u8 zoom_changed = (zoom != actor->last_zoom);
    u8 position_changed = (screen_x != actor->last_screen_x) || (screen_y != actor->last_screen_y);

    if (actor->h_flip != actor->last_h_flip || actor->v_flip != actor->last_v_flip ||
        actor->palette != actor->last_palette || actor->anim_frame != actor->last_anim_frame) {
        actor->tiles_dirty = 1;
    }

    if (first_draw || actor->tiles_dirty) {
        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            u8 src_col = col % asset->width_tiles;

            NG_REG_VRAMADDR = SCB1_BASE + (spr * 64);
            NG_REG_VRAMMOD = 1;

            for (u8 row = 0; row < rows; row++) {
                u8 src_row = row % asset->height_tiles;
                u8 tile_col = (u8)(actor->h_flip ? (asset->width_tiles - 1 - src_col) : src_col);
                u8 tile_row = (u8)(actor->v_flip ? (asset->height_tiles - 1 - src_row) : src_row);

                // Column-major layout: each column is height_tiles sequential tiles
                u16 tile_idx = (u16)(asset->base_tile + frame_offset +
                                     (tile_col * asset->height_tiles) + tile_row);

                NG_REG_VRAMDATA = tile_idx & 0xFFFF;

                // Default h_flip=1 for correct display, inverted when user flips
                u16 attr = ((u16)actor->palette << 8);
                if (actor->v_flip)
                    attr |= 0x02;
                if (!actor->h_flip)
                    attr |= 0x01;
                NG_REG_VRAMDATA = attr;
            }

            for (u8 row = rows; row < 32; row++) {
                NG_REG_VRAMDATA = 0;
                NG_REG_VRAMDATA = 0;
            }
        }

        actor->last_anim_frame = actor->anim_frame;
        actor->last_h_flip = actor->h_flip;
        actor->last_v_flip = actor->v_flip;
        actor->last_palette = actor->palette;
        actor->tiles_dirty = 0;
    }

    if (first_draw || zoom_changed) {
        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            NG_REG_VRAMADDR = SCB2_BASE + spr;
            NG_REG_VRAMDATA = shrink;
        }
    }

    if (first_draw || zoom_changed || position_changed) {
        // Adjust height_bits for zoom: at reduced zoom, shrunk graphics are shorter
        // than the display window causing garbage. Reduce proportionally.
        u8 v_shrink = (u8)(shrink & 0xFF);
        u16 adjusted_rows = (u16)(((u16)rows * v_shrink + 254) / 255); // Ceiling division
        if (adjusted_rows < 1)
            adjusted_rows = 1;
        if (adjusted_rows > 32)
            adjusted_rows = 32;
        u8 height_bits = (u8)adjusted_rows;

        s16 y_val = 496 - screen_y; // NeoGeo Y: 496 at top, decreasing goes down
        if (y_val < 0)
            y_val += 512;
        y_val &= 0x1FF;

        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            u8 chain = (col > 0) ? 1 : 0;

            u16 scb3 = ((u16)y_val << 7) | (chain << 6) | height_bits;
            NG_REG_VRAMADDR = SCB3_BASE + spr;
            NG_REG_VRAMDATA = scb3;

            s16 col_offset = (s16)((col * TILE_SIZE * zoom) >> 4);
            s16 x_pos = (s16)((screen_x + col_offset) & 0x1FF);

            NG_REG_VRAMADDR = (vu16)(SCB4_BASE + spr);
            NG_REG_VRAMDATA = (vu16)(x_pos << 7);
        }

        actor->last_screen_x = screen_x;
        actor->last_screen_y = screen_y;
    }

    actor->last_zoom = zoom;
    actor->hw_sprite_first = first_sprite;
    actor->hw_sprite_count = cols;
    actor->last_cols = cols;
}

u8 _NGActorIsInScene(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].active && actors[handle].in_scene;
}

u8 _NGActorGetZ(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].z;
}

u8 _NGActorIsScreenSpace(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].active && actors[handle].screen_space;
}

NGActorHandle NGActorCreate(const NGVisualAsset *asset, u16 width, u16 height) {
    if (!asset)
        return NG_ACTOR_INVALID;

    NGActorHandle handle = NG_ACTOR_INVALID;
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        if (!actors[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_ACTOR_INVALID)
        return NG_ACTOR_INVALID;

    Actor *actor = &actors[handle];
    actor->asset = asset;
    actor->x = 0;
    actor->y = 0;
    actor->z = 0;
    actor->width = width;
    actor->height = height;
    actor->palette = asset->palette;
    actor->visible = 1;
    actor->h_flip = 0;
    actor->v_flip = 0;
    actor->in_scene = 0;
    actor->active = 1;
    actor->screen_space = 0;
    actor->anim_index = 0;
    actor->anim_frame = 0;
    actor->anim_counter = 0;
    actor->hw_sprite_first = 0xFFFF;
    actor->hw_sprite_count = 0;

    actor->tiles_dirty = 1;
    actor->shrink_dirty = 1;
    actor->position_dirty = 1;
    actor->last_anim_frame = 0xFFFF;
    actor->last_h_flip = 0xFF;
    actor->last_v_flip = 0xFF;
    actor->last_palette = 0xFF;
    actor->last_zoom = 0xFF;
    actor->last_screen_x = 0x7FFF;
    actor->last_screen_y = 0x7FFF;
    actor->last_cols = 0;

    return handle;
}

void NGActorAddToScene(NGActorHandle handle, fixed x, fixed y, u8 z) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;

    actor->x = x;
    actor->y = y;
    actor->z = z;
    actor->in_scene = 1;

    actor->tiles_dirty = 1;
    actor->hw_sprite_first = 0xFFFF;

    _NGSceneMarkRenderQueueDirty();
}

void NGActorRemoveFromScene(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;

    u8 was_in_scene = actor->in_scene;
    actor->in_scene = 0;

    if (actor->hw_sprite_count > 0) {
        for (u8 i = 0; i < actor->hw_sprite_count; i++) {
            NG_REG_VRAMADDR = (vu16)(SCB3_BASE + actor->hw_sprite_first + i);
            NG_REG_VRAMDATA = 0;
        }
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGActorDestroy(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    NGActorRemoveFromScene(handle);
    actors[handle].active = 0;
}

void NGActorSetPos(NGActorHandle handle, fixed x, fixed y) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->x = x;
    actor->y = y;
}

void NGActorMove(NGActorHandle handle, fixed dx, fixed dy) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->x += dx;
    actor->y += dy;
}

void NGActorSetZ(NGActorHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    if (actor->z != z) {
        actor->z = z;
        if (actor->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

fixed NGActorGetX(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].x;
}

fixed NGActorGetY(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].y;
}

u8 NGActorGetZ(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    return actors[handle].z;
}

void NGActorSetAnim(NGActorHandle handle, u8 anim_index) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset)
        return;
    if (anim_index >= actor->asset->anim_count)
        return;

    if (actor->anim_index != anim_index) {
        actor->anim_index = anim_index;
        actor->anim_frame = 0;
        actor->anim_counter = 0;
        actor->tiles_dirty = 1;
    }
}

u8 NGActorSetAnimByName(NGActorHandle handle, const char *name) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims)
        return 0;

    for (u8 i = 0; i < actor->asset->anim_count; i++) {
        if (str_equal(actor->asset->anims[i].name, name)) {
            NGActorSetAnim(handle, i);
            return 1;
        }
    }
    return 0;
}

void NGActorSetFrame(NGActorHandle handle, u16 frame) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset)
        return;
    if (frame >= actor->asset->frame_count)
        return;

    if (actor->anim_frame != frame) {
        actor->anim_frame = frame;
        actor->anim_counter = 0;
        actor->tiles_dirty = 1;
    }
}

u8 NGActorAnimDone(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 1;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims)
        return 1;
    if (actor->anim_index >= actor->asset->anim_count)
        return 1;

    const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];
    if (anim->loop)
        return 0;
    return (actor->anim_frame >= anim->frame_count - 1);
}

void NGActorSetVisible(NGActorHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->visible = visible ? 1 : 0;
}

void NGActorSetPalette(NGActorHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    if (actor->palette != palette) {
        actor->palette = palette;
        actor->tiles_dirty = 1;
    }
}

void NGActorSetHFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    u8 new_flip = flip ? 1 : 0;
    if (actor->h_flip != new_flip) {
        actor->h_flip = new_flip;
        actor->tiles_dirty = 1;
    }
}

void NGActorSetVFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    u8 new_flip = flip ? 1 : 0;
    if (actor->v_flip != new_flip) {
        actor->v_flip = new_flip;
        actor->tiles_dirty = 1;
    }
}

void NGActorSetScreenSpace(NGActorHandle handle, u8 enabled) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    u8 new_val = enabled ? 1 : 0;
    if (actor->screen_space != new_val) {
        actor->screen_space = new_val;
        actor->shrink_dirty = 1;
        actor->position_dirty = 1;
    }
}

void NGActorDraw(NGActorHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->in_scene)
        return;

    draw_actor(actor, first_sprite);
}

u8 NGActorGetSpriteCount(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return 0;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset)
        return 0;

    u16 disp_w = actor->width ? actor->width : actor->asset->width_pixels;
    return (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
}
