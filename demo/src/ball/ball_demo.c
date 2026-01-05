/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ball_demo.c - Bouncing balls demo implementation

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
#include <ngres_generated_assets.h>

// Demo state
static NGActorHandle brick;
static NGParallaxHandle brick_pattern;
static NGParallaxHandle brick_shadow;
static BallSystemHandle balls;
static NGMenuHandle menu;
static u8 menu_open;
static u8 switch_target;  // 0 = none, 2 = scroll, 3 = blank scene

// Camera circular motion
static angle_t cam_angle;           // Current angle (0-255 = full circle)
static fixed cam_circle_radius;     // Circle radius in pixels (fixed-point)
#define CAM_CIRCLE_SPEED  1         // Angle increment per frame (1 = ~4 seconds per rotation)
#define CAM_DEFAULT_RADIUS FIX(24)  // Default circle radius in pixels

// Menu item indices
#define MENU_RESUME        0
#define MENU_ADD_BALL      1
#define MENU_CLEAR_BALLS   2
#define MENU_TOGGLE_ZOOM   3
#define MENU_TOGGLE_MUSIC  4
// Index 5 is separator
#define MENU_SCROLL_DEMO   6
#define MENU_BLANK_SCENE   7
#define MENU_TILEMAP_DEMO  8

void BallDemoInit(void) {
    switch_target = 0;
    menu_open = 0;

    // Set background color to light gray
    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Create repeating brick pattern layer (below shadow)
    // Match the brick asset size (336x256) to avoid sprite limits
    brick_pattern = NGParallaxCreate(&NGVisualAsset_brick_pattern,
                                     336,   // Match brick width
                                     256,   // Match brick height
                                     FIX_FROM_FLOAT(0.8),
                                     FIX_FROM_FLOAT(0.8));
    NGParallaxAddToScene(brick_pattern, 0, 0, 4);  // Aligned with brick, Z=4

    // Create drop shadow as parallax layer (moves slower than camera for depth effect)
    brick_shadow = NGParallaxCreate(&NGVisualAsset_brick_shadow,
                                    NGVisualAsset_brick_shadow.width_pixels,
                                    NGVisualAsset_brick_shadow.height_pixels,
                                    FIX_FROM_FLOAT(0.9),
                                    FIX_FROM_FLOAT(0.9));
    NGParallaxAddToScene(brick_shadow, 8, 8, 5);  // Offset by 8px, Z=5 (behind brick)

    // Brick wall actor (palette auto-loaded from asset)
    brick = NGActorCreate(&NGVisualAsset_brick, 0, 0);
    NGActorAddToScene(brick, FIX(0), FIX(0), 10);  // Z=10

    // Create ball system
    balls = BallSystemCreate(&ng_arena_state, 8);
    BallSpawn(balls);
    BallSpawn(balls);

    // Create menu with palette dimming
    menu = NGMenuCreate(
        &ng_arena_state,
        &NGVisualAsset_ui_panel,
        &NGVisualAsset_ui_cursor,
        10  // Dim amount (0-31), uses palette fade instead of sprites
    );
    NGMenuSetTitle(menu, "BALL DEMO");
    NGMenuAddItem(menu, "Resume");
    NGMenuAddItem(menu, "Add Ball");
    NGMenuAddItem(menu, "Clear Balls");
    NGMenuAddItem(menu, "Toggle Zoom");
    NGMenuAddItem(menu, "Pause Music");
    NGMenuAddSeparator(menu, "--------");
    NGMenuAddItem(menu, "Scroll Demo");
    NGMenuAddItem(menu, "Blank Scene");
    NGMenuAddItem(menu, "Tilemap Demo");
    NGMenuSetSounds(menu, NGSFX_UI_CLICK, NGSFX_UI_SELECT);
    NGEngineSetActiveMenu(menu);

    // Display title
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");

    // Initialize camera circular motion
    cam_angle = 0;
    cam_circle_radius = CAM_DEFAULT_RADIUS;

    // Start background music
    NGMusicPlay(NGMUSIC_BALL_SCENE_MUSIC);
}

u8 BallDemoUpdate(void) {
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

    // Always update menu (for animations)
    NGMenuUpdate(menu);

    // Handle menu input
    if (menu_open) {
        if (NGMenuConfirmed(menu)) {
            switch (NGMenuGetSelection(menu)) {
                case MENU_RESUME:
                    NGMenuHide(menu);
                    menu_open = 0;
                    break;
                case MENU_ADD_BALL:
                    BallSpawn(balls);
                    break;
                case MENU_CLEAR_BALLS:
                    while (BallDestroyLast(balls)) {}
                    break;
                case MENU_TOGGLE_ZOOM:
                    {
                        u8 target = NGCameraGetTargetZoom();
                        if (target == NG_CAM_ZOOM_100) {
                            NGCameraSetTargetZoom(NG_CAM_ZOOM_75);
                        } else {
                            NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
                        }
                    }
                    break;
                case MENU_TOGGLE_MUSIC:
                    if (NGMusicIsPaused()) {
                        NGMusicResume();
                        NGMenuSetItemText(menu, MENU_TOGGLE_MUSIC, "Pause Music");
                    } else {
                        NGMusicPause();
                        NGMenuSetItemText(menu, MENU_TOGGLE_MUSIC, "Resume Music");
                    }
                    break;
                case MENU_SCROLL_DEMO:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_SCROLL;
                    break;
                case MENU_BLANK_SCENE:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_BLANK_SCENE;
                    break;
                case MENU_TILEMAP_DEMO:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_TILEMAP;
                    break;
            }
        }

        if (NGMenuCancelled(menu)) {
            NGMenuHide(menu);
            menu_open = 0;
        }
    }

    // Camera circular motion (paused when menu is open)
    if (!menu_open) {
        u16 visible_w = NGCameraGetVisibleWidth();
        u16 visible_h = NGCameraGetVisibleHeight();
        u16 brick_w = NGVisualAsset_brick.width_pixels;
        u16 brick_h = NGVisualAsset_brick.height_pixels;

        // Update angle for circular motion, detect wrap-around
        angle_t old_angle = cam_angle;
        cam_angle += CAM_CIRCLE_SPEED;

        // Toggle zoom when completing a full rotation (angle wraps from 255 to 0)
        if (cam_angle < old_angle) {
            u8 target = NGCameraGetTargetZoom();
            if (target == NG_CAM_ZOOM_100) {
                NGCameraSetTargetZoom(NG_CAM_ZOOM_75);
            } else {
                NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
            }
        }

        // Calculate center position (where camera orbits around)
        fixed center_x = FIX(((s16)brick_w - (s16)visible_w) / 2);
        fixed center_y = FIX(((s16)brick_h - (s16)visible_h) / 2);

        // Calculate circular offset using sin/cos
        fixed offset_x = FIX_MUL(NGCos(cam_angle), cam_circle_radius);
        fixed offset_y = FIX_MUL(NGSin(cam_angle), cam_circle_radius);

        // Set camera position: center + circular offset
        NGCameraSetPos(center_x + offset_x, center_y + offset_y);
    }

    // Update physics when menu is closed
    if (!menu_open) {
        BallSystemUpdate(balls);
    }

    return switch_target;
}

void BallDemoCleanup(void) {
    // Stop background music
    NGMusicStop();

    // Clear fix layer title
    NGFixClear(0, 3, 40, 1);

    // Destroy ball system (including physics world)
    BallSystemDestroy(balls);

    NGActorRemoveFromScene(brick);
    NGActorDestroy(brick);

    NGParallaxRemoveFromScene(brick_shadow);
    NGParallaxDestroy(brick_shadow);

    NGParallaxRemoveFromScene(brick_pattern);
    NGParallaxDestroy(brick_pattern);

    NGMenuDestroy(menu);

    // Reset backdrop color to black
    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Reset camera
    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
