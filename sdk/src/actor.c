/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// actor.c - Actor management implementation

#include <actor.h>
#include <camera.h>
#include <neogeo.h>

// === Internal Constants ===

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  224
#define TILE_SIZE      16

// SCB base addresses
#define SCB1_BASE  0x0000
#define SCB2_BASE  0x8000
#define SCB3_BASE  0x8200
#define SCB4_BASE  0x8400

// === Internal Types ===

typedef struct {
    const NGVisualAsset *asset;
    fixed x, y;            // Scene position
    u8 z;                  // Z-index (render order)
    u16 width, height;     // Display dimensions (0 = asset size)
    u8 palette;
    u8 visible;
    u8 h_flip, v_flip;
    u8 in_scene;           // Added to scene?
    u8 active;             // Slot in use?
    u8 screen_space;       // If set, ignore camera (UI elements)

    // Animation state
    u8 anim_index;
    u16 anim_frame;
    u8 anim_counter;

    // Hardware sprite allocation
    u16 hw_sprite_first;
    u8 hw_sprite_count;

    // === Dirty tracking for performance optimization ===
    // These flags track what needs to be updated in VRAM
    u8 tiles_dirty;        // SCB1 needs update (frame/flip/palette changed)
    u8 shrink_dirty;       // SCB2 needs update (zoom changed)
    u8 position_dirty;     // SCB3/SCB4 needs update (position/zoom changed)

    // Cached values to detect changes
    u16 last_anim_frame;   // Last animation frame written to VRAM
    u8 last_h_flip;        // Last h_flip state
    u8 last_v_flip;        // Last v_flip state
    u8 last_palette;       // Last palette
    u8 last_zoom;          // Last zoom level
    s16 last_screen_x;     // Last screen X position
    s16 last_screen_y;     // Last screen Y position
    u8 last_cols;          // Last column count (for sprite reallocation detection)
} Actor;

// === Private State ===

static Actor actors[NG_ACTOR_MAX];

// === String Helper ===

static u8 str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

// === Forward declaration for scene integration ===
extern void _NGSceneMarkRenderQueueDirty(void);

// === Internal Functions (called by scene.c) ===

void _NGActorSystemInit(void) {
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        actors[i].active = 0;
        actors[i].in_scene = 0;
    }
}

void _NGActorSystemUpdate(void) {
    // Update animations for all active actors in scene
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        Actor *actor = &actors[i];
        if (!actor->active || !actor->in_scene || !actor->asset) continue;

        // Check if we have animations
        if (!actor->asset->anims || actor->anim_index >= actor->asset->anim_count) continue;
        const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];

        // Advance animation counter
        actor->anim_counter++;
        if (actor->anim_counter >= anim->speed) {
            actor->anim_counter = 0;

            // Advance frame
            u16 old_frame = actor->anim_frame;
            actor->anim_frame++;
            if (actor->anim_frame >= anim->frame_count) {
                if (anim->loop) {
                    actor->anim_frame = 0;
                } else {
                    actor->anim_frame = anim->frame_count - 1;
                }
            }

            // Mark tiles dirty if frame actually changed
            if (actor->anim_frame != old_frame) {
                actor->tiles_dirty = 1;
            }
        }
    }
}

// Draw a single actor
static void draw_actor(Actor *actor, u16 first_sprite) {
    if (!actor->visible || !actor->asset) return;

    const NGVisualAsset *asset = actor->asset;

    // Get display dimensions (0 = use asset size)
    u16 disp_w = actor->width ? actor->width : asset->width_pixels;
    u16 disp_h = actor->height ? actor->height : asset->height_pixels;

    // Calculate tiles needed
    u8 cols = (disp_w + TILE_SIZE - 1) / TILE_SIZE;
    u8 rows = (disp_h + TILE_SIZE - 1) / TILE_SIZE;
    if (rows > 32) rows = 32;  // Hardware limit

    // Calculate current frame tile offset
    u16 frame_offset = 0;
    if (asset->anims && actor->anim_index < asset->anim_count) {
        const NGAnimDef *anim = &asset->anims[actor->anim_index];
        u16 actual_frame = anim->first_frame + actor->anim_frame;
        frame_offset = actual_frame * asset->tiles_per_frame;
    }

    // Transform world position to screen position
    s16 screen_x, screen_y;
    u8 zoom;
    u16 shrink;

    if (actor->screen_space) {
        // Screen-space actors: use position directly, no camera transform or zoom
        screen_x = FIX_INT(actor->x);
        screen_y = FIX_INT(actor->y);
        zoom = 16;  // 100% zoom
        shrink = 0x0FFF;  // No shrink (full size)
    } else {
        // World-space actors: apply camera transform and zoom
        NGCameraWorldToScreen(actor->x, actor->y, &screen_x, &screen_y);
        zoom = NGCameraGetZoom();
        shrink = NGCameraGetShrink();
    }

    // Check if this is a new sprite allocation (first draw or sprite index changed)
    u8 first_draw = (actor->hw_sprite_first != first_sprite) || (actor->last_cols != cols);

    // Detect what changed since last frame
    u8 zoom_changed = (zoom != actor->last_zoom);
    u8 position_changed = (screen_x != actor->last_screen_x) || (screen_y != actor->last_screen_y);

    // Check for tile-affecting changes
    if (actor->h_flip != actor->last_h_flip ||
        actor->v_flip != actor->last_v_flip ||
        actor->palette != actor->last_palette ||
        actor->anim_frame != actor->last_anim_frame) {
        actor->tiles_dirty = 1;
    }

    // === SCB1: Tile data - ONLY when tiles_dirty or first draw ===
    if (first_draw || actor->tiles_dirty) {
        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;

            // Calculate source tile column (with wrapping for tiling)
            u8 src_col = col % asset->width_tiles;

            NG_REG_VRAMADDR = SCB1_BASE + (spr * 64);
            NG_REG_VRAMMOD = 1;

            // Write actual tile data
            for (u8 row = 0; row < rows; row++) {
                // Calculate source tile row (with wrapping for tiling)
                u8 src_row = row % asset->height_tiles;

                // Get tile from tilemap (handle flip by reversing order)
                u8 tile_col = actor->h_flip ? (asset->width_tiles - 1 - src_col) : src_col;
                u8 tile_row = actor->v_flip ? (asset->height_tiles - 1 - src_row) : src_row;

                // Column-major layout: each column is height_tiles sequential tiles
                u16 tile_idx = asset->base_tile + frame_offset +
                              (tile_col * asset->height_tiles) + tile_row;

                // Word 0: tile number
                NG_REG_VRAMDATA = tile_idx & 0xFFFF;

                // Word 1: attributes (palette, flip flags)
                // Base h_flip=1 for correct display, XOR with user h_flip
                u16 attr = ((u16)actor->palette << 8);
                if (actor->v_flip) attr |= 0x02;
                if (!actor->h_flip) attr |= 0x01;  // Default h_flip, inverted when user flips
                NG_REG_VRAMDATA = attr;
            }

            // Clear remaining slots to prevent garbage when zoomed out
            for (u8 row = rows; row < 32; row++) {
                NG_REG_VRAMDATA = 0;  // Tile 0 (transparent)
                NG_REG_VRAMDATA = 0;  // No attributes
            }
        }

        // Update cached values
        actor->last_anim_frame = actor->anim_frame;
        actor->last_h_flip = actor->h_flip;
        actor->last_v_flip = actor->v_flip;
        actor->last_palette = actor->palette;
        actor->tiles_dirty = 0;
    }

    // === SCB2: Shrink - ONLY when zoom changed or first draw ===
    if (first_draw || zoom_changed) {
        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            NG_REG_VRAMADDR = SCB2_BASE + spr;
            NG_REG_VRAMDATA = shrink;
        }
    }

    // === SCB3/SCB4: Position - ONLY when position/zoom changed or first draw ===
    if (first_draw || zoom_changed || position_changed) {
        // Adjust height_bits for zoom level
        // At reduced zoom, shrunk graphics are shorter than the display window
        // which causes garbage (repeated last line) at the bottom.
        // Reduce height_bits proportionally to match the shrunk output height.
        u8 v_shrink = shrink & 0xFF;
        u16 adjusted_rows = ((u16)rows * v_shrink + 254) / 255;  // Ceiling division
        if (adjusted_rows < 1) adjusted_rows = 1;
        if (adjusted_rows > 32) adjusted_rows = 32;
        u8 height_bits = (u8)adjusted_rows;

        // NeoGeo Y: 496 at top of screen, decreasing goes down
        s16 y_val = 496 - screen_y;
        if (y_val < 0) y_val += 512;
        y_val &= 0x1FF;

        for (u8 col = 0; col < cols; col++) {
            u16 spr = first_sprite + col;
            u8 chain = (col > 0) ? 1 : 0;

            // SCB3: Y position and height
            u16 scb3 = ((u16)y_val << 7) | (chain << 6) | height_bits;
            NG_REG_VRAMADDR = SCB3_BASE + spr;
            NG_REG_VRAMDATA = scb3;

            // SCB4: X position
            s16 col_offset = (col * TILE_SIZE * zoom) >> 4;
            s16 x_pos = screen_x + col_offset;
            x_pos &= 0x1FF;

            NG_REG_VRAMADDR = SCB4_BASE + spr;
            NG_REG_VRAMDATA = (x_pos << 7);
        }

        // Update cached values
        actor->last_screen_x = screen_x;
        actor->last_screen_y = screen_y;
    }

    // Always update zoom cache and sprite allocation info
    actor->last_zoom = zoom;
    actor->hw_sprite_first = first_sprite;
    actor->hw_sprite_count = cols;
    actor->last_cols = cols;
}

void _NGActorSystemDraw(u16 first_sprite) {
    // Placeholder - individual actors are drawn via NGActorDraw()
    (void)first_sprite;
}

u8 _NGActorIsInScene(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].active && actors[handle].in_scene;
}

u8 _NGActorGetZ(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].z;
}

u8 _NGActorIsScreenSpace(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].active && actors[handle].screen_space;
}

// === Public API ===

NGActorHandle NGActorCreate(const NGVisualAsset *asset, u16 width, u16 height) {
    if (!asset) return NG_ACTOR_INVALID;

    // Find free slot
    NGActorHandle handle = NG_ACTOR_INVALID;
    for (u8 i = 0; i < NG_ACTOR_MAX; i++) {
        if (!actors[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_ACTOR_INVALID) return NG_ACTOR_INVALID;

    // Initialize actor
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
    actor->hw_sprite_first = 0xFFFF;  // Invalid - forces first draw
    actor->hw_sprite_count = 0;

    // Initialize dirty tracking - force initial draw
    actor->tiles_dirty = 1;
    actor->shrink_dirty = 1;
    actor->position_dirty = 1;
    actor->last_anim_frame = 0xFFFF;  // Invalid
    actor->last_h_flip = 0xFF;
    actor->last_v_flip = 0xFF;
    actor->last_palette = 0xFF;
    actor->last_zoom = 0xFF;
    actor->last_screen_x = 0x7FFF;    // Invalid
    actor->last_screen_y = 0x7FFF;
    actor->last_cols = 0;

    return handle;
}

void NGActorAddToScene(NGActorHandle handle, fixed x, fixed y, u8 z) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;

    actor->x = x;
    actor->y = y;
    actor->z = z;
    actor->in_scene = 1;

    // Force full redraw on scene entry
    actor->tiles_dirty = 1;
    actor->hw_sprite_first = 0xFFFF;  // Force first draw detection

    // Mark render queue for rebuild
    _NGSceneMarkRenderQueueDirty();
}

void NGActorRemoveFromScene(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;

    u8 was_in_scene = actor->in_scene;
    actor->in_scene = 0;

    // Clear hardware sprites if allocated
    if (actor->hw_sprite_count > 0) {
        for (u8 i = 0; i < actor->hw_sprite_count; i++) {
            NG_REG_VRAMADDR = SCB3_BASE + actor->hw_sprite_first + i;
            NG_REG_VRAMDATA = 0;  // Height 0 = invisible
        }
    }

    // Mark render queue for rebuild if we were in scene
    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGActorDestroy(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    NGActorRemoveFromScene(handle);
    actors[handle].active = 0;
}

// === Position Functions ===

void NGActorSetPos(NGActorHandle handle, fixed x, fixed y) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    actor->x = x;
    actor->y = y;
    // Position dirty will be detected in draw via screen coordinate comparison
}

void NGActorMove(NGActorHandle handle, fixed dx, fixed dy) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    actor->x += dx;
    actor->y += dy;
}

void NGActorSetZ(NGActorHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    if (actor->z != z) {
        actor->z = z;
        // Mark render queue for rebuild if Z changed while in scene
        if (actor->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

fixed NGActorGetX(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].x;
}

fixed NGActorGetY(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].y;
}

u8 NGActorGetZ(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    return actors[handle].z;
}

// === Animation Functions ===

void NGActorSetAnim(NGActorHandle handle, u8 anim_index) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset) return;
    if (anim_index >= actor->asset->anim_count) return;

    if (actor->anim_index != anim_index) {
        actor->anim_index = anim_index;
        actor->anim_frame = 0;
        actor->anim_counter = 0;
        actor->tiles_dirty = 1;  // Frame changed, need tile update
    }
}

u8 NGActorSetAnimByName(NGActorHandle handle, const char *name) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims) return 0;

    for (u8 i = 0; i < actor->asset->anim_count; i++) {
        if (str_equal(actor->asset->anims[i].name, name)) {
            NGActorSetAnim(handle, i);
            return 1;
        }
    }
    return 0;
}

void NGActorSetFrame(NGActorHandle handle, u16 frame) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset) return;
    if (frame >= actor->asset->frame_count) return;

    if (actor->anim_frame != frame) {
        actor->anim_frame = frame;
        actor->anim_counter = 0;
        actor->tiles_dirty = 1;  // Frame changed
    }
}

u8 NGActorAnimDone(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 1;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset || !actor->asset->anims) return 1;
    if (actor->anim_index >= actor->asset->anim_count) return 1;

    const NGAnimDef *anim = &actor->asset->anims[actor->anim_index];
    if (anim->loop) return 0;  // Looping anims never done
    return (actor->anim_frame >= anim->frame_count - 1);
}

// === Appearance Functions ===

void NGActorSetVisible(NGActorHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    actor->visible = visible ? 1 : 0;
}

void NGActorSetPalette(NGActorHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    if (actor->palette != palette) {
        actor->palette = palette;
        actor->tiles_dirty = 1;  // Palette is in tile attributes
    }
}

void NGActorSetHFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    u8 new_flip = flip ? 1 : 0;
    if (actor->h_flip != new_flip) {
        actor->h_flip = new_flip;
        actor->tiles_dirty = 1;  // Flip affects tile order and attributes
    }
}

void NGActorSetVFlip(NGActorHandle handle, u8 flip) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    u8 new_flip = flip ? 1 : 0;
    if (actor->v_flip != new_flip) {
        actor->v_flip = new_flip;
        actor->tiles_dirty = 1;  // Flip affects tile order and attributes
    }
}

void NGActorSetScreenSpace(NGActorHandle handle, u8 enabled) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active) return;
    u8 new_val = enabled ? 1 : 0;
    if (actor->screen_space != new_val) {
        actor->screen_space = new_val;
        // Force redraw since zoom/position calculation changes
        actor->shrink_dirty = 1;
        actor->position_dirty = 1;
    }
}

// === Scene Integration (called by scene.c for per-actor draw) ===

void NGActorDraw(NGActorHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->in_scene) return;

    draw_actor(actor, first_sprite);
}

u8 NGActorGetSpriteCount(NGActorHandle handle) {
    if (handle < 0 || handle >= NG_ACTOR_MAX) return 0;
    Actor *actor = &actors[handle];
    if (!actor->active || !actor->asset) return 0;

    u16 disp_w = actor->width ? actor->width : actor->asset->width_pixels;
    return (disp_w + TILE_SIZE - 1) / TILE_SIZE;
}
