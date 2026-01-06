/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <camera.h>
#include <actor.h>

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  224

// Zoom index 0-128 maps to zoom 8-16 (50%-100%). Eliminates fixed-point math.
// Each entry is (h_shrink << 8) | v_shrink
#define ZOOM_INDEX_MIN  0
#define ZOOM_INDEX_MAX  128

static const u16 zoom_shrink_table[129] = {
    0x077F, 0x0780, 0x0781, 0x0782, 0x0783, 0x0784, 0x0785, 0x0786,
    0x0787, 0x0788, 0x0789, 0x078A, 0x078B, 0x078C, 0x078D, 0x078E,
    // Index 16-31: h_shrink=8, v_shrink=143-158
    0x088F, 0x0890, 0x0891, 0x0892, 0x0893, 0x0894, 0x0895, 0x0896,
    0x0897, 0x0898, 0x0899, 0x089A, 0x089B, 0x089C, 0x089D, 0x089E,
    0x099F, 0x09A0, 0x09A1, 0x09A2, 0x09A3, 0x09A4, 0x09A5, 0x09A6,
    0x09A7, 0x09A8, 0x09A9, 0x09AA, 0x09AB, 0x09AC, 0x09AD, 0x09AE,
    0x0AAF, 0x0AB0, 0x0AB1, 0x0AB2, 0x0AB3, 0x0AB4, 0x0AB5, 0x0AB6,
    0x0AB7, 0x0AB8, 0x0AB9, 0x0ABA, 0x0ABB, 0x0ABC, 0x0ABD, 0x0ABE,
    0x0BBF, 0x0BC0, 0x0BC1, 0x0BC2, 0x0BC3, 0x0BC4, 0x0BC5, 0x0BC6,
    0x0BC7, 0x0BC8, 0x0BC9, 0x0BCA, 0x0BCB, 0x0BCC, 0x0BCD, 0x0BCE,
    0x0CCF, 0x0CD0, 0x0CD1, 0x0CD2, 0x0CD3, 0x0CD4, 0x0CD5, 0x0CD6,
    0x0CD7, 0x0CD8, 0x0CD9, 0x0CDA, 0x0CDB, 0x0CDC, 0x0CDD, 0x0CDE,
    0x0DDF, 0x0DE0, 0x0DE1, 0x0DE2, 0x0DE3, 0x0DE4, 0x0DE5, 0x0DE6,
    0x0DE7, 0x0DE8, 0x0DE9, 0x0DEA, 0x0DEB, 0x0DEC, 0x0DED, 0x0DEE,
    0x0EEF, 0x0EF0, 0x0EF1, 0x0EF2, 0x0EF3, 0x0EF4, 0x0EF5, 0x0EF6,
    0x0EF7, 0x0EF8, 0x0EF9, 0x0EFA, 0x0EFB, 0x0EFC, 0x0EFD, 0x0EFE,
    0x0FFF
};

static fixed camera_x;
static fixed camera_y;
static u8 camera_zoom_index;
static u8 camera_zoom_target;
static u8 camera_zoom_step;

static u8 shake_intensity;
static u8 shake_duration;
static u8 shake_timer;
static s8 shake_offset_x;
static s8 shake_offset_y;
static u16 shake_rand_state = 0x1234;

static NGActorHandle track_actor = NG_ACTOR_INVALID;
static u16 track_deadzone_w = 64;
static u16 track_deadzone_h = 32;
static fixed track_follow_speed = FIX_FROM_FLOAT(0.15);
static u16 track_bounds_w = 0;
static u16 track_bounds_h = 0;
static s16 track_offset_x = 0;
static s16 track_offset_y = 0;

static inline u8 zoom_to_index(u8 zoom) {
    if (zoom < 8) zoom = 8;
    if (zoom > 16) zoom = 16;
    return (zoom - 8) << 4;
}

static inline u8 index_to_zoom(u8 index) {
    return 8 + (index >> 4);
}

void NGCameraInit(void) {
    camera_x = 0;
    camera_y = 0;
    camera_zoom_index = ZOOM_INDEX_MAX;
    camera_zoom_target = ZOOM_INDEX_MAX;
    camera_zoom_step = 16;
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
    if (zoom > NG_CAM_ZOOM_100) zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_50) zoom = NG_CAM_ZOOM_50;
    u8 idx = zoom_to_index(zoom);
    camera_zoom_index = idx;
    camera_zoom_target = idx;
}

void NGCameraSetTargetZoom(u8 zoom) {
    if (zoom > NG_CAM_ZOOM_100) zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_50) zoom = NG_CAM_ZOOM_50;
    camera_zoom_target = zoom_to_index(zoom);
}

void NGCameraSetZoomSpeed(fixed speed) {
    u8 step = (u8)(speed >> 14);
    if (step < 1) step = 1;
    if (step > 32) step = 32;
    camera_zoom_step = step;
}

u8 NGCameraGetZoom(void) {
    return index_to_zoom(camera_zoom_index);
}

fixed NGCameraGetZoomFixed(void) {
    return FIX(8) + ((fixed)camera_zoom_index << 12);
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

static void update_shake(void);
static void update_tracking(void);

void NGCameraUpdate(void) {
    if (camera_zoom_index != camera_zoom_target) {
        if (camera_zoom_index < camera_zoom_target) {
            camera_zoom_index += camera_zoom_step;
            if (camera_zoom_index > camera_zoom_target) {
                camera_zoom_index = camera_zoom_target;
            }
        } else {
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

    update_tracking();
    update_shake();
}

u16 NGCameraGetVisibleWidth(void) {
    u8 zoom = index_to_zoom(camera_zoom_index);
    return (SCREEN_WIDTH * 16) / zoom;
}

u16 NGCameraGetVisibleHeight(void) {
    u8 zoom = index_to_zoom(camera_zoom_index);
    return (SCREEN_HEIGHT * 16) / zoom;
}

void NGCameraClampToBounds(u16 world_width, u16 world_height) {
    u16 vis_w = NGCameraGetVisibleWidth();
    u16 vis_h = NGCameraGetVisibleHeight();

    if (world_height > 512) {
        world_height = 512;
    }

    if (camera_x < 0) {
        camera_x = 0;
    }

    s32 max_x = (s32)world_width - (s32)vis_w;
    if (max_x < 0) max_x = 0;

    if (camera_x > FIX(max_x)) {
        camera_x = FIX(max_x);
    }

    if (camera_y < 0) {
        camera_y = 0;
    }

    s32 max_y = (s32)world_height - (s32)vis_h;
    if (max_y < 0) max_y = 0;

    if (camera_y > FIX(max_y)) {
        camera_y = FIX(max_y);
    }
}

void NGCameraWorldToScreen(fixed world_x, fixed world_y,
                           s16 *screen_x, s16 *screen_y) {
    fixed cam_render_x = camera_x + FIX(shake_offset_x);
    fixed cam_render_y = camera_y + FIX(shake_offset_y);

    fixed rel_x = world_x - cam_render_x;
    fixed rel_y = world_y - cam_render_y;

    s32 zoom = index_to_zoom(camera_zoom_index);
    s32 scaled_x = (FIX_INT(rel_x) * zoom) >> 4;
    s32 scaled_y = (FIX_INT(rel_y) * zoom) >> 4;

    *screen_x = (s16)scaled_x;
    *screen_y = (s16)scaled_y;
}

void NGCameraScreenToWorld(s16 screen_x, s16 screen_y,
                           fixed *world_x, fixed *world_y) {
    s32 zoom = index_to_zoom(camera_zoom_index);
    s32 unscaled_x = ((s32)screen_x << 4) / zoom;
    s32 unscaled_y = ((s32)screen_y << 4) / zoom;

    *world_x = FIX(unscaled_x) + camera_x;
    *world_y = FIX(unscaled_y) + camera_y;
}

static s8 shake_random(void) {
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

static void update_shake(void) {
    if (shake_timer > 0) {
        shake_timer--;

        s32 current_intensity = (shake_intensity * shake_timer) / shake_duration;
        if (current_intensity < 1 && shake_timer > 0) current_intensity = 1;

        shake_offset_x = (shake_random() % (current_intensity * 2 + 1)) - current_intensity;
        shake_offset_y = (shake_random() % (current_intensity * 2 + 1)) - current_intensity;
    } else {
        shake_offset_x = 0;
        shake_offset_y = 0;
    }
}

fixed NGCameraGetRenderX(void) {
    return camera_x + FIX(shake_offset_x);
}

fixed NGCameraGetRenderY(void) {
    return camera_y + FIX(shake_offset_y);
}

static void update_tracking(void) {
    if (track_actor == NG_ACTOR_INVALID) return;

    fixed actor_x = NGActorGetX(track_actor);
    fixed actor_y = NGActorGetY(track_actor);

    u16 vis_w = NGCameraGetVisibleWidth();
    u16 vis_h = NGCameraGetVisibleHeight();

    fixed cam_center_x = camera_x + FIX(vis_w / 2);
    fixed cam_center_y = camera_y + FIX(vis_h / 2);

    fixed dist_x = actor_x + FIX(track_offset_x) - cam_center_x;
    fixed dist_y = actor_y + FIX(track_offset_y) - cam_center_y;

    fixed deadzone_half_w = FIX(track_deadzone_w / 2);
    fixed deadzone_half_h = FIX(track_deadzone_h / 2);

    fixed move_x = 0;
    fixed move_y = 0;

    if (dist_x > deadzone_half_w) {
        move_x = dist_x - deadzone_half_w;
    } else if (dist_x < -deadzone_half_w) {
        move_x = dist_x + deadzone_half_w;
    }

    if (dist_y > deadzone_half_h) {
        move_y = dist_y - deadzone_half_h;
    } else if (dist_y < -deadzone_half_h) {
        move_y = dist_y + deadzone_half_h;
    }

    camera_x += FIX_MUL(move_x, track_follow_speed);
    camera_y += FIX_MUL(move_y, track_follow_speed);

    if (track_bounds_w > 0 || track_bounds_h > 0) {
        NGCameraClampToBounds(track_bounds_w, track_bounds_h);
    }
}

void NGCameraTrackActor(NGActorHandle actor) {
    track_actor = actor;
}

void NGCameraStopTracking(void) {
    track_actor = NG_ACTOR_INVALID;
}

void NGCameraSetDeadzone(u16 width, u16 height) {
    track_deadzone_w = width;
    track_deadzone_h = height;
}

void NGCameraSetFollowSpeed(fixed speed) {
    track_follow_speed = speed;
}

void NGCameraSetBounds(u16 world_width, u16 world_height) {
    track_bounds_w = world_width;
    track_bounds_h = world_height;
}

void NGCameraSetTrackOffset(s16 offset_x, s16 offset_y) {
    track_offset_x = offset_x;
    track_offset_y = offset_y;
}
