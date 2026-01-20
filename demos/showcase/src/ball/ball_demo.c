/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "ball_demo.h"
#include "ball.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <scene.h>
#include <backdrop.h>
#include <actor.h>
#include <input.h>
#include <camera.h>
#include <arena.h>
#include <ui.h>
#include <audio.h>
#include <palette.h>
#include <color.h>
#include <ngmath.h>
#include <engine.h>
#include <lighting.h>
#include <progear_assets.h>

#define CAM_CIRCLE_SPEED   1
#define CAM_DEFAULT_RADIUS FIX(24)

/* Night mode timing (at 60fps) */
#define NIGHT_MODE_CYCLE_FRAMES (10 * 60) /* 10 seconds per day/night cycle */
#define NIGHT_MODE_DURATION     (5 * 60)  /* Night lasts 5 seconds */
#define NIGHT_TRANSITION_FRAMES 60        /* 1 second fade in/out */
#define LIGHTNING_MIN_INTERVAL  30        /* Minimum frames between strikes */
#define LIGHTNING_MAX_INTERVAL  90        /* Maximum frames between strikes */

typedef struct BallDemoState {
    Actor brick;
    Backdrop brick_pattern;
    Backdrop brick_shadow;
    BallSystemHandle balls;
    MenuHandle menu;
    angle_t cam_angle;
    fixed cam_circle_radius;
    u8 menu_open;
    u8 switch_target;

    /* Night mode state */
    u16 day_night_timer;
    u8 is_night;
    LightingLayerHandle night_preset;
    u16 lightning_timer;
    u16 rng_state;
} BallDemoState;

static BallDemoState *state;

#define MENU_RESUME       0
#define MENU_ADD_BALL     1
#define MENU_CLEAR_BALLS  2
#define MENU_TOGGLE_ZOOM  3
#define MENU_TOGGLE_MUSIC 4
// Index 5 is separator
#define MENU_SCROLL_DEMO  6
#define MENU_BLANK_SCENE  7
#define MENU_TILEMAP_DEMO 8

/* Simple pseudo-random number generator */
static u16 ball_demo_rand(void) {
    state->rng_state = (u16)(state->rng_state * 25173 + 13849);
    return state->rng_state;
}

static u16 ball_demo_rand_range(u16 min, u16 max) {
    return (u16)(min + (ball_demo_rand() % (max - min + 1)));
}

static void update_lightning(void) {
    if (state->lightning_timer > 0) {
        state->lightning_timer--;
    }
    if (state->lightning_timer == 0) {
        u8 flash_type = (u8)(ball_demo_rand() % 3);
        if (flash_type == 0) {
            LightingFlash(25, 25, 30, 4);
        } else if (flash_type == 1) {
            LightingFlash(20, 20, 25, 3);
            LightingFlash(30, 30, 35, 6);
        } else {
            LightingFlash(12, 12, 18, 8);
        }
        state->lightning_timer =
            ball_demo_rand_range(LIGHTNING_MIN_INTERVAL, LIGHTNING_MAX_INTERVAL);
    }
}

static void update_day_night_cycle(void) {
    state->day_night_timer++;

    /* Update pre-baked fade animation */
    LightingUpdatePrebakedFade();

    /* Check if fade-out completed (transitioning back to day) */
    if (state->is_night && state->night_preset == LIGHTING_INVALID) {
        /* Fade completed, now fully day */
        state->is_night = 0;
    }

    /* Transition to night */
    if (!state->is_night && !LightingIsPrebakedFading() &&
        state->day_night_timer >= NIGHT_MODE_CYCLE_FRAMES) {
        state->is_night = 1;
        state->day_night_timer = 0;
        /* Use pre-baked night preset - zero CPU palette transforms! */
        state->night_preset = LightingPushPreset(LIGHTING_PREBAKED_NIGHT, NIGHT_TRANSITION_FRAMES);
        state->lightning_timer =
            ball_demo_rand_range(LIGHTNING_MIN_INTERVAL, LIGHTNING_MAX_INTERVAL);
        BallSystemSetGravity(state->balls, FIX(-1));
    }
    /* Transition to day */
    else if (state->is_night && !LightingIsPrebakedFading() &&
             state->day_night_timer >= NIGHT_MODE_DURATION) {
        state->day_night_timer = 0;
        /* Pop preset with fade - animates back to original palettes */
        LightingPopPreset(state->night_preset, NIGHT_TRANSITION_FRAMES);
        state->night_preset = LIGHTING_INVALID;
        BallSystemSetGravity(state->balls, FIX(1));
    }

    if (state->is_night && !LightingIsPrebakedFading()) {
        update_lightning();
    }
}

void BallDemoInit(void) {
    state = ARENA_ALLOC(&arena_state, BallDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->cam_angle = 0;
    state->cam_circle_radius = CAM_DEFAULT_RADIUS;

    /* Initialize night mode state */
    state->day_night_timer = 0;
    state->is_night = 0;
    state->night_preset = LIGHTING_INVALID;
    state->lightning_timer = 0;
    state->rng_state = 12345;

    PalSetBackdrop(COLOR_BLACK);

    // Match brick asset size to avoid sprite limits
    state->brick_pattern = BackdropCreate(&VisualAsset_brick_pattern, 336, 256, FIX_FROM_FLOAT(0.8),
                                          FIX_FROM_FLOAT(0.8));
    BackdropAddToScene(state->brick_pattern, 0, 0, 4);

    // Shadow moves slower than camera for depth effect
    state->brick_shadow = BackdropCreate(
        &VisualAsset_brick_shadow, VisualAsset_brick_shadow.width_pixels,
        VisualAsset_brick_shadow.height_pixels, FIX_FROM_FLOAT(0.9), FIX_FROM_FLOAT(0.9));
    BackdropAddToScene(state->brick_shadow, 8, 8, 5);

    state->brick = ActorCreate(&VisualAsset_brick);
    ActorAddToScene(state->brick, FIX(0), FIX(0), 10);

    state->balls = BallSystemCreate(&arena_state, 8);
    BallSpawn(state->balls);
    BallSpawn(state->balls);

    state->menu = MenuCreateDefault(&arena_state, 10);
    MenuSetTitle(state->menu, "BALL DEMO");
    MenuAddItem(state->menu, "Resume");
    MenuAddItem(state->menu, "Add Ball");
    MenuAddItem(state->menu, "Clear Balls");
    MenuAddItem(state->menu, "Toggle Zoom");
    MenuAddItem(state->menu, "Pause Music");
    MenuAddSeparator(state->menu, "--------");
    MenuAddItem(state->menu, "Scroll Demo");
    MenuAddItem(state->menu, "Blank Scene");
    MenuAddItem(state->menu, "Tilemap Demo");
    MenuSetDefaultSounds(state->menu);
    EngineSetActiveMenu(state->menu);

    TextPrint(FixLayoutAlign(ALIGN_CENTER, ALIGN_TOP), 0, "PRESS START FOR MENU");

    AudioPlayMusic(MUSIC_BALL_SCENE_MUSIC);
}

u8 BallDemoUpdate(void) {
    if (InputPressed(PLAYER_1, BUTTON_START)) {
        if (state->menu_open) {
            MenuHide(state->menu);
            state->menu_open = 0;
        } else {
            MenuShow(state->menu);
            state->menu_open = 1;
        }
    }

    MenuUpdate(state->menu);

    if (state->menu_open) {
        if (MenuConfirmed(state->menu)) {
            switch (MenuGetSelection(state->menu)) {
                case MENU_RESUME:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    break;
                case MENU_ADD_BALL:
                    BallSpawn(state->balls);
                    break;
                case MENU_CLEAR_BALLS:
                    while (BallDestroyLast(state->balls)) {}
                    break;
                case MENU_TOGGLE_ZOOM: {
                    u8 target = CameraGetTargetZoom();
                    if (target == CAM_ZOOM_100) {
                        CameraSetTargetZoom(CAM_ZOOM_75);
                    } else {
                        CameraSetTargetZoom(CAM_ZOOM_100);
                    }
                } break;
                case MENU_TOGGLE_MUSIC:
                    if (AudioMusicPaused()) {
                        AudioResumeMusic();
                        MenuSetItemText(state->menu, MENU_TOGGLE_MUSIC, "Pause Music");
                    } else {
                        AudioPauseMusic();
                        MenuSetItemText(state->menu, MENU_TOGGLE_MUSIC, "Resume Music");
                    }
                    break;
                case MENU_SCROLL_DEMO:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_SCROLL;
                    break;
                case MENU_BLANK_SCENE:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BLANK_SCENE;
                    break;
                case MENU_TILEMAP_DEMO:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_TILEMAP;
                    break;
            }
        }

        if (MenuCancelled(state->menu)) {
            MenuHide(state->menu);
            state->menu_open = 0;
        }
    }

    if (!state->menu_open) {
        u16 visible_w = CameraGetVisibleWidth();
        u16 visible_h = CameraGetVisibleHeight();
        u16 brick_w = VisualAsset_brick.width_pixels;
        u16 brick_h = VisualAsset_brick.height_pixels;

        angle_t old_angle = state->cam_angle;
        state->cam_angle += CAM_CIRCLE_SPEED;

        // Toggle zoom when completing full rotation (angle wraps 255->0)
        if (state->cam_angle < old_angle) {
            u8 target = CameraGetTargetZoom();
            if (target == CAM_ZOOM_100) {
                CameraSetTargetZoom(CAM_ZOOM_75);
            } else {
                CameraSetTargetZoom(CAM_ZOOM_100);
            }
        }

        fixed center_x = FIX(((s16)brick_w - (s16)visible_w) / 2);
        fixed center_y = FIX(((s16)brick_h - (s16)visible_h) / 2);

        fixed offset_x = FIX_MUL(Cos(state->cam_angle), state->cam_circle_radius);
        fixed offset_y = FIX_MUL(Sin(state->cam_angle), state->cam_circle_radius);

        CameraSetPos(center_x + offset_x, center_y + offset_y);
    }

    if (!state->menu_open) {
        BallSystemUpdate(state->balls);
        update_day_night_cycle();
    }

    return state->switch_target;
}

void BallDemoCleanup(void) {
    AudioStopMusic();

    /* Clean up lighting - instant pop (no fade) */
    if (state->night_preset != LIGHTING_INVALID) {
        LightingPopPreset(state->night_preset, 0);
        state->night_preset = LIGHTING_INVALID;
    }

    FixClear(0, 3, 40, 1);

    BallSystemDestroy(state->balls);

    ActorRemoveFromScene(state->brick);
    ActorDestroy(state->brick);

    BackdropRemoveFromScene(state->brick_shadow);
    BackdropDestroy(state->brick_shadow);

    BackdropRemoveFromScene(state->brick_pattern);
    BackdropDestroy(state->brick_pattern);

    MenuDestroy(state->menu);

    PalSetBackdrop(COLOR_BLACK);

    CameraSetPos(0, 0);
    CameraSetZoom(CAM_ZOOM_100);
}
