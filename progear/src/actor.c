/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <actor.h>
#include <camera.h>
#include <engine.h>
#include <graphic.h>
#include <ng_audio.h>
#include <ng_string.h>

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

    s16 anchor_x, anchor_y; // Anchor offset from the frame's top-left, in pixels

    u8 anim_index;
    u16 anim_frame;
    u8 anim_counter;

    NGGraphic *graphic; // Graphics abstraction handles rendering
} Actor;

static Actor actors[NG_ACTOR_MAX];

/* Resolve a handle to an active actor, or NULL if out of range or inactive. */
static Actor *resolve_actor(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX)
        return NULL;
    Actor *actor = &actors[handle];
    return actor->active ? actor : NULL;
}

void _NGActorSystemInit(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        actors[i].active = 0;
        actors[i].in_scene = 0;
        actors[i].graphic = NULL;
    }
}

void _NGActorSystemUpdate(void) {
    u8 paused = NGEngineIsPaused();

    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        Actor *actor = &actors[i];
        if (!actor->active || !actor->in_scene || !actor->asset)
            continue;
        /* While paused the world freezes, but screen-space actors are UI
         * (menu cursors and the like) and keep animating. */
        if (paused && !actor->screen_space)
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
        scale = NGCameraZoomToScale(NGCameraGetZoom());
    }

    /* The graphic is placed by its top-left corner, so back the anchor off the
     * actor's position. A shrunk sprite occupies proportionally fewer pixels,
     * so the offset scales with it - that is what keeps a scaled sprite pinned
     * to its anchor instead of drifting. The common unscaled case skips the
     * multiply entirely. */
    if (actor->anchor_x | actor->anchor_y) {
        if (scale == NG_GRAPHIC_SCALE_ONE) {
            screen_x = (s16)(screen_x - actor->anchor_x);
            screen_y = (s16)(screen_y - actor->anchor_y);
        } else {
            screen_x = (s16)(screen_x - (s16)(((s32)actor->anchor_x * scale) >> 8));
            screen_y = (s16)(screen_y - (s16)(((s32)actor->anchor_y * scale) >> 8));
        }
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
    actor->anchor_x = 0; /* top-left: positions mean what they always did */
    actor->anchor_y = 0;
    actor->in_scene = 0;
    actor->active = 1;
    actor->screen_space = 0;
    actor->anim_index = 0;
    actor->anim_frame = 0;
    actor->anim_counter = 0;

    return handle;
}

void NGActorAddToScene(NGActorHandle handle, fixed x, fixed y, u8 z) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
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
}

void NGActorRemoveFromScene(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;

    actor->in_scene = 0;

    // Hide graphic
    if (actor->graphic) {
        NGGraphicSetVisible(actor->graphic, 0);
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

/* Display size the anchor is measured against (0 means "use the asset's"). */
static u16 actor_display_w(const Actor *actor) {
    return actor->width ? actor->width : actor->asset->width_pixels;
}

static u16 actor_display_h(const Actor *actor) {
    return actor->height ? actor->height : actor->asset->height_pixels;
}

/* One axis of a 3x3 anchor: 0 = near edge, 1 = centre, 2 = far edge */
static s16 anchor_axis(u16 size, u8 cell) {
    if (cell == 0)
        return 0;
    if (cell == 1)
        return (s16)(size >> 1);
    return (s16)size;
}

static void resolve_anchor(const Actor *actor, NGAnchor anchor, s16 *out_x, s16 *out_y) {
    /* Anchors are laid out row-major in a 3x3 grid, so column and row fall
     * out of the index without a divide. */
    static const u8 col[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
    static const u8 row[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};

    u8 i = (u8)anchor;
    if (i > NG_ANCHOR_BOTTOM_RIGHT)
        i = NG_ANCHOR_TOP_LEFT;

    *out_x = anchor_axis(actor_display_w(actor), col[i]);
    *out_y = anchor_axis(actor_display_h(actor), row[i]);
}

void NGActorSetAnchor(NGActorHandle handle, NGAnchor anchor) {
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset)
        return;
    resolve_anchor(actor, anchor, &actor->anchor_x, &actor->anchor_y);
}

void NGActorSetAnchorPixels(NGActorHandle handle, s16 x, s16 y) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->anchor_x = x;
    actor->anchor_y = y;
}

const NGVisualAsset *_NGActorGetAsset(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    return actor ? actor->asset : 0;
}

u16 _NGActorGetAnimFrame(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    return actor ? actor->anim_frame : 0;
}

u16 _NGActorGetAbsoluteFrame(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset)
        return 0;
    /* Without animations the actor sits on the frame set by NGActorSetFrame */
    if (!actor->asset->anims || actor->anim_index >= actor->asset->anim_count)
        return actor->anim_frame;
    return (u16)(actor->asset->anims[actor->anim_index].first_frame + actor->anim_frame);
}

u8 _NGActorGetHFlip(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    return actor ? actor->h_flip : 0;
}

NGVec2 NGActorGetAnchorPixels(NGActorHandle handle) {
    NGVec2 v = {0, 0};
    Actor *actor = resolve_actor(handle);
    if (actor) {
        v.x = actor->anchor_x;
        v.y = actor->anchor_y;
    }
    return v;
}

void NGActorSetPos(NGActorHandle handle, fixed x, fixed y) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->x = x;
    actor->y = y;
}

void NGActorSetPosAnchored(NGActorHandle handle, fixed x, fixed y, NGAnchor anchor) {
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset)
        return;

    /* Convert to the actor's own anchor so its stored position keeps meaning
     * what it always did. */
    s16 ax, ay;
    resolve_anchor(actor, anchor, &ax, &ay);
    actor->x = x + FIX(actor->anchor_x - ax);
    actor->y = y + FIX(actor->anchor_y - ay);
}

void NGActorMove(NGActorHandle handle, fixed dx, fixed dy) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->x += dx;
    actor->y += dy;
}

void NGActorSetZ(NGActorHandle handle, u8 z) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    if (actor->z != z) {
        actor->z = z;
        if (actor->graphic) {
            NGGraphicSetZOrder(actor->graphic, z);
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

struct NGGraphic *NGActorGetGraphic(NGActorHandle handle) {
    Actor *actor = resolve_actor(handle);
    return actor ? actor->graphic : NULL;
}

void NGActorSetAnim(NGActorHandle handle, u8 anim_index) {
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset)
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
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset || !actor->asset->anims)
        return 0;

    for (u8 i = 0; i < actor->asset->anim_count; i++) {
        if (ng_str_equal(actor->asset->anims[i].name, name)) {
            NGActorSetAnim(handle, i);
            return 1;
        }
    }
    return 0;
}

void NGActorSetFrame(NGActorHandle handle, u16 frame) {
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset)
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
    Actor *actor = resolve_actor(handle);
    if (!actor || !actor->asset || !actor->asset->anims)
        return 1;
    if (actor->anim_index >= actor->asset->anim_count)
        return 1;

    const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];
    if (anim->loop)
        return 0;
    return (actor->anim_frame >= anim->frame_count - 1);
}

void NGActorSetVisible(NGActorHandle handle, u8 visible) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->visible = visible ? 1 : 0;

    // Only update graphic visibility if in scene
    if (actor->in_scene && actor->graphic) {
        NGGraphicSetVisible(actor->graphic, actor->visible);
    }
}

void NGActorSetPalette(NGActorHandle handle, u8 palette) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
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
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->h_flip = flip ? 1 : 0;
}

void NGActorSetVFlip(NGActorHandle handle, u8 flip) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
        return;
    actor->v_flip = flip ? 1 : 0;
}

void NGActorSetScreenSpace(NGActorHandle handle, u8 enabled) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
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
            _NGPaletteMaskSet(palette_mask, actor->palette);
        }
    }
}

void NGActorPlaySfx(NGActorHandle handle, u8 sfx_index) {
    Actor *actor = resolve_actor(handle);
    if (!actor)
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
