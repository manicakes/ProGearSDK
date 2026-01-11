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
#include <parallax.h>
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
#include <ngres_generated_assets.h>

#define CAM_CIRCLE_SPEED   1
#define CAM_DEFAULT_RADIUS FIX(24)

/* Night mode timing (at 60fps) */
#define NIGHT_MODE_CYCLE_FRAMES (10 * 60) /* 10 seconds per day/night cycle */
#define NIGHT_MODE_DURATION     (5 * 60)  /* Night lasts 5 seconds */
#define NIGHT_TRANSITION_FRAMES 60        /* 1 second fade in/out */
#define LIGHTNING_MIN_INTERVAL  30        /* Minimum frames between strikes */
#define LIGHTNING_MAX_INTERVAL  90        /* Maximum frames between strikes */

/* Night preset values (from lighting.c) */
#define NIGHT_TINT_R      (-8)
#define NIGHT_TINT_G      (-5)
#define NIGHT_TINT_B      12
#define NIGHT_BRIGHTNESS  FIX_FROM_FLOAT(0.65)

typedef struct BallDemoState {
    NGActorHandle brick;
    NGParallaxHandle brick_pattern;
    NGParallaxHandle brick_shadow;
    BallSystemHandle balls;
    NGMenuHandle menu;
    angle_t cam_angle;
    fixed cam_circle_radius;
    u8 menu_open;
    u8 switch_target;

    /* Night mode state */
    u16 day_night_timer;
    u8 is_night;
    u8 fading_to_day;
    u16 fade_timer;
    NGLightingLayerHandle night_layer;
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
            NGLightingFlash(25, 25, 30, 4);
        } else if (flash_type == 1) {
            NGLightingFlash(20, 20, 25, 3);
            NGLightingFlash(30, 30, 35, 6);
        } else {
            NGLightingFlash(12, 12, 18, 8);
        }
        state->lightning_timer =
            ball_demo_rand_range(LIGHTNING_MIN_INTERVAL, LIGHTNING_MAX_INTERVAL);
    }
}

static void update_day_night_cycle(void) {
    state->day_night_timer++;

    /* Handle fade-out completion */
    if (state->fading_to_day) {
        state->fade_timer++;
        if (state->fade_timer >= NIGHT_TRANSITION_FRAMES) {
            state->fading_to_day = 0;
            if (state->night_layer != NG_LIGHTING_INVALID_HANDLE) {
                NGLightingPop(state->night_layer);
                state->night_layer = NG_LIGHTING_INVALID_HANDLE;
            }
        }
    }

    /* Transition to night */
    if (!state->is_night && !state->fading_to_day &&
        state->day_night_timer >= NIGHT_MODE_CYCLE_FRAMES) {
        state->is_night = 1;
        state->day_night_timer = 0;
        state->night_layer = NGLightingPush(NG_LIGHTING_PRIORITY_AMBIENT);
        NGLightingFadeTint(state->night_layer, NIGHT_TINT_R, NIGHT_TINT_G, NIGHT_TINT_B,
                           NIGHT_TRANSITION_FRAMES);
        NGLightingFadeBrightness(state->night_layer, NIGHT_BRIGHTNESS, NIGHT_TRANSITION_FRAMES);
        state->lightning_timer =
            ball_demo_rand_range(LIGHTNING_MIN_INTERVAL, LIGHTNING_MAX_INTERVAL);
        BallSystemSetGravity(state->balls, FIX(-1));
    }
    /* Transition to day */
    else if (state->is_night && !state->fading_to_day &&
             state->day_night_timer >= NIGHT_MODE_DURATION) {
        state->is_night = 0;
        state->fading_to_day = 1;
        state->fade_timer = 0;
        state->day_night_timer = 0;
        if (state->night_layer != NG_LIGHTING_INVALID_HANDLE) {
            NGLightingFadeTint(state->night_layer, 0, 0, 0, NIGHT_TRANSITION_FRAMES);
            NGLightingFadeBrightness(state->night_layer, FIX_ONE, NIGHT_TRANSITION_FRAMES);
        }
        BallSystemSetGravity(state->balls, FIX(1));
    }

    if (state->is_night) {
        update_lightning();
    }
}

void BallDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, BallDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->cam_angle = 0;
    state->cam_circle_radius = CAM_DEFAULT_RADIUS;

    /* Initialize night mode state */
    state->day_night_timer = 0;
    state->is_night = 0;
    state->fading_to_day = 0;
    state->fade_timer = 0;
    state->night_layer = NG_LIGHTING_INVALID_HANDLE;
    state->lightning_timer = 0;
    state->rng_state = 12345;

    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Match brick asset size to avoid sprite limits
    state->brick_pattern = NGParallaxCreate(&NGVisualAsset_brick_pattern, 336, 256,
                                            FIX_FROM_FLOAT(0.8), FIX_FROM_FLOAT(0.8));
    NGParallaxAddToScene(state->brick_pattern, 0, 0, 4);

    // Shadow moves slower than camera for depth effect
    state->brick_shadow = NGParallaxCreate(
        &NGVisualAsset_brick_shadow, NGVisualAsset_brick_shadow.width_pixels,
        NGVisualAsset_brick_shadow.height_pixels, FIX_FROM_FLOAT(0.9), FIX_FROM_FLOAT(0.9));
    NGParallaxAddToScene(state->brick_shadow, 8, 8, 5);

    state->brick = NGActorCreate(&NGVisualAsset_brick, 0, 0);
    NGActorAddToScene(state->brick, FIX(0), FIX(0), 10);

    state->balls = BallSystemCreate(&ng_arena_state, 8);
    BallSpawn(state->balls);
    BallSpawn(state->balls);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "BALL DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Add Ball");
    NGMenuAddItem(state->menu, "Clear Balls");
    NGMenuAddItem(state->menu, "Toggle Zoom");
    NGMenuAddItem(state->menu, "Pause Music");
    NGMenuAddSeparator(state->menu, "--------");
    NGMenuAddItem(state->menu, "Scroll Demo");
    NGMenuAddItem(state->menu, "Blank Scene");
    NGMenuAddItem(state->menu, "Tilemap Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");

    NGMusicPlay(NGMUSIC_BALL_SCENE_MUSIC);
}

u8 BallDemoUpdate(void) {
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (state->menu_open) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        } else {
            NGMenuShow(state->menu);
            state->menu_open = 1;
        }
    }

    NGMenuUpdate(state->menu);

    if (state->menu_open) {
        if (NGMenuConfirmed(state->menu)) {
            switch (NGMenuGetSelection(state->menu)) {
                case MENU_RESUME:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    break;
                case MENU_ADD_BALL:
                    BallSpawn(state->balls);
                    break;
                case MENU_CLEAR_BALLS:
                    while (BallDestroyLast(state->balls)) {}
                    break;
                case MENU_TOGGLE_ZOOM: {
                    u8 target = NGCameraGetTargetZoom();
                    if (target == NG_CAM_ZOOM_100) {
                        NGCameraSetTargetZoom(NG_CAM_ZOOM_75);
                    } else {
                        NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
                    }
                } break;
                case MENU_TOGGLE_MUSIC:
                    if (NGMusicIsPaused()) {
                        NGMusicResume();
                        NGMenuSetItemText(state->menu, MENU_TOGGLE_MUSIC, "Pause Music");
                    } else {
                        NGMusicPause();
                        NGMenuSetItemText(state->menu, MENU_TOGGLE_MUSIC, "Resume Music");
                    }
                    break;
                case MENU_SCROLL_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_SCROLL;
                    break;
                case MENU_BLANK_SCENE:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BLANK_SCENE;
                    break;
                case MENU_TILEMAP_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_TILEMAP;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        }
    }

    if (!state->menu_open) {
        u16 visible_w = NGCameraGetVisibleWidth();
        u16 visible_h = NGCameraGetVisibleHeight();
        u16 brick_w = NGVisualAsset_brick.width_pixels;
        u16 brick_h = NGVisualAsset_brick.height_pixels;

        angle_t old_angle = state->cam_angle;
        state->cam_angle += CAM_CIRCLE_SPEED;

        // Toggle zoom when completing full rotation (angle wraps 255->0)
        if (state->cam_angle < old_angle) {
            u8 target = NGCameraGetTargetZoom();
            if (target == NG_CAM_ZOOM_100) {
                NGCameraSetTargetZoom(NG_CAM_ZOOM_75);
            } else {
                NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
            }
        }

        fixed center_x = FIX(((s16)brick_w - (s16)visible_w) / 2);
        fixed center_y = FIX(((s16)brick_h - (s16)visible_h) / 2);

        fixed offset_x = FIX_MUL(NGCos(state->cam_angle), state->cam_circle_radius);
        fixed offset_y = FIX_MUL(NGSin(state->cam_angle), state->cam_circle_radius);

        NGCameraSetPos(center_x + offset_x, center_y + offset_y);
    }

    if (!state->menu_open) {
        BallSystemUpdate(state->balls);
        update_day_night_cycle();
    }

    return state->switch_target;
}

void BallDemoCleanup(void) {
    NGMusicStop();

    /* Clean up lighting */
    if (state->night_layer != NG_LIGHTING_INVALID_HANDLE) {
        NGLightingPop(state->night_layer);
        state->night_layer = NG_LIGHTING_INVALID_HANDLE;
    }

    NGFixClear(0, 3, 40, 1);

    BallSystemDestroy(state->balls);

    NGActorRemoveFromScene(state->brick);
    NGActorDestroy(state->brick);

    NGParallaxRemoveFromScene(state->brick_shadow);
    NGParallaxDestroy(state->brick_shadow);

    NGParallaxRemoveFromScene(state->brick_pattern);
    NGParallaxDestroy(state->brick_pattern);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
