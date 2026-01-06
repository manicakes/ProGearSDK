/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <scene.h>
#include <actor.h>
#include <parallax.h>
#include <tilemap.h>
#include <camera.h>
#include <neogeo.h>

typedef struct {
    u8 type;
    s8 handle;
    u8 z;
} RenderEntry;

#define RENDER_TYPE_ACTOR    0
#define RENDER_TYPE_PARALLAX 1
#define RENDER_TYPE_TILEMAP  2
#define MAX_RENDER_ENTRIES  (NG_ACTOR_MAX + NG_PARALLAX_MAX + NG_TILEMAP_MAX)

static RenderEntry render_queue[MAX_RENDER_ENTRIES];
static u8 render_count;
static u8 scene_initialized;

// Render queue rebuilt only when scene composition or Z-order changes
static u8 render_queue_dirty;

static u16 hw_sprite_next;
static u16 hw_sprite_ui_next;
static u16 hw_sprite_last_max;
static u16 hw_sprite_last_ui_max;
#define HW_SPRITE_FIRST    1
#define HW_SPRITE_MAX    380
#define HW_SPRITE_UI_BASE 350

extern void _NGActorSystemInit(void);
extern void _NGActorSystemUpdate(void);
extern u8 _NGActorIsInScene(NGActorHandle handle);
extern u8 _NGActorGetZ(NGActorHandle handle);
extern u8 _NGActorIsScreenSpace(NGActorHandle handle);
extern void NGActorDraw(NGActorHandle handle, u16 first_sprite);
extern u8 NGActorGetSpriteCount(NGActorHandle handle);

extern void _NGParallaxSystemInit(void);
extern void _NGParallaxSystemUpdate(void);
extern u8 _NGParallaxIsInScene(NGParallaxHandle handle);
extern u8 _NGParallaxGetZ(NGParallaxHandle handle);
extern void NGParallaxDraw(NGParallaxHandle handle, u16 first_sprite);
extern u8 NGParallaxGetSpriteCount(NGParallaxHandle handle);
extern void NGParallaxDestroy(NGParallaxHandle handle);
extern void NGActorDestroy(NGActorHandle handle);

extern void _NGTilemapSystemInit(void);
extern u8 _NGTilemapIsInScene(NGTilemapHandle handle);
extern u8 _NGTilemapGetZ(NGTilemapHandle handle);
extern void NGTilemapDraw(NGTilemapHandle handle, u16 first_sprite);
extern u8 NGTilemapGetSpriteCount(NGTilemapHandle handle);

void _NGSceneMarkRenderQueueDirty(void) {
    render_queue_dirty = 1;
}

static void sort_render_queue(void) {
    for (u8 i = 1; i < render_count; i++) {
        // Save the element to insert
        u8 temp_type = render_queue[i].type;
        s8 temp_handle = render_queue[i].handle;
        u8 temp_z = render_queue[i].z;

        s8 j = i - 1;
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

    for (s8 i = 0; i < NG_PARALLAX_MAX; i++) {
        if (_NGParallaxIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_PARALLAX;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _NGParallaxGetZ(i);
                render_count++;
            }
        }
    }

    for (s8 i = 0; i < NG_TILEMAP_MAX; i++) {
        if (_NGTilemapIsInScene(i)) {
            if (render_count < MAX_RENDER_ENTRIES) {
                render_queue[render_count].type = RENDER_TYPE_TILEMAP;
                render_queue[render_count].handle = i;
                render_queue[render_count].z = _NGTilemapGetZ(i);
                render_count++;
            }
        }
    }

    sort_render_queue();
}

void NGSceneInit(void) {
    _NGActorSystemInit();
    _NGParallaxSystemInit();
    _NGTilemapSystemInit();

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;

    NG_REG_VRAMADDR = 0x8200;
    NG_REG_VRAMMOD = 1;
    for (u16 i = 0; i < HW_SPRITE_MAX; i++) {
        NG_REG_VRAMDATA = 0;  // Height 0 = invisible
    }

    scene_initialized = 1;
}

void NGSceneUpdate(void) {
    if (!scene_initialized) return;

    NGCameraUpdate();
    _NGActorSystemUpdate();
    _NGParallaxSystemUpdate();
}

void NGSceneDraw(void) {
    if (!scene_initialized) return;

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
        } else if (entry->type == RENDER_TYPE_PARALLAX) {
            u8 sprite_count = NGParallaxGetSpriteCount(entry->handle);
            NGParallaxDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        } else if (entry->type == RENDER_TYPE_TILEMAP) {
            u8 sprite_count = NGTilemapGetSpriteCount(entry->handle);
            NGTilemapDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        }
    }

    if (hw_sprite_next < hw_sprite_last_max) {
        NG_REG_VRAMADDR = 0x8200 + hw_sprite_next;
        NG_REG_VRAMMOD = 1;
        for (u16 i = hw_sprite_next; i < hw_sprite_last_max; i++) {
            NG_REG_VRAMDATA = 0;
        }
    }

    if (hw_sprite_ui_next < hw_sprite_last_ui_max) {
        NG_REG_VRAMADDR = 0x8200 + hw_sprite_ui_next;
        NG_REG_VRAMMOD = 1;
        for (u16 i = hw_sprite_ui_next; i < hw_sprite_last_ui_max; i++) {
            NG_REG_VRAMDATA = 0;
        }
    }

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

    for (s8 i = 0; i < NG_PARALLAX_MAX; i++) {
        NGParallaxDestroy(i);
    }

    for (s8 i = 0; i < NG_TILEMAP_MAX; i++) {
        NGTilemapDestroy(i);
    }

    NG_REG_VRAMADDR = 0x8200;
    NG_REG_VRAMMOD = 1;
    for (u16 i = 0; i < HW_SPRITE_MAX; i++) {
        NG_REG_VRAMDATA = 0;
    }

    render_count = 0;
    render_queue_dirty = 1;

    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;
}
