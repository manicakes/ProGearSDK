/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "ball_demo.h"
#include "ball.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <scene.h>
#include <backdrop.h>
#include <actor.h>
#include <ng_input.h>
#include <camera.h>
#include <ng_arena.h>
#include <ui.h>
#include <ng_audio.h>
#include <ng_palette.h>
#include <ng_color.h>
#include <ng_math.h>
#include <ng_interrupt.h>
#include <ng_sprite.h>
#include <graphic.h>
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
    NGActorHandle brick;
    NGBackdropHandle brick_pattern;
    NGBackdropHandle brick_shadow;
    BallSystemHandle balls;
    NGMenuHandle menu;
    angle_t cam_angle;
    fixed cam_circle_radius;
    u8 menu_open;
    u8 switch_target;

    /* Night mode state */
    u16 day_night_timer;
    u8 is_night;
    NGLightingLayerHandle night_preset;
    u16 lightning_timer;
    u16 rng_state;

    /* Raster effects */
    NGGraphic *rain_overlay; /* Transparent streak overlay for storm mode */
} BallDemoState;

static BallDemoState *state;

/* ============================================================
 * Raster effects: heat wave (day) and rain (storm)
 *
 * A timer interrupt rewrites sprite X positions (SCB4) in bands of a few
 * scanlines. The timer is re-armed at the very start of each frame (during
 * VBlank) with a fuse long enough to clear the rest of VBlank and the
 * scene's VRAM flush in NGEngineFrameEnd - an interrupt landing inside
 * those writes would corrupt them. The top display lines above the first
 * interrupt keep the scene's own positions.
 * ============================================================ */

#define FX_VBLANK_LINES 32 /* Approx scanlines from the VBlank IRQ to display line 0 */
#define FX_FIRST_LINE   16 /* Target display line of the first interrupt */

/* Heat wave: subtle sine displacement of the brick backdrop and logo
 * (not the balls) that drifts each frame like air shimmering. */
#define HEAT_BAND_LINES  8  /* Scanlines per displacement band */
#define HEAT_BANDS       28 /* 224 screen lines / 8 */
#define HEAT_AMPLITUDE   2  /* Max displacement in pixels (subtle) */
#define HEAT_ANGLE_STEP  14 /* Wave angle advance per band (~1.5 waves down the screen) */
#define HEAT_PHASE_STEP  3  /* Wave angle advance per frame (shimmer speed) */
#define HEAT_TARGETS_MAX 3  /* brick_pattern, brick_shadow, brick logo */

/* Rain: transparent streak overlay sheared into falling diagonal rain
 * (same technique as the raster demo). */
#define RAIN_BAND_LINES 4   /* Scanlines per shear band */
#define RAIN_BANDS      56  /* 224 screen lines / 4 */
#define RAIN_WIDTH      384 /* 32px margin each side of the 320px screen */
#define RAIN_HEIGHT     288 /* Screen + one 64px texture repeat for vertical scroll */
#define RAIN_X          -32
#define RAIN_ROWS       18 /* RAIN_HEIGHT in 16px tiles (for SCB3 writes) */
#define RAIN_SHEAR      1  /* Extra X pixels per band (slant of the streaks) */
#define RAIN_FALL       8  /* Fall speed in pixels per frame */
#define RAIN_DRIFT      2  /* X pixels per frame: RAIN_FALL * RAIN_SHEAR / RAIN_BAND_LINES */
#define RAIN_X_WRAP     31 /* X offsets wrap at the 32px horizontal repeat */
#define RAIN_Y_WRAP     63 /* Y offsets wrap at the 64px vertical repeat */

typedef enum { FX_NONE, FX_HEAT, FX_RAIN } RasterFxMode;

static volatile RasterFxMode fx_mode = FX_NONE;
static volatile u8 raster_band = 0;

static volatile NGGraphicRasterXInfo heat_targets[HEAT_TARGETS_MAX];
static volatile u8 heat_target_count = 0;
static volatile u8 heat_phase = 0;

static volatile u16 rain_first = 0; /* Hardware sprites backing the overlay */
static volatile u8 rain_count = 0;
static volatile u8 rain_x_phase = 0;
static u8 rain_y_phase = 0; /* Only read by the main thread */

static s16 heat_wave_offset(u8 band) {
    angle_t a = (angle_t)(heat_phase + band * HEAT_ANGLE_STEP);
    return (s16)((NGSin(a) * HEAT_AMPLITUDE) >> FIX_SHIFT);
}

static void raster_fx_handler(void) {
    switch (fx_mode) {
        case FX_HEAT:
            if (raster_band < HEAT_BANDS) {
                s16 off = heat_wave_offset(raster_band);
                for (u8 i = 0; i < heat_target_count; i++) {
                    NGSpriteXSetSpaced(heat_targets[i].first_sprite, heat_targets[i].count,
                                       (s16)(heat_targets[i].base_x + off),
                                       heat_targets[i].spacing);
                }
                raster_band++;
                if (raster_band < HEAT_BANDS) {
                    NGTimerSetReload(NGTimerScanlineToReload(HEAT_BAND_LINES));
                }
            }
            break;

        case FX_RAIN:
            /* Linear shear: each band shifts a little further right,
             * turning the overlay's vertical dashes into diagonal streaks */
            if (raster_band < RAIN_BANDS && rain_count > 0) {
                s16 x = (s16)(RAIN_X + ((rain_x_phase + raster_band * RAIN_SHEAR) & RAIN_X_WRAP));
                NGSpriteXSetSpaced(rain_first, rain_count, x, 16);
                raster_band++;
                if (raster_band < RAIN_BANDS) {
                    NGTimerSetReload(NGTimerScanlineToReload(RAIN_BAND_LINES));
                }
            }
            break;

        default:
            break;
    }
}

/* Called once per frame: pick the effect for the current mode, refresh the
 * displacement targets, and arm the timer for this frame's bands. */
static void update_raster_fx(void) {
    if (state->is_night) {
        if (fx_mode != FX_RAIN) {
            fx_mode = FX_RAIN;
            NGGraphicSetVisible(state->rain_overlay, 1);
        }

        /* Slide the overlay along the streak direction: down by the fall
         * speed, right by the matching shear drift */
        rain_x_phase = (u8)((rain_x_phase + RAIN_DRIFT) & RAIN_X_WRAP);
        rain_y_phase = (u8)((rain_y_phase + RAIN_FALL) & RAIN_Y_WRAP);

        u16 first;
        u8 count;
        if (NGGraphicGetSpriteRange(state->rain_overlay, &first, &count)) {
            rain_first = first;
            rain_count = count;
            /* The overlay is not scene-managed, so its vertical scroll
             * (SCB3) and top-of-frame X are written here during VBlank */
            NGSpriteYSetUniform(first, count, (s16)(rain_y_phase - 64), RAIN_ROWS);
            NGSpriteXSetSpaced(first, count, (s16)(RAIN_X + rain_x_phase), 16);
        } else {
            rain_count = 0; /* Not allocated until first draw */
        }
        raster_band = FX_FIRST_LINE / RAIN_BAND_LINES;
    } else {
        if (fx_mode != FX_HEAT) {
            fx_mode = FX_HEAT;
            NGGraphicSetVisible(state->rain_overlay, 0);
        }

        heat_phase = (u8)(heat_phase + HEAT_PHASE_STEP);

        /* Block the handler while the target array is rebuilt */
        heat_target_count = 0;

        /* Suspend the wave during zoom transitions: the snapshot lags the
         * animated scale by a frame, which would misplace columns. */
        u8 n = 0;
        if (!NGCameraIsZooming()) {
            NGGraphic *targets[HEAT_TARGETS_MAX] = {
                NGBackdropGetGraphic(state->brick_pattern),
                NGBackdropGetGraphic(state->brick_shadow),
                NGActorGetGraphic(state->brick),
            };
            NGGraphicRasterXInfo info;
            for (u8 i = 0; i < HEAT_TARGETS_MAX; i++) {
                if (NGGraphicGetRasterXInfo(targets[i], &info)) {
                    heat_targets[n].first_sprite = info.first_sprite;
                    heat_targets[n].count = info.count;
                    heat_targets[n].base_x = info.base_x;
                    heat_targets[n].spacing = info.spacing;
                    n++;
                }
            }
        }
        heat_target_count = n;
        raster_band = FX_FIRST_LINE / HEAT_BAND_LINES;
    }

    /* Writing the reload restarts the countdown, cancelling the short
     * countdown the hardware auto-loaded at VBlank start */
    NGTimerSetReload(NGTimerScanlineToReload(FX_VBLANK_LINES + FX_FIRST_LINE));
}

#define MENU_RESUME       0
#define MENU_ADD_BALL     1
#define MENU_CLEAR_BALLS  2
#define MENU_TOGGLE_ZOOM  3
#define MENU_TOGGLE_MUSIC 4
// Index 5 is separator
#define MENU_SCROLL_DEMO  6
#define MENU_BLANK_SCENE  7
#define MENU_TILEMAP_DEMO 8
#define MENU_MVS_DEMO     9
#define MENU_RASTER_DEMO  10

/* Opening the menu pauses the world: the engine stops the raster timer, freezes
 * world animation, and hides the rain overlay (NG_PAUSE_HIDE). */
static void set_menu_open(u8 open) {
    state->menu_open = open;
    if (open) {
        NGEnginePause();
        /* The handler is disabled mid-frame, so the bands it already displaced
         * keep their offsets; re-arm from a clean state on resume. */
        fx_mode = FX_NONE;
        heat_target_count = 0;
        rain_count = 0;
    } else {
        NGEngineResume();
    }
}

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

    /* Update pre-baked fade animation */
    NGLightingUpdatePrebakedFade();

    /* Check if fade-out completed (transitioning back to day) */
    if (state->is_night && state->night_preset == NG_LIGHTING_INVALID) {
        /* Fade completed, now fully day */
        state->is_night = 0;
    }

    /* Transition to night */
    if (!state->is_night && !NGLightingIsPrebakedFading() &&
        state->day_night_timer >= NIGHT_MODE_CYCLE_FRAMES) {
        state->is_night = 1;
        state->day_night_timer = 0;
        /* Use pre-baked night preset - zero CPU palette transforms! */
        state->night_preset =
            NGLightingPushPreset(NG_LIGHTING_PREBAKED_NIGHT, NIGHT_TRANSITION_FRAMES);
        state->lightning_timer =
            ball_demo_rand_range(LIGHTNING_MIN_INTERVAL, LIGHTNING_MAX_INTERVAL);
        BallSystemSetGravity(state->balls, FIX(-1));
    }
    /* Transition to day */
    else if (state->is_night && !NGLightingIsPrebakedFading() &&
             state->day_night_timer >= NIGHT_MODE_DURATION) {
        state->day_night_timer = 0;
        /* Pop preset with fade - animates back to original palettes */
        NGLightingPopPreset(state->night_preset, NIGHT_TRANSITION_FRAMES);
        state->night_preset = NG_LIGHTING_INVALID;
        BallSystemSetGravity(state->balls, FIX(1));
    }

    if (state->is_night && !NGLightingIsPrebakedFading()) {
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
    state->night_preset = NG_LIGHTING_INVALID;
    state->lightning_timer = 0;
    state->rng_state = 12345;

    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Match brick asset size to avoid sprite limits
    state->brick_pattern =
        NGBackdropCreate(&NGVisualAsset_brick_pattern, 336, 256, FIX(0.8), FIX(0.8));
    NGBackdropAddToScene(state->brick_pattern, 0, 0, 4);

    // Shadow moves slower than camera for depth effect
    state->brick_shadow =
        NGBackdropCreate(&NGVisualAsset_brick_shadow, NGVisualAsset_brick_shadow.width_pixels,
                         NGVisualAsset_brick_shadow.height_pixels, FIX(0.9), FIX(0.9));
    NGBackdropAddToScene(state->brick_shadow, 8, 8, 5);

    state->brick = NGActorCreate(&NGVisualAsset_brick, 0, 0);
    NGActorAddToScene(state->brick, FIX(0), FIX(0), 10);

    state->balls = BallSystemCreate(&ng_arena_state, 8);
    BallSpawn(state->balls);
    BallSpawn(state->balls);

    /* Rain overlay for storm mode: transparent streaks over the whole
     * scene, one texture repeat wider/taller than the screen so the
     * displacement and vertical scroll wraps stay hidden */
    NGGraphicConfig rain_cfg = {
        .width = RAIN_WIDTH,
        .height = RAIN_HEIGHT,
        .tile_mode = NG_GRAPHIC_TILE_REPEAT,
        .layer = NG_GRAPHIC_LAYER_FOREGROUND,
        .z_order = 255,
    };
    state->rain_overlay = NGGraphicCreate(&rain_cfg);
    NGGraphicSetSource(state->rain_overlay, &NGVisualAsset_rain_streaks, NGPAL_RAIN_STREAKS);
    NGGraphicSetPosition(state->rain_overlay, RAIN_X, -64);
    NGGraphicSetVisible(state->rain_overlay, 0);
    /* 24 sprites wide: with the backdrops and the menu panel this overruns the
     * 96-per-scanline limit, so drop it while the pause menu is up */
    NGGraphicSetPauseBehavior(state->rain_overlay, NG_PAUSE_HIDE);

    /* Raster effects run for the demo's whole lifetime */
    fx_mode = FX_NONE;
    raster_band = 0;
    heat_target_count = 0;
    rain_count = 0;
    NGInterruptSetTimerHandler(raster_fx_handler);
    NGTimerSetReload(NGTimerScanlineToReload(FX_VBLANK_LINES + FX_FIRST_LINE));
    NGTimerEnable();

    state->menu = NGMenuCreateDefault(&ng_arena_state, 12);
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
    NGMenuAddItem(state->menu, "MVS Features");
    NGMenuAddItem(state->menu, "Raster Effects");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");

    NGMusicPlay(NGMUSIC_BALL_SCENE_MUSIC);
}

u8 BallDemoUpdate(void) {
    /* First thing each frame: re-arm the raster timer while VBlank has
     * barely started, so its stray auto-reloaded countdown can't fire
     * during this frame's VRAM writes. Skipped while paused - the timer is
     * disabled and the rain overlay hidden, and re-arming would undo both. */
    if (!NGEngineIsPaused()) {
        update_raster_fx();
    }

    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (state->menu_open) {
            NGMenuHide(state->menu);
            set_menu_open(0);
        } else {
            NGMenuShow(state->menu);
            set_menu_open(1);
        }
    }

    NGMenuUpdate(state->menu);

    if (state->menu_open) {
        if (NGMenuConfirmed(state->menu)) {
            switch (NGMenuGetSelection(state->menu)) {
                case MENU_RESUME:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    break;
                case MENU_ADD_BALL:
                    BallSpawn(state->balls);
                    /* Sprite allocation shifts at the next draw; this
                     * frame's target snapshot is stale, so skip its raster
                     * writes rather than displace the wrong sprites */
                    heat_target_count = 0;
                    rain_count = 0;
                    break;
                case MENU_CLEAR_BALLS:
                    while (BallDestroyLast(state->balls)) {}
                    heat_target_count = 0;
                    rain_count = 0;
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
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_SCROLL;
                    break;
                case MENU_BLANK_SCENE:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_BLANK_SCENE;
                    break;
                case MENU_TILEMAP_DEMO:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_TILEMAP;
                    break;
                case MENU_MVS_DEMO:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_MVS;
                    break;
                case MENU_RASTER_DEMO:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_RASTER;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            set_menu_open(0);
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
    /* Leave the engine unpaused for whatever state comes next */
    NGEngineResume();

    /* Stop raster effects before tearing down their target graphics */
    NGTimerDisable();
    NGInterruptSetTimerHandler(0);
    fx_mode = FX_NONE;

    NGGraphicDestroy(state->rain_overlay);

    NGMusicStop();

    /* Clean up lighting - instant pop (no fade) */
    if (state->night_preset != NG_LIGHTING_INVALID) {
        NGLightingPopPreset(state->night_preset, 0);
        state->night_preset = NG_LIGHTING_INVALID;
    }

    NGFixClear(0, 3, 40, 1);

    BallSystemDestroy(state->balls);

    NGActorRemoveFromScene(state->brick);
    NGActorDestroy(state->brick);

    NGBackdropRemoveFromScene(state->brick_shadow);
    NGBackdropDestroy(state->brick_shadow);

    NGBackdropRemoveFromScene(state->brick_pattern);
    NGBackdropDestroy(state->brick_pattern);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
