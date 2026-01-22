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
#include <graphic.h>

static u8 scene_initialized;

// Scene terrain state
static NGTerrainHandle scene_terrain = NG_TERRAIN_INVALID;
static u8 terrain_z;
static u8 terrain_in_scene;

extern void _NGActorSystemInit(void);
extern void _NGActorSystemUpdate(void);
extern void _NGActorSyncGraphics(void);

extern void _NGBackdropSystemInit(void);
extern void _NGBackdropSystemUpdate(void);
extern void _NGBackdropSyncGraphics(void);
extern void NGBackdropDestroy(NGBackdropHandle handle);

extern void _NGTerrainSystemInit(void);
extern void _NGTerrainSyncGraphics(void);
extern void NGActorDestroy(NGActorHandle handle);

/* Kept for backwards compatibility - no longer used internally */
void _NGSceneMarkRenderQueueDirty(void) {
    (void)0;
}

void NGSceneInit(void) {
    NGGraphicSystemInit();
    _NGActorSystemInit();
    _NGBackdropSystemInit();
    _NGTerrainSystemInit();

    scene_terrain = NG_TERRAIN_INVALID;
    terrain_z = 0;
    terrain_in_scene = 0;

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

    /* Sync all scene objects to their graphics */
    _NGBackdropSyncGraphics();
    _NGTerrainSyncGraphics();
    _NGActorSyncGraphics();

    /* Graphics system handles all rendering */
    NGGraphicSystemDraw();
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

    // Reset graphics system
    NGGraphicSystemReset();
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
        if (terrain_in_scene && scene_terrain != NG_TERRAIN_INVALID) {
            NGTerrainSetZ(scene_terrain, z);
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
