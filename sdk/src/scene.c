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
#include <hw/sprite.h>
#include <hw/lspc.h>

typedef struct {
    u8 type;
    s8 handle;
    u8 z;
} RenderEntry;

#define RENDER_TYPE_ACTOR    0
#define RENDER_TYPE_BACKDROP 1
#define RENDER_TYPE_TERRAIN  2
#define MAX_RENDER_ENTRIES   (ACTOR_MAX + BACKDROP_MAX + 1)

static RenderEntry render_queue[MAX_RENDER_ENTRIES];
static u8 render_count;
static u8 scene_initialized;

/* Render queue rebuilt only when scene composition or Z-order changes */
static u8 render_queue_dirty;

static u16 hw_sprite_next;
static u16 hw_sprite_ui_next;
static u16 hw_sprite_last_max;
static u16 hw_sprite_last_ui_max;
#define HW_SPRITE_FIRST   1
#define HW_SPRITE_MAX     380
#define HW_SPRITE_UI_BASE 350

/* Scene terrain state */
static TerrainHandle scene_terrain = TERRAIN_INVALID;
static u8 terrain_z;
static u8 terrain_in_scene;

/* External actor system functions */
extern void _ActorSystemInit(void);
extern void _ActorSystemUpdate(void);
extern u8 _ActorIsInScene(Actor handle);
extern u8 _ActorGetZ(Actor handle);
extern u8 _ActorIsScreenSpace(Actor handle);
extern void _ActorDraw(Actor handle, u16 first_sprite);
extern u8 _ActorGetSpriteCount(Actor handle);

/* External backdrop system functions */
extern void _BackdropSystemInit(void);
extern void _BackdropSystemUpdate(void);
extern u8 _BackdropIsInScene(Backdrop handle);
extern u8 _BackdropGetZ(Backdrop handle);
extern void _BackdropDraw(Backdrop handle, u16 first_sprite);
extern u8 _BackdropGetSpriteCount(Backdrop handle);

/* External terrain system functions */
extern void _TerrainSystemInit(void);
extern void _TerrainDraw(TerrainHandle handle, u16 first_sprite);
extern u8 _TerrainGetSpriteCount(TerrainHandle handle);

static void clear_unused_sprites(u16 current, u16 last_max) {
    if (current >= last_max)
        return;
    hw_sprite_hide(current, (u8)(last_max - current));
}

void _SceneMarkRenderQueueDirty(void) {
    render_queue_dirty = 1;
}

static void sort_render_queue(void) {
    for (u8 i = 1; i < render_count; i++) {
        /* Save the element to insert */
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

    for (s8 i = 0; i < ACTOR_MAX; i++) {
        if (_ActorIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_ACTOR;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _ActorGetZ(i);
                render_count++;
            }
        }
    }

    for (s8 i = 0; i < BACKDROP_MAX; i++) {
        if (_BackdropIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_BACKDROP;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _BackdropGetZ(i);
                render_count++;
            }
        }
    }

    /* Add terrain if active */
    if (terrain_in_scene && scene_terrain != TERRAIN_INVALID) {
        if (render_count < MAX_RENDER_ENTRIES) {
            render_queue[render_count].type = RENDER_TYPE_TERRAIN;
            render_queue[render_count].handle = scene_terrain;
            render_queue[render_count].z = terrain_z;
            render_count++;
        }
    }

    sort_render_queue();
}

void SceneInit(void) {
    _ActorSystemInit();
    _BackdropSystemInit();
    _TerrainSystemInit();

    scene_terrain = TERRAIN_INVALID;
    terrain_z = 0;
    terrain_in_scene = 0;

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;

    /* Hide all sprites */
    hw_sprite_hide_all();

    scene_initialized = 1;
}

void SceneUpdate(void) {
    if (!scene_initialized)
        return;

    CameraUpdate();
    _ActorSystemUpdate();
    _BackdropSystemUpdate();
}

void SceneDraw(void) {
    if (!scene_initialized)
        return;

    if (render_queue_dirty) {
        build_render_queue();
        render_queue_dirty = 0;
    }

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;

    /* NeoGeo renders higher sprite indices on top (painter's algorithm).
     * Screen-space actors use reserved high range for stable allocation. */
    for (u8 i = 0; i < render_count; i++) {
        RenderEntry *entry = &render_queue[i];

        if (entry->type == RENDER_TYPE_ACTOR) {
            u8 sprite_count = _ActorGetSpriteCount(entry->handle);

            if (_ActorIsScreenSpace(entry->handle)) {
                _ActorDraw(entry->handle, hw_sprite_ui_next);
                hw_sprite_ui_next += sprite_count;
            } else {
                _ActorDraw(entry->handle, hw_sprite_next);
                hw_sprite_next += sprite_count;
            }
        } else if (entry->type == RENDER_TYPE_BACKDROP) {
            u8 sprite_count = _BackdropGetSpriteCount(entry->handle);
            _BackdropDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        } else if (entry->type == RENDER_TYPE_TERRAIN) {
            u8 sprite_count = _TerrainGetSpriteCount(entry->handle);
            _TerrainDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        }
    }

    clear_unused_sprites(hw_sprite_next, hw_sprite_last_max);
    clear_unused_sprites(hw_sprite_ui_next, hw_sprite_last_ui_max);

    hw_sprite_last_max = hw_sprite_next;
    hw_sprite_last_ui_max = hw_sprite_ui_next;
}

u16 SceneAllocSprites(u8 count) {
    if (hw_sprite_next + count > HW_SPRITE_MAX) {
        return 0xFFFF;
    }
    u16 first = hw_sprite_next;
    hw_sprite_next += count;
    return first;
}

u16 SceneGetNextSprite(void) {
    return hw_sprite_next;
}

void SceneSetNextSprite(u16 next) {
    hw_sprite_next = next;
}

void SceneReset(void) {
    for (s8 i = 0; i < ACTOR_MAX; i++) {
        ActorDestroy(i);
    }

    for (s8 i = 0; i < BACKDROP_MAX; i++) {
        BackdropDestroy(i);
    }

    /* Clear terrain */
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainDestroy(scene_terrain);
        scene_terrain = TERRAIN_INVALID;
        terrain_in_scene = 0;
    }

    /* Hide all sprites */
    hw_sprite_hide_all();

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;
}

/* === Terrain API Implementation === */

void SceneSetTerrain(const struct TerrainAsset *asset) {
    /* Clear existing terrain if any */
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainDestroy(scene_terrain);
    }

    if (!asset) {
        scene_terrain = TERRAIN_INVALID;
        terrain_in_scene = 0;
        render_queue_dirty = 1;
        return;
    }

    scene_terrain = TerrainCreate(asset);
    if (scene_terrain != TERRAIN_INVALID) {
        terrain_in_scene = 1;
        terrain_z = 0;
        /* Add to scene at origin */
        TerrainAddToScene(scene_terrain, 0, 0, terrain_z);
        TerrainSetVisible(scene_terrain, 1);
    }
}

void SceneClearTerrain(void) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainDestroy(scene_terrain);
        scene_terrain = TERRAIN_INVALID;
        terrain_in_scene = 0;
        render_queue_dirty = 1;
    }
}

void SceneSetTerrainPos(fixed x, fixed y) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainSetPos(scene_terrain, x, y);
    }
}

void SceneSetTerrainZ(u8 z) {
    if (terrain_z != z) {
        terrain_z = z;
        if (terrain_in_scene) {
            render_queue_dirty = 1;
        }
    }
}

void SceneSetTerrainVisible(u8 visible) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainSetVisible(scene_terrain, visible);
    }
}

void SceneGetTerrainBounds(u16 *width, u16 *height) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainGetDimensions(scene_terrain, width, height);
    } else {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
    }
}

u8 SceneGetCollisionAt(fixed x, fixed y) {
    if (scene_terrain == TERRAIN_INVALID)
        return 0;
    return TerrainGetCollision(scene_terrain, x, y);
}

u8 SceneTestCollision(fixed x, fixed y, fixed half_w, fixed half_h, u8 *flags_out) {
    if (scene_terrain == TERRAIN_INVALID) {
        if (flags_out)
            *flags_out = 0;
        return 0;
    }
    return TerrainTestAABB(scene_terrain, x, y, half_w, half_h, flags_out);
}

u8 SceneResolveCollision(fixed *x, fixed *y, fixed half_w, fixed half_h, fixed *vel_x,
                         fixed *vel_y) {
    if (scene_terrain == TERRAIN_INVALID)
        return 0;
    return TerrainResolveAABB(scene_terrain, x, y, half_w, half_h, vel_x, vel_y);
}

u8 SceneGetTileAt(u16 tile_x, u16 tile_y) {
    if (scene_terrain == TERRAIN_INVALID)
        return 0;
    return TerrainGetTileAt(scene_terrain, tile_x, tile_y);
}

void SceneSetTileAt(u16 tile_x, u16 tile_y, u8 tile_index) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainSetTile(scene_terrain, tile_x, tile_y, tile_index);
    }
}

void SceneSetCollisionAt(u16 tile_x, u16 tile_y, u8 collision) {
    if (scene_terrain != TERRAIN_INVALID) {
        TerrainSetCollision(scene_terrain, tile_x, tile_y, collision);
    }
}
