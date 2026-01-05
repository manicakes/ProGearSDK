/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// scroll_demo.c - Parallax scrolling demo implementation

#include "scroll_demo.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <scene.h>
#include <parallax.h>
#include <input.h>
#include <camera.h>
#include <arena.h>
#include <ui.h>
#include <engine.h>
#include <ngres_generated_assets.h>

// Demo state
static NGParallaxHandle back;
static NGParallaxHandle middle;
static NGParallaxHandle front;
static NGMenuHandle menu;
static u8 menu_open;
static u8 switch_target;  // 0 = none, 1 = ball, 3 = blank scene

// Auto-scroll state
static s16 scroll_x;           // Current X scroll position (0 to 320)
static s8 scroll_dir;          // 1 = scrolling right, -1 = scrolling left
static u8 bob_phase;           // Phase for vertical bobbing (0-255)

#define SCROLL_SPEED      2    // Pixels per frame
#define SCREEN_WIDTH    320    // One screen length
#define BOB_AMPLITUDE     10    // +/- 3 pixels

// Menu item indices
#define MENU_RESUME        0
#define MENU_TOGGLE_ZOOM   1
#define MENU_RESET_CAMERA  2
// Index 3 is separator
#define MENU_BALL_DEMO     4
#define MENU_BLANK_SCENE 5

void ScrollDemoInit(void) {
    switch_target = 0;
    menu_open = 0;

    // Initialize auto-scroll state
    scroll_x = 0;
    scroll_dir = 1;  // Start scrolling right
    bob_phase = 0;

    // Back layer - distant mountains, slow parallax
    back = NGParallaxCreate(
        &NGVisualAsset_back_layer,
        NG_PARALLAX_WIDTH_INFINITE,  // Infinite width for seamless scrolling
        0,                            // Use asset height
        FIX_FROM_FLOAT(0.25),         // Slow horizontal parallax
        FIX_FROM_FLOAT(0.25)          // Slow vertical parallax
    );
    NGParallaxAddToScene(back, 0, 0, 0);  // Z=0, furthest back

    // Middle layer - middle ground, medium parallax
    middle = NGParallaxCreate(
        &NGVisualAsset_middle_layer,
        NG_PARALLAX_WIDTH_INFINITE,
        0,
        FIX_FROM_FLOAT(0.5),          // Medium horizontal parallax
        FIX_FROM_FLOAT(0.5)
    );
    // Position at bottom of screen
    s16 middle_y = NG_SCENE_VIEWPORT_H - NGVisualAsset_middle_layer.height_pixels - 20;
    NGParallaxAddToScene(middle, 0, middle_y, 1);  // Z=1

    // Front layer - foreground, moves 1:1 with camera
    front = NGParallaxCreate(
        &NGVisualAsset_front_layer,
        NG_PARALLAX_WIDTH_INFINITE,
        0,
        FIX_ONE,                      // 1:1 with camera
        FIX_ONE
    );
    s16 front_y = NG_SCENE_VIEWPORT_H - NGVisualAsset_front_layer.height_pixels;
    NGParallaxAddToScene(front, 0, front_y, 2);  // Z=2, closest

    // Create menu with palette dimming
    // Uses palette fade instead of sprites, so no sprite limit issues!
    menu = NGMenuCreate(
        &ng_arena_state,
        &NGVisualAsset_ui_panel,
        &NGVisualAsset_ui_cursor,
        10  // Dim amount (0-31)
    );
    NGMenuSetTitle(menu, "SCROLL DEMO");
    NGMenuAddItem(menu, "Resume");
    NGMenuAddItem(menu, "Toggle Zoom");
    NGMenuAddItem(menu, "Reset Camera");
    NGMenuAddSeparator(menu, "--------");
    NGMenuAddItem(menu, "Ball Demo");
    NGMenuAddItem(menu, "Blank Scene");
    NGMenuSetSounds(menu, NGSFX_UI_CLICK, NGSFX_UI_SELECT);
    NGEngineSetActiveMenu(menu);

    // Display title
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 ScrollDemoUpdate(void) {
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
                case MENU_TOGGLE_ZOOM:
                    {
                        u8 target = NGCameraGetTargetZoom();
                        if (target == NG_CAM_ZOOM_100) {
                            NGCameraSetTargetZoom(NG_CAM_ZOOM_50);
                        } else {
                            NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
                        }
                    }
                    break;
                case MENU_RESET_CAMERA:
                    NGCameraSetPos(0, 0);
                    NGCameraSetZoom(NG_CAM_ZOOM_100);
                    scroll_x = 0;
                    scroll_dir = 1;
                    bob_phase = 0;
                    break;
                case MENU_BALL_DEMO:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_BALL;
                    break;
                case MENU_BLANK_SCENE:
                    NGMenuHide(menu);
                    menu_open = 0;
                    switch_target = DEMO_ID_BLANK_SCENE;
                    break;
            }
        }

        if (NGMenuCancelled(menu)) {
            NGMenuHide(menu);
            menu_open = 0;
        }
    } else {
        // Auto-scroll horizontally
        scroll_x += scroll_dir * SCROLL_SPEED;

        // Reverse direction at screen boundaries
        if (scroll_x >= SCREEN_WIDTH) {
            scroll_x = SCREEN_WIDTH;
            scroll_dir = -1;
        } else if (scroll_x <= 0) {
            scroll_x = 0;
            scroll_dir = 1;
        }

        // Vertical bobbing using simple triangle wave
        // bob_phase goes 0->255, maps to -5 to +5 pixels
        bob_phase += 2;
        s8 bob_y;
        if (bob_phase < 128) {
            // Rising: 0->127 maps to -5 to +5
            bob_y = -BOB_AMPLITUDE + ((bob_phase * BOB_AMPLITUDE * 2) >> 7);
        } else {
            // Falling: 128->255 maps to +5 to -5
            bob_y = BOB_AMPLITUDE - (((bob_phase - 128) * BOB_AMPLITUDE * 2) >> 7);
        }

        // Set camera position
        NGCameraSetPos(FIX(scroll_x), FIX(bob_y));
    }

    return switch_target;
}

void ScrollDemoCleanup(void) {
    // Clear fix layer title
    NGFixClear(0, 3, 40, 1);

    // Remove all parallax layers from scene
    NGParallaxRemoveFromScene(front);
    NGParallaxDestroy(front);

    NGParallaxRemoveFromScene(middle);
    NGParallaxDestroy(middle);

    NGParallaxRemoveFromScene(back);
    NGParallaxDestroy(back);

    NGMenuDestroy(menu);

    // Reset camera
    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
