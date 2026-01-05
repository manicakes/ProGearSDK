/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// tilemap_demo.c - Tilemap system demo
// Demonstrates tilemap rendering and collision detection

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

// Demo state
static NGMenuHandle menu;
static u8 menu_open;
static u8 switch_target;

// Player state
static NGActorHandle player;
static fixed player_x;
static fixed player_y;
static fixed player_vel_x;
static fixed player_vel_y;
static u8 on_ground;
static u8 coyote_timer;      // Frames since leaving ground (for coyote time)
static u8 jump_buffer;       // Frames since jump was pressed (for jump buffering)
static u8 jumping;           // Currently in a jump (for variable height)

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

static NGTilemapHandle tilemap;

// Menu item indices
#define MENU_RESUME      0
#define MENU_BALL_DEMO   1
#define MENU_SCROLL_DEMO 2

void TilemapDemoInit(void) {
    switch_target = 0;
    menu_open = 0;

    // Reset camera to origin
    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);

    // Set background color
    NGPalSetBackdrop(NG_COLOR_DARK_BLUE);

    // Load tileset palette
    NGPalSet(NGPAL_TILES_SIMPLE, NGPal_tiles_simple);

    // Create and add tilemap to scene
    tilemap = NGTilemapCreate(&test_tilemap);
    NGTilemapAddToScene(tilemap, 0, 0, 0);  // At origin, Z=0 (behind player)

    // Initialize player position (above solid ground on left side)
    // Ground is at row 12 (y=192), start above it, avoiding the gap
    player_x = FIX(80);
    player_y = FIX(100);
    player_vel_x = 0;
    player_vel_y = 0;
    on_ground = 0;
    coyote_timer = 0;
    jump_buffer = 0;
    jumping = 0;

    // Create player sprite using ball asset (32x32)
    // Offset sprite position so it's centered on collision AABB
    player = NGActorCreate(&NGVisualAsset_ball, 0, 0);
    NGActorAddToScene(player, player_x - FIX(16), player_y - FIX(16), 10);

    // Set up camera to track the player (Metal Slug-style)
    NGCameraTrackActor(player);
    NGCameraSetDeadzone(80, 40);              // Player can move in center area freely
    NGCameraSetFollowSpeed(FIX_FROM_FLOAT(0.12));  // Smooth follow
    NGCameraSetBounds(LEVEL_WIDTH_PX, LEVEL_HEIGHT_PX);  // Clamp to level

    // Create menu
    menu = NGMenuCreate(
        &ng_arena_state,
        &NGVisualAsset_ui_panel,
        &NGVisualAsset_ui_cursor,
        10
    );
    NGMenuSetTitle(menu, "TILEMAP DEMO");
    NGMenuAddItem(menu, "Resume");
    NGMenuAddItem(menu, "Ball Demo");
    NGMenuAddItem(menu, "Scroll Demo");
    NGMenuSetSounds(menu, NGSFX_UI_CLICK, NGSFX_UI_SELECT);
    NGEngineSetActiveMenu(menu);

    // Display title and controls
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "TILEMAP DEMO");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0, "DPAD:MOVE  A:JUMP  START:MENU");
}

u8 TilemapDemoUpdate(void) {
    // Toggle menu with START button
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (menu_open) {
            NGMenuHide(menu);
            menu_open = 0;
        } else {
            NGMenuShow(menu);
            menu_open = 1;
        }
    }

    NGMenuUpdate(menu);

    // Handle menu input
    if (menu_open) {
        if (NGMenuConfirmed(menu)) {
            switch (NGMenuGetSelection(menu)) {
                case MENU_RESUME:
                    NGMenuHide(menu);
                    menu_open = 0;
                    break;
                case MENU_BALL_DEMO:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_BALL;
                    break;
                case MENU_SCROLL_DEMO:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_SCROLL;
                    break;
            }
        }

        if (NGMenuCancelled(menu)) {
            NGMenuHide(menu);
            menu_open = 0;
        }
    } else {
        // Player input
        player_vel_x = 0;

        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
            player_vel_x = -PLAYER_SPEED;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
            player_vel_x = PLAYER_SPEED;
        }

        // === Jump Buffering ===
        // Remember jump presses for a few frames (helps timing)
        if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
            jump_buffer = JUMP_BUFFER_FRAMES;
        } else if (jump_buffer > 0) {
            jump_buffer--;
        }

        // === Coyote Time ===
        // Allow jumping for a few frames after leaving ground
        if (on_ground) {
            coyote_timer = COYOTE_FRAMES;
        } else if (coyote_timer > 0) {
            coyote_timer--;
        }

        // === Jump Initiation ===
        // Can jump if: (on ground OR within coyote time) AND (just pressed OR buffered)
        u8 can_jump = (on_ground || coyote_timer > 0);
        u8 want_jump = (jump_buffer > 0);

        if (can_jump && want_jump) {
            player_vel_y = JUMP_FORCE;
            jumping = 1;
            coyote_timer = 0;  // Consume coyote time
            jump_buffer = 0;   // Consume buffer
        }

        // === Variable Jump Height ===
        // Release A early to cut jump short (only while rising)
        if (jumping && player_vel_y < 0 && !NGInputHeld(NG_PLAYER_1, NG_BTN_A)) {
            player_vel_y = FIX_MUL(player_vel_y, JUMP_CUT_MULT);
            jumping = 0;  // No longer in variable jump phase
        }

        // End jump state when starting to fall
        if (player_vel_y >= 0) {
            jumping = 0;
        }

        // === Asymmetric Gravity ===
        // Lower gravity while rising (more hangtime at apex)
        // Higher gravity while falling (snappier descent)
        if (player_vel_y < 0) {
            player_vel_y += GRAVITY_UP;
        } else {
            player_vel_y += GRAVITY_DOWN;
        }

        // Terminal velocity
        if (player_vel_y > MAX_FALL_SPEED) {
            player_vel_y = MAX_FALL_SPEED;
        }

        // Resolve tilemap collision
        u8 coll = NGTilemapResolveAABB(
            tilemap,
            &player_x, &player_y,
            PLAYER_HALF_W, PLAYER_HALF_H,
            &player_vel_x, &player_vel_y
        );

        // Update ground state
        on_ground = (coll & NG_COLL_BOTTOM) ? 1 : 0;

        // Clamp player to level bounds
        if (player_x < PLAYER_HALF_W) {
            player_x = PLAYER_HALF_W;
            player_vel_x = 0;
        }
        if (player_x > FIX(LEVEL_WIDTH_PX) - PLAYER_HALF_W) {
            player_x = FIX(LEVEL_WIDTH_PX) - PLAYER_HALF_W;
            player_vel_x = 0;
        }

        // Respawn if player falls off screen
        if (player_y > FIX(250)) {
            player_x = FIX(80);
            player_y = FIX(100);
            player_vel_y = 0;
        }

        // Update player sprite position (offset for centering)
        NGActorSetPos(player, player_x - FIX(16), player_y - FIX(16));
    }

    return switch_target;
}

void TilemapDemoCleanup(void) {
    // Clear fix layer text
    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 27, 40, 1);

    // Stop camera tracking
    NGCameraStopTracking();

    // Destroy player
    NGActorRemoveFromScene(player);
    NGActorDestroy(player);

    // Remove tilemap from scene and destroy
    NGTilemapRemoveFromScene(tilemap);
    NGTilemapDestroy(tilemap);

    NGMenuDestroy(menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Reset camera
    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
