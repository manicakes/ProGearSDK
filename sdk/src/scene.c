/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <scene.h>
#include <actor.h>
#include <backdrop.h>
#include <terrain.h>
#include <camera.h>
#include <sprite.h>

typedef struct {
    u8 type;
    s8 handle;
    u8 z;
} RenderEntry;

#define RENDER_TYPE_ACTOR    0
#define RENDER_TYPE_BACKDROP 1
#define RENDER_TYPE_TERRAIN  2
#define MAX_RENDER_ENTRIES   (NG_ACTOR_MAX + NG_BACKDROP_MAX + 1)

static RenderEntry render_queue[MAX_RENDER_ENTRIES];
static u8 render_count;
static u8 scene_initialized;

// Render queue rebuilt only when scene composition or Z-order changes
static u8 render_queue_dirty;

static u16 hw_sprite_next;
static u16 hw_sprite_ui_next;
static u16 hw_sprite_last_max;
static u16 hw_sprite_last_ui_max;
#define HW_SPRITE_FIRST   1
#define HW_SPRITE_MAX     380
#define HW_SPRITE_UI_BASE 350

// Scene terrain state
static NGTerrainHandle scene_terrain = NG_TERRAIN_INVALID;
static u8 terrain_z;
static u8 terrain_in_scene;

extern void _NGActorSystemInit(void);
extern void _NGActorSystemUpdate(void);
extern u8 _NGActorIsInScene(NGActorHandle handle);
extern u8 _NGActorGetZ(NGActorHandle handle);
extern u8 _NGActorIsScreenSpace(NGActorHandle handle);
extern void NGActorDraw(NGActorHandle handle, u16 first_sprite);
extern u8 NGActorGetSpriteCount(NGActorHandle handle);

extern void _NGBackdropSystemInit(void);
extern void _NGBackdropSystemUpdate(void);
extern u8 _NGBackdropIsInScene(NGBackdropHandle handle);
extern u8 _NGBackdropGetZ(NGBackdropHandle handle);
extern void NGBackdropDraw(NGBackdropHandle handle, u16 first_sprite);
extern u8 NGBackdropGetSpriteCount(NGBackdropHandle handle);
extern void NGBackdropDestroy(NGBackdropHandle handle);
extern void NGActorDestroy(NGActorHandle handle);

extern void _NGTerrainSystemInit(void);
extern void NGTerrainDraw(NGTerrainHandle handle, u16 first_sprite);
extern u8 NGTerrainGetSpriteCount(NGTerrainHandle handle);

static void clear_unused_sprites(u16 current, u16 last_max) {
    if (current >= last_max)
        return;
    NGSpriteHideRange(current, (u8)(last_max - current));
}

void _NGSceneMarkRenderQueueDirty(void) {
    render_queue_dirty = 1;
}

static void sort_render_queue(void) {
    for (u8 i = 1; i < render_count; i++) {
        // Save the element to insert
        u8 temp_type = render_queue[i].type;
        s8 temp_handle = render_queue[i].handle;
        u8 temp_z = render_queue[i].z;

        s8 j = (s8)(i - 1);
        while (j >= 0 && render_queue[j].z > temp_z) {
            render_queue[j + 1].type = render_queue[j].type;
            render_queue[j + 1].handle = render_queue[j].handle;
            render_queue[j + 1].z = render_queue[j].z;
            j--;
        }
        render_queue[j + 1].type = temp_type;
        render_queue[j + 1].handle = temp_handle;
        render_queue[j + 1].z = temp_z;
    }
}

static void build_render_queue(void) {
    render_count = 0;

    for (s8 i = 0; i < NG_ACTOR_MAX; i++) {
        if (_NGActorIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_ACTOR;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _NGActorGetZ(i);
                render_count++;
            }
        }
    }

    for (s8 i = 0; i < NG_BACKDROP_MAX; i++) {
        if (_NGBackdropIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_BACKDROP;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _NGBackdropGetZ(i);
                render_count++;
            }
        }
    }

    // Add terrain if active
    if (terrain_in_scene && scene_terrain != NG_TERRAIN_INVALID) {
        if (render_count < MAX_RENDER_ENTRIES) {
            render_queue[render_count].type = RENDER_TYPE_TERRAIN;
            render_queue[render_count].handle = scene_terrain;
            render_queue[render_count].z = terrain_z;
            render_count++;
        }
    }

    sort_render_queue();
}

void NGSceneInit(void) {
    _NGActorSystemInit();
    _NGBackdropSystemInit();
    _NGTerrainSystemInit();

    scene_terrain = NG_TERRAIN_INVALID;
    terrain_z = 0;
    terrain_in_scene = 0;

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;

    /* Clear all sprites by hiding them */
    NGSpriteHideRange(0, 255);
    NGSpriteHideRange(255, (u8)(HW_SPRITE_MAX - 255));

    scene_initialized = 1;
}

void NGSceneUpdate(void) {
    if (!scene_initialized)
        return;

    NGCameraUpdate();
    _NGActorSystemUpdate();
    _NGBackdropSystemUpdate();
}

void NGSceneDraw(void) {
    if (!scene_initialized)
        return;

    if (render_queue_dirty) {
        build_render_queue();
        render_queue_dirty = 0;
    }

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;

    // NeoGeo renders higher sprite indices on top (painter's algorithm).
    // Screen-space actors use reserved high range for stable allocation.
    for (u8 i = 0; i < render_count; i++) {
        RenderEntry *entry = &render_queue[i];

        if (entry->type == RENDER_TYPE_ACTOR) {
            u8 sprite_count = NGActorGetSpriteCount(entry->handle);

            if (_NGActorIsScreenSpace(entry->handle)) {
                NGActorDraw(entry->handle, hw_sprite_ui_next);
                hw_sprite_ui_next += sprite_count;
            } else {
                NGActorDraw(entry->handle, hw_sprite_next);
                hw_sprite_next += sprite_count;
            }
        } else if (entry->type == RENDER_TYPE_BACKDROP) {
            u8 sprite_count = NGBackdropGetSpriteCount(entry->handle);
            NGBackdropDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        } else if (entry->type == RENDER_TYPE_TERRAIN) {
            u8 sprite_count = NGTerrainGetSpriteCount(entry->handle);
            NGTerrainDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        }
    }

    clear_unused_sprites(hw_sprite_next, hw_sprite_last_max);
    clear_unused_sprites(hw_sprite_ui_next, hw_sprite_last_ui_max);

    hw_sprite_last_max = hw_sprite_next;
    hw_sprite_last_ui_max = hw_sprite_ui_next;
}

u16 NGSceneAllocSprites(u8 count) {
    if (hw_sprite_next + count > HW_SPRITE_MAX) {
        return 0xFFFF;
    }
    u16 first = hw_sprite_next;
    hw_sprite_next += count;
    return first;
}

u16 NGSceneGetNextSprite(void) {
    return hw_sprite_next;
}

void NGSceneSetNextSprite(u16 next) {
    hw_sprite_next = next;
}

void NGSceneReset(void) {
    for (s8 i = 0; i < NG_ACTOR_MAX; i++) {
        NGActorDestroy(i);
    }

    for (s8 i = 0; i < NG_BACKDROP_MAX; i++) {
        NGBackdropDestroy(i);
    }

    // Clear terrain
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainDestroy(scene_terrain);
        scene_terrain = NG_TERRAIN_INVALID;
        terrain_in_scene = 0;
    }

    /* Clear all sprites by hiding them */
    NGSpriteHideRange(0, 255);
    NGSpriteHideRange(255, (u8)(HW_SPRITE_MAX - 255));

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;
}

/* === Terrain API Implementation === */

void NGSceneSetTerrain(const struct NGTerrainAsset *asset) {
    // Clear existing terrain if any
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainDestroy(scene_terrain);
    }

    if (!asset) {
        scene_terrain = NG_TERRAIN_INVALID;
        terrain_in_scene = 0;
        render_queue_dirty = 1;
        return;
    }

    scene_terrain = NGTerrainCreate(asset);
    if (scene_terrain != NG_TERRAIN_INVALID) {
        terrain_in_scene = 1;
        terrain_z = 0;
        // Add to scene at origin - this sets tm->in_scene flag needed for rendering
        NGTerrainAddToScene(scene_terrain, 0, 0, terrain_z);
        NGTerrainSetVisible(scene_terrain, 1);
    }
}

void NGSceneClearTerrain(void) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainDestroy(scene_terrain);
        scene_terrain = NG_TERRAIN_INVALID;
        terrain_in_scene = 0;
        render_queue_dirty = 1;
    }
}

void NGSceneSetTerrainPos(fixed x, fixed y) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainSetPos(scene_terrain, x, y);
    }
}

void NGSceneSetTerrainZ(u8 z) {
    if (terrain_z != z) {
        terrain_z = z;
        if (terrain_in_scene) {
            render_queue_dirty = 1;
        }
    }
}

void NGSceneSetTerrainVisible(u8 visible) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainSetVisible(scene_terrain, visible);
    }
}

void NGSceneGetTerrainBounds(u16 *width, u16 *height) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainGetDimensions(scene_terrain, width, height);
    } else {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
    }
}

u8 NGSceneGetCollisionAt(fixed x, fixed y) {
    if (scene_terrain == NG_TERRAIN_INVALID)
        return 0;
    return NGTerrainGetCollision(scene_terrain, x, y);
}

u8 NGSceneTestCollision(fixed x, fixed y, fixed half_w, fixed half_h, u8 *flags_out) {
    if (scene_terrain == NG_TERRAIN_INVALID) {
        if (flags_out)
            *flags_out = 0;
        return 0;
    }
    return NGTerrainTestAABB(scene_terrain, x, y, half_w, half_h, flags_out);
}

u8 NGSceneResolveCollision(fixed *x, fixed *y, fixed half_w, fixed half_h, fixed *vel_x,
                           fixed *vel_y) {
    if (scene_terrain == NG_TERRAIN_INVALID)
        return 0;
    return NGTerrainResolveAABB(scene_terrain, x, y, half_w, half_h, vel_x, vel_y);
}

u8 NGSceneGetTileAt(u16 tile_x, u16 tile_y) {
    if (scene_terrain == NG_TERRAIN_INVALID)
        return 0;
    return NGTerrainGetTileAt(scene_terrain, tile_x, tile_y);
}

void NGSceneSetTileAt(u16 tile_x, u16 tile_y, u8 tile_index) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainSetTile(scene_terrain, tile_x, tile_y, tile_index);
    }
}

void NGSceneSetCollisionAt(u16 tile_x, u16 tile_y, u8 collision) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainSetCollision(scene_terrain, tile_x, tile_y, collision);
    }
}
