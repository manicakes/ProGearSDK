/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "tilemap_demo.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <scene.h>
#include <camera.h>
#include <ng_palette.h>
#include <ng_color.h>
#include <ui.h>
#include <ng_arena.h>
#include <actor.h>
#include <engine.h>
#include <progear_assets.h>

#define PLAYER_HALF_W FIX(6)
#define PLAYER_HALF_H FIX(12)
#define PLAYER_SPEED  FIX(2)

// Tuned jump physics (inspired by Celeste/Hollow Knight)
#define JUMP_FORCE         FIX(-6.5) // Initial jump velocity
#define GRAVITY_UP         FIX(0.35) // Gravity while rising
#define GRAVITY_DOWN       FIX(0.55) // Gravity while falling (faster)
#define JUMP_CUT_MULT      FIX(0.4)  // Velocity multiplier when releasing jump early
#define MAX_FALL_SPEED     FIX(10)              // Terminal velocity
#define COYOTE_FRAMES      6                    // Frames after leaving ground you can still jump
#define JUMP_BUFFER_FRAMES 6                    // Frames before landing a jump press is remembered

typedef struct TilemapDemoState {
    NGMenuHandle menu;
    NGActorHandle player;
    u16 level_width;
    u16 level_height;
    fixed player_x;
    fixed player_y;
    fixed player_vel_x;
    fixed player_vel_y;
    u8 menu_open;
    u8 switch_target;
    u8 on_ground;
    u8 coyote_timer;
    u8 jump_buffer;
    u8 jumping;
} TilemapDemoState;

static TilemapDemoState *state;

#define MENU_RESUME      0
#define MENU_BALL_DEMO   1
#define MENU_SCROLL_DEMO 2

void TilemapDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, TilemapDemoState);
    state->switch_target = 0;
    state->menu_open = 0;

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);

    NGPalSetBackdrop(NG_COLOR_DARK_BLUE);

    NGPalSet(NGPAL_TILES_SIMPLE, NGPal_tiles_simple);

    // Set the scene's terrain
    NGSceneSetTerrain(&NGTerrainAsset_tilemap_demo_level);
    NGSceneGetTerrainBounds(&state->level_width, &state->level_height);

    // Ground is at row 12 (y=192), start above it
    state->player_x = FIX(80);
    state->player_y = FIX(100);
    state->player_vel_x = 0;
    state->player_vel_y = 0;
    state->on_ground = 0;
    state->coyote_timer = 0;
    state->jump_buffer = 0;
    state->jumping = 0;

    // Offset sprite so it's centered on collision AABB
    state->player = NGActorCreate(&NGVisualAsset_ball, 0, 0);
    NGActorAddToScene(state->player, state->player_x - FIX(16), state->player_y - FIX(16), 10);

    NGCameraTrackActor(state->player);
    NGCameraSetDeadzone(80, 40);
    NGCameraSetFollowSpeed(FIX(0.12));
    NGCameraSetBounds(state->level_width, state->level_height);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "TILEMAP DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuAddItem(state->menu, "Scroll Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "TILEMAP DEMO");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0,
                "DPAD:MOVE  A:JUMP  START:MENU");
}

u8 TilemapDemoUpdate(void) {
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
                case MENU_BALL_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BALL;
                    break;
                case MENU_SCROLL_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_SCROLL;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        }
    } else {
        state->player_vel_x = 0;

        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
            state->player_vel_x = -PLAYER_SPEED;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
            state->player_vel_x = PLAYER_SPEED;
        }

        // Jump buffering: remember presses for a few frames
        if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
            state->jump_buffer = JUMP_BUFFER_FRAMES;
        } else if (state->jump_buffer > 0) {
            state->jump_buffer--;
        }

        // Coyote time: allow jumping briefly after leaving ground
        if (state->on_ground) {
            state->coyote_timer = COYOTE_FRAMES;
        } else if (state->coyote_timer > 0) {
            state->coyote_timer--;
        }

        u8 can_jump = (state->on_ground || state->coyote_timer > 0);
        u8 want_jump = (state->jump_buffer > 0);

        if (can_jump && want_jump) {
            state->player_vel_y = JUMP_FORCE;
            state->jumping = 1;
            state->coyote_timer = 0;
            state->jump_buffer = 0;
        }

        // Variable jump height: release A early to cut jump short
        if (state->jumping && state->player_vel_y < 0 && !NGInputHeld(NG_PLAYER_1, NG_BTN_A)) {
            state->player_vel_y = FIX_MUL(state->player_vel_y, JUMP_CUT_MULT);
            state->jumping = 0;
        }

        if (state->player_vel_y >= 0) {
            state->jumping = 0;
        }

        // Asymmetric gravity: lower while rising, higher while falling
        if (state->player_vel_y < 0) {
            state->player_vel_y += GRAVITY_UP;
        } else {
            state->player_vel_y += GRAVITY_DOWN;
        }

        if (state->player_vel_y > MAX_FALL_SPEED) {
            state->player_vel_y = MAX_FALL_SPEED;
        }

        // Resolve collision against the scene's terrain
        u8 coll =
            NGSceneResolveCollision(&state->player_x, &state->player_y, PLAYER_HALF_W,
                                    PLAYER_HALF_H, &state->player_vel_x, &state->player_vel_y);
        state->on_ground = (coll & NG_COLL_BOTTOM) ? 1 : 0;

        if (state->player_x < PLAYER_HALF_W) {
            state->player_x = PLAYER_HALF_W;
            state->player_vel_x = 0;
        }
        if (state->player_x > FIX(state->level_width) - PLAYER_HALF_W) {
            state->player_x = FIX(state->level_width) - PLAYER_HALF_W;
            state->player_vel_x = 0;
        }

        if (state->player_y > FIX(250)) {
            state->player_x = FIX(80);
            state->player_y = FIX(100);
            state->player_vel_y = 0;
        }

        NGActorSetPos(state->player, state->player_x - FIX(16), state->player_y - FIX(16));
    }

    return state->switch_target;
}

void TilemapDemoCleanup(void) {
    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 27, 40, 1);

    NGCameraStopTracking();

    NGActorRemoveFromScene(state->player);
    NGActorDestroy(state->player);

    // Clear the scene's terrain
    NGSceneClearTerrain();

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
