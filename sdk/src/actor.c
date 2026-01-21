/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <actor.h>
#include <camera.h>
#include <hw/sprite.h>
#include <hw/lspc.h>

/* Internal actor data structure */
typedef struct {
    const NGVisualAsset *asset;
    fixed x, y;        /* Scene position */
    u8 z;              /* Z-index (render order) */
    u16 width, height; /* Display dimensions (0 = asset size) */
    u8 palette;
    u8 visible;
    u8 h_flip, v_flip;
    u8 in_scene;     /* Added to scene? */
    u8 active;       /* Slot in use? */
    u8 screen_space; /* If set, ignore camera (UI elements) */

    u8 anim_index;
    u16 anim_frame;
    u8 anim_counter;

    u16 hw_sprite_first;
    u8 hw_sprite_count;

    /* Dirty tracking: flags indicate which SCB registers need VRAM updates */
    u8 tiles_dirty;
    u8 shrink_dirty;
    u8 position_dirty;

    /* Cached values to detect changes */
    u16 last_anim_frame;
    u8 last_h_flip;
    u8 last_v_flip;
    u8 last_palette;
    u8 last_zoom;
    s16 last_screen_x;
    s16 last_screen_y;
    u8 last_cols;
} ActorData;

static ActorData actors[ACTOR_MAX];

static u8 str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}

extern void _SceneMarkRenderQueueDirty(void);

void _ActorSystemInit(void) {
    for (u8 i = 0; i < ACTOR_MAX; i++) {
        actors[i].active = 0;
        actors[i].in_scene = 0;
    }
}

void _ActorSystemUpdate(void) {
    for (u8 i = 0; i < ACTOR_MAX; i++) {
        ActorData *actor = &actors[i];
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

/**
 * Render an actor to VRAM.
 * Uses hw/sprite layer for optimized VRAM access.
 */
static void draw_actor(ActorData *actor, u16 first_sprite) {
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
        shrink = hw_sprite_full_shrink();
    } else {
        CameraWorldToScreen(actor->x, actor->y, &screen_x, &screen_y);
        zoom = CameraGetZoom();
        shrink = CameraGetShrink();
    }

    u8 first_draw = (actor->hw_sprite_first != first_sprite) || (actor->last_cols != cols);
    u8 zoom_changed = (zoom != actor->last_zoom);
    u8 position_changed = (screen_x != actor->last_screen_x) || (screen_y != actor->last_screen_y);

    if (actor->h_flip != actor->last_h_flip || actor->v_flip != actor->last_v_flip ||
        actor->palette != actor->last_palette || actor->anim_frame != actor->last_anim_frame) {
        actor->tiles_dirty = 1;
    }

    /* SCB1: Write tile indices and attributes (palette, flip bits) */
    if (first_draw || actor->tiles_dirty) {
        /* Build tile/attr arrays for each column and write via hw_sprite */
        u16 tiles[32];
        u16 attrs[32];

        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            u8 src_col = col % asset->width_tiles;

            for (u8 row = 0; row < rows; row++) {
                u8 src_row = row % asset->height_tiles;
                u8 tile_col = (u8)(actor->h_flip ? (asset->width_tiles - 1 - src_col) : src_col);
                u8 tile_row = (u8)(actor->v_flip ? (asset->height_tiles - 1 - src_row) : src_row);

                /* Column-major layout: each column is height_tiles sequential tiles */
                u16 tile_idx = (u16)(asset->base_tile + frame_offset +
                                     (tile_col * asset->height_tiles) + tile_row);

                tiles[row] = tile_idx;
                attrs[row] = hw_sprite_attr(actor->palette, actor->h_flip, actor->v_flip);
            }

            /* Write tiles for this sprite column */
            hw_sprite_write_tiles(spr, rows, tiles, attrs);
        }

        actor->last_anim_frame = actor->anim_frame;
        actor->last_h_flip = actor->h_flip;
        actor->last_v_flip = actor->v_flip;
        actor->last_palette = actor->palette;
        actor->tiles_dirty = 0;
    }

    /* SCB2: Shrink values */
    if (first_draw || zoom_changed) {
        hw_sprite_write_shrink(first_sprite, cols, shrink);
    }

    /* SCB3: Y position and height, SCB4: X position */
    if (first_draw || zoom_changed || position_changed) {
        /* Adjust height for zoom: at reduced zoom, shrunk graphics are shorter */
        u8 height_bits = hw_sprite_adjusted_height(rows, (u8)(shrink & 0xFF));

        hw_sprite_write_ychain(first_sprite, cols, screen_y, height_bits);
        hw_sprite_write_xchain(first_sprite, cols, screen_x, zoom);

        actor->last_screen_x = screen_x;
        actor->last_screen_y = screen_y;
    }

    actor->last_zoom = zoom;
    actor->hw_sprite_first = first_sprite;
    actor->hw_sprite_count = cols;
    actor->last_cols = cols;
}

u8 _ActorIsInScene(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].active && actors[handle].in_scene;
}

u8 _ActorGetZ(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].z;
}

u8 _ActorIsScreenSpace(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].active && actors[handle].screen_space;
}

Actor ActorCreate(const VisualAsset *asset) {
    return ActorCreateSized(asset, 0, 0);
}

Actor ActorCreateSized(const VisualAsset *asset, u16 width, u16 height) {
    if (!asset)
        return ACTOR_INVALID;

    Actor handle = ACTOR_INVALID;
    for (u8 i = 0; i < ACTOR_MAX; i++) {
        if (!actors[i].active) {
            handle = (Actor)i;
            break;
        }
    }
    if (handle == ACTOR_INVALID)
        return ACTOR_INVALID;

    ActorData *actor = &actors[handle];
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

void ActorAddToScene(Actor handle, fixed x, fixed y, u8 z) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;

    actor->x = x;
    actor->y = y;
    actor->z = z;
    actor->in_scene = 1;

    actor->tiles_dirty = 1;
    actor->hw_sprite_first = 0xFFFF;

    _SceneMarkRenderQueueDirty();
}

void ActorRemoveFromScene(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;

    u8 was_in_scene = actor->in_scene;
    actor->in_scene = 0;

    /* Hide sprites */
    if (actor->hw_sprite_count > 0) {
        hw_sprite_hide(actor->hw_sprite_first, actor->hw_sprite_count);
    }

    if (was_in_scene) {
        _SceneMarkRenderQueueDirty();
    }
}

void ActorDestroy(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorRemoveFromScene(handle);
    actors[handle].active = 0;
}

void ActorSetPos(Actor handle, fixed x, fixed y) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->x = x;
    actor->y = y;
}

void ActorMove(Actor handle, fixed dx, fixed dy) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->x += dx;
    actor->y += dy;
}

void ActorSetZ(Actor handle, u8 z) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    if (actor->z != z) {
        actor->z = z;
        if (actor->in_scene) {
            _SceneMarkRenderQueueDirty();
        }
    }
}

fixed ActorGetX(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].x;
}

fixed ActorGetY(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].y;
}

u8 ActorGetZ(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    return actors[handle].z;
}

void ActorSetAnim(Actor handle, u8 anim_index) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
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

u8 ActorPlayAnim(Actor handle, const char *name) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    ActorData *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims)
        return 0;

    for (u8 i = 0; i < actor->asset->anim_count; i++) {
        if (str_equal(actor->asset->anims[i].name, name)) {
            ActorSetAnim(handle, i);
            return 1;
        }
    }
    return 0;
}

void ActorSetFrame(Actor handle, u16 frame) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
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

u8 ActorAnimDone(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 1;
    ActorData *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims)
        return 1;
    if (actor->anim_index >= actor->asset->anim_count)
        return 1;

    const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];
    if (anim->loop)
        return 0;
    return (actor->anim_frame >= anim->frame_count - 1);
}

void ActorSetVisible(Actor handle, u8 visible) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->visible = visible ? 1 : 0;
}

void ActorSetPalette(Actor handle, u8 palette) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    if (actor->palette != palette) {
        actor->palette = palette;
        actor->tiles_dirty = 1;
    }
}

void ActorSetFlip(Actor handle, u8 h_flip, u8 v_flip) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    u8 new_h = h_flip ? 1 : 0;
    u8 new_v = v_flip ? 1 : 0;
    if (actor->h_flip != new_h || actor->v_flip != new_v) {
        actor->h_flip = new_h;
        actor->v_flip = new_v;
        actor->tiles_dirty = 1;
    }
}

void ActorSetScreenSpace(Actor handle, u8 enabled) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active)
        return;
    u8 new_val = enabled ? 1 : 0;
    if (actor->screen_space != new_val) {
        actor->screen_space = new_val;
        actor->shrink_dirty = 1;
        actor->position_dirty = 1;
    }
}

void _ActorDraw(Actor handle, u16 first_sprite) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return;
    ActorData *actor = &actors[handle];
    if (!actor->active || !actor->in_scene)
        return;

    draw_actor(actor, first_sprite);
}

u8 _ActorGetSpriteCount(Actor handle) {
    if (handle < 0 || handle >= ACTOR_MAX)
        return 0;
    ActorData *actor = &actors[handle];
    if (!actor->active || !actor->asset)
        return 0;

    u16 disp_w = actor->width ? actor->width : actor->asset->width_pixels;
    return (u8)((disp_w + TILE_SIZE - 1) / TILE_SIZE);
}

/* Internal: collect palettes from all actors in scene into bitmask */
void _ActorCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < ACTOR_MAX; i++) {
        ActorData *actor = &actors[i];
        if (actor->active && actor->in_scene && actor->visible) {
            u8 pal = actor->palette;
            palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
        }
    }
}
