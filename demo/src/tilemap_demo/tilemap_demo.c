/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "tilemap_demo.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <input.h>
#include <tilemap.h>
#include <camera.h>
#include <palette.h>
#include <color.h>
#include <ui.h>
#include <arena.h>
#include <actor.h>
#include <engine.h>
#include <ngres_generated_assets.h>

#define PLAYER_HALF_W  FIX(6)
#define PLAYER_HALF_H  FIX(12)
#define PLAYER_SPEED   FIX(2)

// Tuned jump physics (inspired by Celeste/Hollow Knight)
#define JUMP_FORCE       FIX_FROM_FLOAT(-6.5)   // Initial jump velocity
#define GRAVITY_UP       FIX_FROM_FLOAT(0.35)   // Gravity while rising
#define GRAVITY_DOWN     FIX_FROM_FLOAT(0.55)   // Gravity while falling (faster)
#define JUMP_CUT_MULT    FIX_FROM_FLOAT(0.4)    // Velocity multiplier when releasing jump early
#define MAX_FALL_SPEED   FIX(10)                // Terminal velocity
#define COYOTE_FRAMES    6                      // Frames after leaving ground you can still jump
#define JUMP_BUFFER_FRAMES 6                    // Frames before landing a jump press is remembered

// Level dimensions: 60x14 tiles = 960x224 pixels (3 screens wide)
#define LEVEL_WIDTH  60
#define LEVEL_HEIGHT 14
#define LEVEL_WIDTH_PX  (LEVEL_WIDTH * 16)
#define LEVEL_HEIGHT_PX (LEVEL_HEIGHT * 16)

// Test tilemap data (60x14 tiles = 960x224 pixels, 3 screens wide)
// Tile 0 = empty, Tile 1 = solid green platform
static const u8 test_tile_data[] = {
    // Row 0: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 1: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 2: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 3: high platforms
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 4: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 5: floating platforms (staggered)
    0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,0,0,0,0,
    // Row 6: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 7: mid platforms
    0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,1,1,1,1,0,0, 0,0,0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0,
    // Row 8: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 9: low platforms
    0,0,0,0,0,0,1,1,1,1, 1,1,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,1,1, 1,1,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 10: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 11: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 12: ground with gaps
    1,1,1,1,1,1,1,0,0,0, 0,0,1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,1,1,1, 1,1,1,1,1,1,1,1,0,0, 0,0,0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,
    // Row 13: solid ground
    1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,
};

// Collision data (same layout as tile_data)
static const u8 test_collision_data[] = {
    // Row 0: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 1: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 2: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 3: high platforms
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 4: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 5: floating platforms (staggered)
    0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,0,0,0,0,
    // Row 6: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 7: mid platforms
    0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,1,1,1,1,0,0, 0,0,0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0,
    // Row 8: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 9: low platforms
    0,0,0,0,0,0,1,1,1,1, 1,1,0,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,1,1, 1,1,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 10: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 11: empty
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    // Row 12: ground with gaps
    1,1,1,1,1,1,1,0,0,0, 0,0,1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,1,1,1, 1,1,1,1,1,1,1,1,0,0, 0,0,0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,
    // Row 13: solid ground
    1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,
};

// Palette lookup - all solid tiles use the tileset palette
static const u8 test_tile_to_palette[256] = {
    [0] = NGPAL_TILES_SIMPLE,
    [1] = NGPAL_TILES_SIMPLE,
};

// Test tilemap asset - uses tiles_simple tileset
static const NGTilemapAsset test_tilemap = {
    .name = "test_level",
    .width_tiles = LEVEL_WIDTH,
    .height_tiles = LEVEL_HEIGHT,
    .base_tile = 1430,  // tiles_simple base_tile from generated assets
    .tile_data = test_tile_data,
    .collision_data = test_collision_data,
    .tile_to_palette = test_tile_to_palette,
    .default_palette = NGPAL_TILES_SIMPLE,
};

typedef struct TilemapDemoState {
    NGMenuHandle menu;
    NGActorHandle player;
    NGTilemapHandle tilemap;
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

    state->tilemap = NGTilemapCreate(&test_tilemap);
    NGTilemapAddToScene(state->tilemap, 0, 0, 0);

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
    NGCameraSetFollowSpeed(FIX_FROM_FLOAT(0.12));
    NGCameraSetBounds(LEVEL_WIDTH_PX, LEVEL_HEIGHT_PX);

    state->menu = NGMenuCreate(
        &ng_arena_state,
        &NGVisualAsset_ui_panel,
        &NGVisualAsset_ui_cursor,
        10
    );
    NGMenuSetTitle(state->menu, "TILEMAP DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuAddItem(state->menu, "Scroll Demo");
    NGMenuSetSounds(state->menu, NGSFX_UI_CLICK, NGSFX_UI_SELECT);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "TILEMAP DEMO");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0, "DPAD:MOVE  A:JUMP  START:MENU");
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

        u8 coll = NGTilemapResolveAABB(
            state->tilemap,
            &state->player_x, &state->player_y,
            PLAYER_HALF_W, PLAYER_HALF_H,
            &state->player_vel_x, &state->player_vel_y
        );

        state->on_ground = (coll & NG_COLL_BOTTOM) ? 1 : 0;

        if (state->player_x < PLAYER_HALF_W) {
            state->player_x = PLAYER_HALF_W;
            state->player_vel_x = 0;
        }
        if (state->player_x > FIX(LEVEL_WIDTH_PX) - PLAYER_HALF_W) {
            state->player_x = FIX(LEVEL_WIDTH_PX) - PLAYER_HALF_W;
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

    NGTilemapRemoveFromScene(state->tilemap);
    NGTilemapDestroy(state->tilemap);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
