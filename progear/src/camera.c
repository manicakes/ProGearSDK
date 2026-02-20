/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <camera.h>
#include <actor.h>

// Zoom index 0-128 maps to zoom 8-16 (50%-100%). Eliminates fixed-point math.
// Each entry is (h_shrink << 8) | v_shrink
#define ZOOM_INDEX_MIN 0
#define ZOOM_INDEX_MAX 128

static const u16 zoom_shrink_table[129] = {
    0x077F, 0x0780, 0x0781, 0x0782, 0x0783, 0x0784, 0x0785, 0x0786, 0x0787, 0x0788, 0x0789, 0x078A,
    0x078B, 0x078C, 0x078D, 0x078E,
    // Index 16-31: h_shrink=8, v_shrink=143-158
    0x088F, 0x0890, 0x0891, 0x0892, 0x0893, 0x0894, 0x0895, 0x0896, 0x0897, 0x0898, 0x0899, 0x089A,
    0x089B, 0x089C, 0x089D, 0x089E, 0x099F, 0x09A0, 0x09A1, 0x09A2, 0x09A3, 0x09A4, 0x09A5, 0x09A6,
    0x09A7, 0x09A8, 0x09A9, 0x09AA, 0x09AB, 0x09AC, 0x09AD, 0x09AE, 0x0AAF, 0x0AB0, 0x0AB1, 0x0AB2,
    0x0AB3, 0x0AB4, 0x0AB5, 0x0AB6, 0x0AB7, 0x0AB8, 0x0AB9, 0x0ABA, 0x0ABB, 0x0ABC, 0x0ABD, 0x0ABE,
    0x0BBF, 0x0BC0, 0x0BC1, 0x0BC2, 0x0BC3, 0x0BC4, 0x0BC5, 0x0BC6, 0x0BC7, 0x0BC8, 0x0BC9, 0x0BCA,
    0x0BCB, 0x0BCC, 0x0BCD, 0x0BCE, 0x0CCF, 0x0CD0, 0x0CD1, 0x0CD2, 0x0CD3, 0x0CD4, 0x0CD5, 0x0CD6,
    0x0CD7, 0x0CD8, 0x0CD9, 0x0CDA, 0x0CDB, 0x0CDC, 0x0CDD, 0x0CDE, 0x0DDF, 0x0DE0, 0x0DE1, 0x0DE2,
    0x0DE3, 0x0DE4, 0x0DE5, 0x0DE6, 0x0DE7, 0x0DE8, 0x0DE9, 0x0DEA, 0x0DEB, 0x0DEC, 0x0DED, 0x0DEE,
    0x0EEF, 0x0EF0, 0x0EF1, 0x0EF2, 0x0EF3, 0x0EF4, 0x0EF5, 0x0EF6, 0x0EF7, 0x0EF8, 0x0EF9, 0x0EFA,
    0x0EFB, 0x0EFC, 0x0EFD, 0x0EFE, 0x0FFF};

static struct {
    fixed x;
    fixed y;
    struct {
        u8 index;
        u8 target;
        u8 step;
    } zoom;
    struct {
        u8 intensity;
        u8 duration;
        u8 timer;
        s8 offset_x;
        s8 offset_y;
        u16 rand_state;
    } shake;
    struct {
        NGActorHandle actor;
        u16 deadzone_w;
        u16 deadzone_h;
        fixed follow_speed;
        u16 bounds_w;
        u16 bounds_h;
        s16 offset_x;
        s16 offset_y;
    } track;
} camera = {
    .shake.rand_state = 0x1234,
    .track.actor = NG_ACTOR_INVALID,
    .track.deadzone_w = 64,
    .track.deadzone_h = 32,
    .track.follow_speed = FIX(0.15),
};

static inline u8 zoom_to_index(u8 zoom) {
    if (zoom < 8)
        zoom = 8;
    if (zoom > 16)
        zoom = 16;
    return (u8)((zoom - 8) << 4);
}

static inline u8 index_to_zoom(u8 index) {
    return 8 + (index >> 4);
}

// Forward declarations
fixed NGCameraGetRenderX(void);
fixed NGCameraGetRenderY(void);

void NGCameraInit(void) {
    camera.x = 0;
    camera.y = 0;
    camera.zoom.index = ZOOM_INDEX_MAX;
    camera.zoom.target = ZOOM_INDEX_MAX;
    camera.zoom.step = 16;
}

void NGCameraSetPos(fixed x, fixed y) {
    camera.x = x;
    camera.y = y;
}

void NGCameraMove(fixed dx, fixed dy) {
    camera.x += dx;
    camera.y += dy;
}

fixed NGCameraGetX(void) {
    return camera.x;
}

fixed NGCameraGetY(void) {
    return camera.y;
}

void NGCameraSetZoom(u8 zoom) {
    if (zoom > NG_CAM_ZOOM_100)
        zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_75)
        zoom = NG_CAM_ZOOM_75;
    u8 idx = zoom_to_index(zoom);
    camera.zoom.index = idx;
    camera.zoom.target = idx;
}

void NGCameraSetTargetZoom(u8 zoom) {
    if (zoom > NG_CAM_ZOOM_100)
        zoom = NG_CAM_ZOOM_100;
    if (zoom < NG_CAM_ZOOM_75)
        zoom = NG_CAM_ZOOM_75;
    camera.zoom.target = zoom_to_index(zoom);
}

void NGCameraSetZoomSpeed(fixed speed) {
    u8 step = (u8)(speed >> 14);
    if (step < 1)
        step = 1;
    if (step > 32)
        step = 32;
    camera.zoom.step = step;
}

u8 NGCameraGetZoom(void) {
    return index_to_zoom(camera.zoom.index);
}

u8 NGCameraIsZooming(void) {
    return camera.zoom.index != camera.zoom.target ? 1 : 0;
}

u8 NGCameraGetTargetZoom(void) {
    return index_to_zoom(camera.zoom.target);
}

u16 NGCameraGetShrink(void) {
    return zoom_shrink_table[camera.zoom.index];
}

static void update_shake(void);
static void update_tracking(void);

void NGCameraUpdate(void) {
    if (camera.zoom.index != camera.zoom.target) {
        if (camera.zoom.index < camera.zoom.target) {
            camera.zoom.index += camera.zoom.step;
            if (camera.zoom.index > camera.zoom.target) {
                camera.zoom.index = camera.zoom.target;
            }
        } else {
            if (camera.zoom.index >= camera.zoom.step) {
                camera.zoom.index -= camera.zoom.step;
            } else {
                camera.zoom.index = 0;
            }
            if (camera.zoom.index < camera.zoom.target) {
                camera.zoom.index = camera.zoom.target;
            }
        }
    }

    update_tracking();
    update_shake();
}

u16 NGCameraGetVisibleWidth(void) {
    u8 zoom = index_to_zoom(camera.zoom.index);
    return (SCREEN_WIDTH * 16) / zoom;
}

u16 NGCameraGetVisibleHeight(void) {
    u8 zoom = index_to_zoom(camera.zoom.index);
    return (SCREEN_HEIGHT * 16) / zoom;
}

void NGCameraClampToBounds(u16 world_width, u16 world_height) {
    u16 vis_w = NGCameraGetVisibleWidth();
    u16 vis_h = NGCameraGetVisibleHeight();

    if (world_height > 512) {
        world_height = 512;
    }

    if (camera.x < 0) {
        camera.x = 0;
    }

    s32 max_x = (s32)world_width - (s32)vis_w;
    if (max_x < 0)
        max_x = 0;

    if (camera.x > FIX(max_x)) {
        camera.x = FIX(max_x);
    }

    if (camera.y < 0) {
        camera.y = 0;
    }

    s32 max_y = (s32)world_height - (s32)vis_h;
    if (max_y < 0)
        max_y = 0;

    if (camera.y > FIX(max_y)) {
        camera.y = FIX(max_y);
    }
}

void NGCameraWorldToScreen(fixed world_x, fixed world_y, s16 *screen_x, s16 *screen_y) {
    fixed rel_x = world_x - NGCameraGetRenderX();
    fixed rel_y = world_y - NGCameraGetRenderY();

    s32 zoom = index_to_zoom(camera.zoom.index);
    s32 scaled_x = (FIX_INT(rel_x) * zoom) >> 4;
    s32 scaled_y = (FIX_INT(rel_y) * zoom) >> 4;

    *screen_x = (s16)scaled_x;
    *screen_y = (s16)scaled_y;
}

void NGCameraScreenToWorld(s16 screen_x, s16 screen_y, fixed *world_x, fixed *world_y) {
    s32 zoom = index_to_zoom(camera.zoom.index);
    s32 unscaled_x = ((s32)screen_x << 4) / zoom;
    s32 unscaled_y = ((s32)screen_y << 4) / zoom;

    *world_x = FIX(unscaled_x) + camera.x;
    *world_y = FIX(unscaled_y) + camera.y;
}

static s8 shake_random(void) {
    camera.shake.rand_state = (u16)(camera.shake.rand_state * 1103515245 + 12345);
    return (s8)((camera.shake.rand_state >> 8) & 0xFF);
}

void NGCameraShake(u8 intensity, u8 duration) {
    camera.shake.intensity = intensity;
    camera.shake.duration = duration;
    camera.shake.timer = duration;
}

u8 NGCameraIsShaking(void) {
    return camera.shake.timer > 0 ? 1 : 0;
}

void NGCameraShakeStop(void) {
    camera.shake.timer = 0;
    camera.shake.offset_x = 0;
    camera.shake.offset_y = 0;
}

static void update_shake(void) {
    if (camera.shake.timer > 0) {
        camera.shake.timer--;

        s32 current_intensity =
            (camera.shake.intensity * camera.shake.timer) / camera.shake.duration;
        if (current_intensity < 1 && camera.shake.timer > 0)
            current_intensity = 1;

        camera.shake.offset_x =
            (s8)((shake_random() % (current_intensity * 2 + 1)) - current_intensity);
        camera.shake.offset_y =
            (s8)((shake_random() % (current_intensity * 2 + 1)) - current_intensity);
    } else {
        camera.shake.offset_x = 0;
        camera.shake.offset_y = 0;
    }
}

fixed NGCameraGetRenderX(void) {
    return camera.x + FIX(camera.shake.offset_x);
}

fixed NGCameraGetRenderY(void) {
    return camera.y + FIX(camera.shake.offset_y);
}

static void update_tracking(void) {
    if (camera.track.actor == NG_ACTOR_INVALID)
        return;

    fixed actor_x = NGActorGetX(camera.track.actor);
    fixed actor_y = NGActorGetY(camera.track.actor);

    u16 vis_w = NGCameraGetVisibleWidth();
    u16 vis_h = NGCameraGetVisibleHeight();

    fixed cam_center_x = camera.x + FIX(vis_w / 2);
    fixed cam_center_y = camera.y + FIX(vis_h / 2);

    fixed dist_x = actor_x + FIX(camera.track.offset_x) - cam_center_x;
    fixed dist_y = actor_y + FIX(camera.track.offset_y) - cam_center_y;

    fixed deadzone_half_w = FIX(camera.track.deadzone_w / 2);
    fixed deadzone_half_h = FIX(camera.track.deadzone_h / 2);

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

    camera.x += FIX_MUL(move_x, camera.track.follow_speed);
    camera.y += FIX_MUL(move_y, camera.track.follow_speed);

    if (camera.track.bounds_w > 0 || camera.track.bounds_h > 0) {
        NGCameraClampToBounds(camera.track.bounds_w, camera.track.bounds_h);
    }
}

void NGCameraTrackActor(NGActorHandle actor) {
    camera.track.actor = actor;
}

void NGCameraStopTracking(void) {
    camera.track.actor = NG_ACTOR_INVALID;
}

void NGCameraSetDeadzone(u16 width, u16 height) {
    camera.track.deadzone_w = width;
    camera.track.deadzone_h = height;
}

void NGCameraSetFollowSpeed(fixed speed) {
    camera.track.follow_speed = speed;
}

void NGCameraSetBounds(u16 world_width, u16 world_height) {
    camera.track.bounds_w = world_width;
    camera.track.bounds_h = world_height;
}

void NGCameraSetTrackOffset(s16 offset_x, s16 offset_y) {
    camera.track.offset_x = offset_x;
    camera.track.offset_y = offset_y;
}
