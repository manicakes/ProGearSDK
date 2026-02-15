/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <actor.h>
#include <camera.h>
#include <graphic.h>
#include <ng_audio.h>

#include "sdk_internal.h"

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

    NGGraphic *graphic; // Graphics abstraction handles rendering
} Actor;

static Actor actors[NG_ACTOR_MAX];

static u8 str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}

void _NGActorSystemInit(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        actors[i].active = 0;
        actors[i].in_scene = 0;
        actors[i].graphic = NULL;
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

            // Update graphic with new animation frame
            if (actor->anim_frame != old_frame && actor->graphic) {
                u16 actual_frame = anim->first_frame + actor->anim_frame;
                NGGraphicSetFrame(actor->graphic, actual_frame);
            }
        }
    }
}

/**
 * Sync actor state to its graphic.
 * Called during scene draw to update graphic properties.
 */
static void sync_actor_graphic(Actor *actor) {
    if (!actor->graphic || !actor->asset)
        return;

    // Calculate screen position
    s16 screen_x, screen_y;
    u16 scale;

    if (actor->screen_space) {
        screen_x = FIX_INT(actor->x);
        screen_y = FIX_INT(actor->y);
        scale = NG_GRAPHIC_SCALE_ONE;
    } else {
        NGCameraWorldToScreen(actor->x, actor->y, &screen_x, &screen_y);
        // Convert camera zoom (1-16, where 16 is full) to scale (256 = 1.0x)
        u8 zoom = NGCameraGetZoom();
        scale = (u16)((zoom * NG_GRAPHIC_SCALE_ONE) >> 4);
    }

    NGGraphicSetPosition(actor->graphic, screen_x, screen_y);
    NGGraphicSetScale(actor->graphic, scale);

    // Update flip flags
    NGGraphicFlip flip = NG_GRAPHIC_FLIP_NONE;
    if (actor->h_flip)
        flip = (NGGraphicFlip)(flip | NG_GRAPHIC_FLIP_H);
    if (actor->v_flip)
        flip = (NGGraphicFlip)(flip | NG_GRAPHIC_FLIP_V);
    NGGraphicSetFlip(actor->graphic, flip);

    // Visibility
    NGGraphicSetVisible(actor->graphic, actor->visible);
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

    // Determine display dimensions
    u16 disp_w = width ? width : asset->width_pixels;
    u16 disp_h = height ? height : asset->height_pixels;

    // Create graphic for this actor
    NGGraphicConfig cfg = {.width = disp_w,
                           .height = disp_h,
                           .tile_mode = NG_GRAPHIC_TILE_REPEAT,
                           .layer = NG_GRAPHIC_LAYER_ENTITY,
                           .z_order = 0};
    actor->graphic = NGGraphicCreate(&cfg);
    if (!actor->graphic) {
        return NG_ACTOR_INVALID;
    }

    // Configure graphic source
    NGGraphicSetSource(actor->graphic, asset, asset->palette);

    // Initially hidden (not in scene yet)
    NGGraphicSetVisible(actor->graphic, 0);

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

    // Update graphic z-order and make visible
    if (actor->graphic) {
        NGGraphicSetZOrder(actor->graphic, z);
        NGGraphicSetLayer(actor->graphic,
                          actor->screen_space ? NG_GRAPHIC_LAYER_UI : NG_GRAPHIC_LAYER_ENTITY);
        NGGraphicSetVisible(actor->graphic, actor->visible);
        sync_actor_graphic(actor);
    }

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

    // Hide graphic
    if (actor->graphic) {
        NGGraphicSetVisible(actor->graphic, 0);
    }

    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGActorDestroy(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;

    Actor *actor = &actors[handle];

    // Destroy graphic
    if (actor->graphic) {
        NGGraphicDestroy(actor->graphic);
        actor->graphic = NULL;
    }

    NGActorRemoveFromScene(handle);
    actor->active = 0;
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
        if (actor->graphic) {
            NGGraphicSetZOrder(actor->graphic, z);
        }
        if (actor->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

NGVec2 NGActorGetPos(NGActorHandle handle) {
    NGVec2 pos = {0, 0};
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return pos;
    pos.x = actors[handle].x;
    pos.y = actors[handle].y;
    return pos;
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

        // Update graphic frame
        if (actor->graphic && actor->asset->anims) {
            const NGAnimDef *anim = &actor->asset->anims[anim_index];
            NGGraphicSetFrame(actor->graphic, anim->first_frame);
        }
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

        if (actor->graphic) {
            NGGraphicSetFrame(actor->graphic, frame);
        }
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

    // Only update graphic visibility if in scene
    if (actor->in_scene && actor->graphic) {
        NGGraphicSetVisible(actor->graphic, actor->visible);
    }
}

void NGActorSetPalette(NGActorHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    if (actor->palette != palette) {
        actor->palette = palette;

        // Update graphic source with new palette
        if (actor->graphic && actor->asset) {
            NGGraphicSetSource(actor->graphic, actor->asset, palette);
        }
    }
}

void NGActorSetHFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->h_flip = flip ? 1 : 0;
}

void NGActorSetVFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;
    actor->v_flip = flip ? 1 : 0;
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

        // Update graphic layer
        if (actor->graphic) {
            NGGraphicSetLayer(actor->graphic,
                              new_val ? NG_GRAPHIC_LAYER_UI : NG_GRAPHIC_LAYER_ENTITY);
        }
    }
}

/**
 * Sync all in-scene actors to their graphics.
 * Called by scene before graphic system draw.
 */
void _NGActorSyncGraphics(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        Actor *actor = &actors[i];
        if (actor->active && actor->in_scene) {
            sync_actor_graphic(actor);
        }
    }
}

/* Internal: collect palettes from all actors in scene into bitmask */
void _NGActorCollectPalettes(u8 *palette_mask) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        Actor *actor = &actors[i];
        if (actor->active && actor->in_scene && actor->visible) {
            u8 pal = actor->palette;
            palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
        }
    }
}

void NGActorPlaySfx(NGActorHandle handle, u8 sfx_index) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return;
    Actor *actor = &actors[handle];
    if (!actor->active)
        return;

    // Calculate screen position for panning
    s16 screen_x, screen_y;
    if (actor->screen_space) {
        screen_x = FIX_INT(actor->x);
    } else {
        NGCameraWorldToScreen(actor->x, actor->y, &screen_x, &screen_y);
    }

    // Map screen position to pan: left/center/right
    // Screen is 320 pixels wide, divide into thirds
    NGPan pan;
    if (screen_x < 107) {
        pan = NG_PAN_LEFT;
    } else if (screen_x > 213) {
        pan = NG_PAN_RIGHT;
    } else {
        pan = NG_PAN_CENTER;
    }

    NGSfxPlayPan(sfx_index, pan);
}
