/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// scene.c - Scene management implementation

#include <scene.h>
#include <actor.h>
#include <parallax.h>
#include <camera.h>
#include <neogeo.h>

// === Internal Types ===

// Render queue entry for Z-sorting
typedef struct {
    u8 type;       // 0 = actor, 1 = parallax
    s8 handle;     // Actor or parallax handle
    u8 z;          // Z-index for sorting
} RenderEntry;

#define RENDER_TYPE_ACTOR    0
#define RENDER_TYPE_PARALLAX 1

// Maximum total objects in scene
#define MAX_RENDER_ENTRIES  (NG_ACTOR_MAX + NG_PARALLAX_MAX)

// === Private State ===

static RenderEntry render_queue[MAX_RENDER_ENTRIES];
static u8 render_count;
static u8 scene_initialized;

// === Performance Optimization: Render Queue Caching ===
// The render queue is only rebuilt when scene composition or Z-order changes.
// This avoids iterating through all actors/parallax and sorting every frame.
static u8 render_queue_dirty;

// Hardware sprite tracking
static u16 hw_sprite_next;       // Next sprite for game objects (low end)
static u16 hw_sprite_ui_next;    // Next sprite for UI objects (high end, counting up)
static u16 hw_sprite_last_max;   // Highest game sprite used last frame
static u16 hw_sprite_last_ui_max; // Highest UI sprite used last frame
#define HW_SPRITE_FIRST    1    // Start after sprite 0 (often used by BIOS)
#define HW_SPRITE_MAX    380    // Max usable sprites
#define HW_SPRITE_UI_BASE 350   // UI sprites start here and go up (reserved range 350-380)

// === Forward Declarations (from actor.c and parallax.c) ===

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

// === Internal API for actor.c and parallax.c to mark scene dirty ===

void _NGSceneMarkRenderQueueDirty(void) {
    render_queue_dirty = 1;
}

// === Helper Functions ===

// Simple insertion sort for render queue (small N, partially sorted)
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

// Build render queue from active actors and parallax effects
static void build_render_queue(void) {
    render_count = 0;

    // Add all actors that are in the scene
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

    // Add all parallax effects that are in the scene
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

    // Sort by Z (lower Z = rendered first = behind)
    sort_render_queue();
}

// === Public Functions ===

void NGSceneInit(void) {
    // Initialize subsystems
    _NGActorSystemInit();
    _NGParallaxSystemInit();

    // Reset render queue
    render_count = 0;
    render_queue_dirty = 1;  // Force initial build

    // Reset hardware sprite allocation
    // Game objects use low sprites (1 upward to ~349)
    // UI objects use high sprites (350 upward to 380)
    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;

    // Clear all hardware sprites (once at init)
    NG_REG_VRAMADDR = 0x8200;  // SCB3 base
    NG_REG_VRAMMOD = 1;
    for (u16 i = 0; i < HW_SPRITE_MAX; i++) {
        NG_REG_VRAMDATA = 0;  // Height 0 = invisible
    }

    scene_initialized = 1;
}

void NGSceneUpdate(void) {
    if (!scene_initialized) return;

    // Update camera (handles zoom transitions, shake effects)
    NGCameraUpdate();

    // Update all actors (animations, etc.)
    _NGActorSystemUpdate();

    // Update all parallax effects
    _NGParallaxSystemUpdate();
}

void NGSceneDraw(void) {
    if (!scene_initialized) return;

    // === OPTIMIZATION: Only rebuild render queue when dirty ===
    // The queue is marked dirty when:
    // - An actor/parallax is added to or removed from scene
    // - An actor/parallax Z-index changes
    if (render_queue_dirty) {
        build_render_queue();
        render_queue_dirty = 0;
    }

    // Reset sprite allocation for this frame
    // Game objects: low sprites (1 upward to ~349)
    // UI objects (screen-space): high sprites (350 upward) - stable allocation!
    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;

    // Draw all objects in Z-order (low Z first = background first)
    // Neo Geo renders higher sprite indices ON TOP (painter's algorithm)
    // So low-Z objects get low sprites (behind), high-Z objects get high sprites (in front)
    //
    // Screen-space actors (UI) get allocated from a reserved high range for stability -
    // this prevents UI sprite indices from shifting when game objects are added/removed.
    for (u8 i = 0; i < render_count; i++) {
        RenderEntry *entry = &render_queue[i];

        if (entry->type == RENDER_TYPE_ACTOR) {
            u8 sprite_count = NGActorGetSpriteCount(entry->handle);

            // Screen-space actors use high sprites (stable allocation, 350+)
            if (_NGActorIsScreenSpace(entry->handle)) {
                NGActorDraw(entry->handle, hw_sprite_ui_next);
                hw_sprite_ui_next += sprite_count;
            } else {
                // World-space actors use low sprites
                NGActorDraw(entry->handle, hw_sprite_next);
                hw_sprite_next += sprite_count;
            }
        } else {
            u8 sprite_count = NGParallaxGetSpriteCount(entry->handle);
            NGParallaxDraw(entry->handle, hw_sprite_next);
            hw_sprite_next += sprite_count;
        }
    }

    // Clear game sprites that were used last frame but not this frame
    if (hw_sprite_next < hw_sprite_last_max) {
        NG_REG_VRAMADDR = 0x8200 + hw_sprite_next;  // SCB3
        NG_REG_VRAMMOD = 1;
        for (u16 i = hw_sprite_next; i < hw_sprite_last_max; i++) {
            NG_REG_VRAMDATA = 0;
        }
    }

    // Clear UI sprites that were used last frame but not this frame
    if (hw_sprite_ui_next < hw_sprite_last_ui_max) {
        NG_REG_VRAMADDR = 0x8200 + hw_sprite_ui_next;  // SCB3
        NG_REG_VRAMMOD = 1;
        for (u16 i = hw_sprite_ui_next; i < hw_sprite_last_ui_max; i++) {
            NG_REG_VRAMDATA = 0;
        }
    }

    // Track sprite usage for next frame's cleanup
    hw_sprite_last_max = hw_sprite_next;
    hw_sprite_last_ui_max = hw_sprite_ui_next;
}

// === Hardware Sprite Allocation ===

u16 NGSceneAllocSprites(u8 count) {
    if (hw_sprite_next + count > HW_SPRITE_MAX) {
        return 0xFFFF;  // Failed
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
    // Destroy all actors (including those not in scene)
    for (s8 i = 0; i < NG_ACTOR_MAX; i++) {
        NGActorDestroy(i);
    }

    // Destroy all parallax effects (including those not in scene)
    for (s8 i = 0; i < NG_PARALLAX_MAX; i++) {
        NGParallaxDestroy(i);
    }

    // Clear all hardware sprites
    NG_REG_VRAMADDR = 0x8200;  // SCB3 base
    NG_REG_VRAMMOD = 1;
    for (u16 i = 0; i < HW_SPRITE_MAX; i++) {
        NG_REG_VRAMDATA = 0;  // Height 0 = invisible
    }

    // Reset render queue
    render_count = 0;
    render_queue_dirty = 1;

    // Reset sprite allocation
    hw_sprite_next = HW_SPRITE_FIRST;
    hw_sprite_last_max = HW_SPRITE_FIRST;
    hw_sprite_ui_next = HW_SPRITE_UI_BASE;
    hw_sprite_last_ui_max = HW_SPRITE_UI_BASE;
}
