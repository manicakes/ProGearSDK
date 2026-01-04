/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// camera.c - World camera system

#include <camera.h>

// Screen dimensions
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  224

// === Zoom Lookup Table ===
//
// Zoom index 0-128 maps to zoom 8-16 (50%-100%)
// Each entry is (h_shrink << 8) | v_shrink
// - v_shrink = 127 + index (smooth 8-bit interpolation)
// - h_shrink = 7 + (index / 16) (9 discrete steps due to 4-bit limit)
//
// This eliminates all fixed-point math from zoom transitions.

#define ZOOM_INDEX_MIN  0    // zoom = 8 (50%)
#define ZOOM_INDEX_MAX  128  // zoom = 16 (100%)

// Precalculated shrink values for each zoom index
static const u16 zoom_shrink_table[129] = {
    // Index 0-15: h_shrink=7, v_shrink=127-142
    0x077F, 0x0780, 0x0781, 0x0782, 0x0783, 0x0784, 0x0785, 0x0786,
    0x0787, 0x0788, 0x0789, 0x078A, 0x078B, 0x078C, 0x078D, 0x078E,
    // Index 16-31: h_shrink=8, v_shrink=143-158
    0x088F, 0x0890, 0x0891, 0x0892, 0x0893, 0x0894, 0x0895, 0x0896,
    0x0897, 0x0898, 0x0899, 0x089A, 0x089B, 0x089C, 0x089D, 0x089E,
    // Index 32-47: h_shrink=9, v_shrink=159-174
    0x099F, 0x09A0, 0x09A1, 0x09A2, 0x09A3, 0x09A4, 0x09A5, 0x09A6,
    0x09A7, 0x09A8, 0x09A9, 0x09AA, 0x09AB, 0x09AC, 0x09AD, 0x09AE,
    // Index 48-63: h_shrink=10, v_shrink=175-190
    0x0AAF, 0x0AB0, 0x0AB1, 0x0AB2, 0x0AB3, 0x0AB4, 0x0AB5, 0x0AB6,
    0x0AB7, 0x0AB8, 0x0AB9, 0x0ABA, 0x0ABB, 0x0ABC, 0x0ABD, 0x0ABE,
    // Index 64-79: h_shrink=11, v_shrink=191-206
    0x0BBF, 0x0BC0, 0x0BC1, 0x0BC2, 0x0BC3, 0x0BC4, 0x0BC5, 0x0BC6,
    0x0BC7, 0x0BC8, 0x0BC9, 0x0BCA, 0x0BCB, 0x0BCC, 0x0BCD, 0x0BCE,
    // Index 80-95: h_shrink=12, v_shrink=207-222
    0x0CCF, 0x0CD0, 0x0CD1, 0x0CD2, 0x0CD3, 0x0CD4, 0x0CD5, 0x0CD6,
    0x0CD7, 0x0CD8, 0x0CD9, 0x0CDA, 0x0CDB, 0x0CDC, 0x0CDD, 0x0CDE,
    // Index 96-111: h_shrink=13, v_shrink=223-238
    0x0DDF, 0x0DE0, 0x0DE1, 0x0DE2, 0x0DE3, 0x0DE4, 0x0DE5, 0x0DE6,
    0x0DE7, 0x0DE8, 0x0DE9, 0x0DEA, 0x0DEB, 0x0DEC, 0x0DED, 0x0DEE,
    // Index 112-127: h_shrink=14, v_shrink=239-254
    0x0EEF, 0x0EF0, 0x0EF1, 0x0EF2, 0x0EF3, 0x0EF4, 0x0EF5, 0x0EF6,
    0x0EF7, 0x0EF8, 0x0EF9, 0x0EFA, 0x0EFB, 0x0EFC, 0x0EFD, 0x0EFE,
    // Index 128: h_shrink=15, v_shrink=255
    0x0FFF
};

// === Private State ===

static fixed camera_x;
static fixed camera_y;
static u8 camera_zoom_index;    // Current zoom index (0-128)
static u8 camera_zoom_target;   // Target zoom index (0-128)
static u8 camera_zoom_step;     // Steps per frame for linear interpolation

// Shake effect state
static u8 shake_intensity;
static u8 shake_duration;
static u8 shake_timer;
static s8 shake_offset_x;
static s8 shake_offset_y;
static u16 shake_rand_state = 0x1234;

// === Helper: Convert zoom level (8-16) to index (0-128) ===
static inline u8 zoom_to_index(u8 zoom) {
    // zoom 8 -> index 0, zoom 16 -> index 128
    // Each zoom unit = 16 index steps
    if (zoom < 8) zoom = 8;
    if (zoom > 16) zoom = 16;
    return (zoom - 8) << 4;  // (zoom - 8) * 16
}

// === Helper: Convert index (0-128) to zoom level (8-16) ===
static inline u8 index_to_zoom(u8 index) {
    // index 0 -> zoom 8, index 128 -> zoom 16
    return 8 + (index >> 4);  // 8 + index / 16
}

// === System Functions ===

void NGCameraInit(void) {
    camera_x = 0;
    camera_y = 0;
    camera_zoom_index = ZOOM_INDEX_MAX;   // Start at 100% (index 128)
    camera_zoom_target = ZOOM_INDEX_MAX;
    camera_zoom_step = 16;  // Step by 16 = 8 frames for full zoom, matches h-shrink steps
}

void NGCameraSetPos(fixed x, fixed y) {
    camera_x = x;
    camera_y = y;
}

void NGCameraMove(fixed dx, fixed dy) {
    camera_x += dx;
    camera_y += dy;
}

fixed NGCameraGetX(void) {
    return camera_x;
}

fixed NGCameraGetY(void) {
    return camera_y;
}

void NGCameraSetZoom(u8 zoom) {
    // Clamp to valid range
    if (zoom > NG_CAM_ZOOM_100) zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_50) zoom = NG_CAM_ZOOM_50;
    // Instant change - set both current and target
    u8 idx = zoom_to_index(zoom);
    camera_zoom_index = idx;
    camera_zoom_target = idx;
}

void NGCameraSetTargetZoom(u8 zoom) {
    // Clamp to valid range
    if (zoom > NG_CAM_ZOOM_100) zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_50) zoom = NG_CAM_ZOOM_50;
    // Set target only - will smoothly transition
    camera_zoom_target = zoom_to_index(zoom);
}

void NGCameraSetZoomSpeed(fixed speed) {
    // Convert fixed-point speed to step count
    // Higher speed = more steps per frame
    // speed 0.1 -> step 1, speed 0.4 -> step 4, etc.
    u8 step = (u8)(speed >> 14);  // Convert 16.16 fixed to rough step
    if (step < 1) step = 1;
    if (step > 32) step = 32;
    camera_zoom_step = step;
}

u8 NGCameraGetZoom(void) {
    return index_to_zoom(camera_zoom_index);
}

fixed NGCameraGetZoomFixed(void) {
    // Convert index to fixed-point zoom for compatibility
    // index 0 = FIX(8), index 128 = FIX(16)
    return FIX(8) + ((fixed)camera_zoom_index << 12);  // index * 0.5 in fixed
}

u8 NGCameraIsZooming(void) {
    return camera_zoom_index != camera_zoom_target ? 1 : 0;
}

u8 NGCameraGetTargetZoom(void) {
    return index_to_zoom(camera_zoom_target);
}

u16 NGCameraGetShrink(void) {
    return zoom_shrink_table[camera_zoom_index];
}

// Forward declaration
static void update_shake(void);

void NGCameraUpdate(void) {
    // Linear interpolation towards target (no fixed-point math!)
    if (camera_zoom_index != camera_zoom_target) {
        if (camera_zoom_index < camera_zoom_target) {
            // Zooming in (index increasing)
            camera_zoom_index += camera_zoom_step;
            if (camera_zoom_index > camera_zoom_target) {
                camera_zoom_index = camera_zoom_target;
            }
        } else {
            // Zooming out (index decreasing)
            if (camera_zoom_index >= camera_zoom_step) {
                camera_zoom_index -= camera_zoom_step;
            } else {
                camera_zoom_index = 0;
            }
            if (camera_zoom_index < camera_zoom_target) {
                camera_zoom_index = camera_zoom_target;
            }
        }
    }

    // Update shake effect
    update_shake();
}

// === Utilities ===

u16 NGCameraGetVisibleWidth(void) {
    // At zoom Z, visible width = screen_width * 16 / Z
    u8 zoom = index_to_zoom(camera_zoom_index);
    return (SCREEN_WIDTH * 16) / zoom;
}

u16 NGCameraGetVisibleHeight(void) {
    // At zoom Z, visible height = screen_height * 16 / Z
    u8 zoom = index_to_zoom(camera_zoom_index);
    return (SCREEN_HEIGHT * 16) / zoom;
}

void NGCameraClampToBounds(u16 world_width, u16 world_height) {
    u16 vis_w = NGCameraGetVisibleWidth();
    u16 vis_h = NGCameraGetVisibleHeight();

    // Enforce max world height for smooth Y scrolling (512 pixel limit)
    if (world_height > 512) {
        world_height = 512;
    }

    // Camera position is top-left of view
    // Clamp X so we don't see past world edges
    if (camera_x < 0) {
        camera_x = 0;
    }

    // Max X is where camera right edge hits world right edge
    s32 max_x = (s32)world_width - (s32)vis_w;
    if (max_x < 0) max_x = 0;  // World smaller than screen

    if (camera_x > FIX(max_x)) {
        camera_x = FIX(max_x);
    }

    // Clamp Y so we don't see past world edges
    if (camera_y < 0) {
        camera_y = 0;
    }

    // Max Y is where camera bottom edge hits world bottom edge
    s32 max_y = (s32)world_height - (s32)vis_h;
    if (max_y < 0) max_y = 0;  // World smaller than screen

    if (camera_y > FIX(max_y)) {
        camera_y = FIX(max_y);
    }
}

void NGCameraWorldToScreen(fixed world_x, fixed world_y,
                           s16 *screen_x, s16 *screen_y) {
    // Camera position is top-left of view, including shake offset
    // screen_pos = (world_pos - camera_pos) * zoom / 16
    //
    // At 100% zoom (16): screen = world - camera
    // At 50% zoom (8):   screen = (world - camera) * 0.5

    // Include shake offset in camera position
    fixed cam_render_x = camera_x + FIX(shake_offset_x);
    fixed cam_render_y = camera_y + FIX(shake_offset_y);

    fixed rel_x = world_x - cam_render_x;
    fixed rel_y = world_y - cam_render_y;

    // Get zoom from index (integer 8-16)
    s32 zoom = index_to_zoom(camera_zoom_index);
    s32 scaled_x = (FIX_INT(rel_x) * zoom) >> 4;
    s32 scaled_y = (FIX_INT(rel_y) * zoom) >> 4;

    *screen_x = (s16)scaled_x;
    *screen_y = (s16)scaled_y;
}

void NGCameraScreenToWorld(s16 screen_x, s16 screen_y,
                           fixed *world_x, fixed *world_y) {
    // Inverse of WorldToScreen:
    // world = screen * 16 / zoom + camera

    // Scale by inverse zoom: multiply by 16, divide by zoom
    s32 zoom = index_to_zoom(camera_zoom_index);
    s32 unscaled_x = ((s32)screen_x << 4) / zoom;
    s32 unscaled_y = ((s32)screen_y << 4) / zoom;

    *world_x = FIX(unscaled_x) + camera_x;
    *world_y = FIX(unscaled_y) + camera_y;
}

// === Shake Effect ===

static s8 shake_random(void) {
    // Simple linear congruential generator
    shake_rand_state = shake_rand_state * 1103515245 + 12345;
    return (s8)((shake_rand_state >> 8) & 0xFF);
}

void NGCameraShake(u8 intensity, u8 duration) {
    shake_intensity = intensity;
    shake_duration = duration;
    shake_timer = duration;
}

u8 NGCameraIsShaking(void) {
    return shake_timer > 0 ? 1 : 0;
}

void NGCameraShakeStop(void) {
    shake_timer = 0;
    shake_offset_x = 0;
    shake_offset_y = 0;
}

// Called during NGCameraUpdate to apply shake
static void update_shake(void) {
    if (shake_timer > 0) {
        shake_timer--;

        // Calculate shake offset (decaying over time)
        s32 current_intensity = (shake_intensity * shake_timer) / shake_duration;
        if (current_intensity < 1 && shake_timer > 0) current_intensity = 1;

        // Random offset within intensity range
        shake_offset_x = (shake_random() % (current_intensity * 2 + 1)) - current_intensity;
        shake_offset_y = (shake_random() % (current_intensity * 2 + 1)) - current_intensity;
    } else {
        shake_offset_x = 0;
        shake_offset_y = 0;
    }
}

// Get shake-adjusted camera position for rendering
fixed NGCameraGetRenderX(void) {
    return camera_x + FIX(shake_offset_x);
}

fixed NGCameraGetRenderY(void) {
    return camera_y + FIX(shake_offset_y);
}
