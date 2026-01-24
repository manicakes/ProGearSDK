/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "scroll_demo.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <scene.h>
#include <backdrop.h>
#include <ng_input.h>
#include <camera.h>
#include <ng_arena.h>
#include <ui.h>
#include <engine.h>
#include <progear_assets.h>

#define SCROLL_SPEED  2
#define BOB_AMPLITUDE 10

typedef struct ScrollDemoState {
    NGBackdropHandle back;
    NGBackdropHandle middle;
    NGBackdropHandle front;
    NGMenuHandle menu;
    s16 scroll_x;
    s8 scroll_dir;
    u8 bob_phase;
    u8 menu_open;
    u8 switch_target;
} ScrollDemoState;

static ScrollDemoState *state;

#define MENU_RESUME       0
#define MENU_TOGGLE_ZOOM  1
#define MENU_RESET_CAMERA 2
// Index 3 is separator
#define MENU_BALL_DEMO    4
#define MENU_BLANK_SCENE  5
#define MENU_TILEMAP_DEMO 6

void ScrollDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, ScrollDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->scroll_x = 0;
    state->scroll_dir = 1;
    state->bob_phase = 0;

    state->back = NGBackdropCreate(&NGVisualAsset_back_layer, NG_BACKDROP_WIDTH_INFINITE, 0,
                                   FIX_FROM_FLOAT(0.25), FIX_FROM_FLOAT(0.25));
    NGBackdropAddToScene(state->back, 0, 0, 0);

    state->middle = NGBackdropCreate(&NGVisualAsset_middle_layer, NG_BACKDROP_WIDTH_INFINITE, 0,
                                     FIX_FROM_FLOAT(0.5), FIX_FROM_FLOAT(0.5));
    s16 middle_y = (s16)(NG_SCENE_VIEWPORT_H - NGVisualAsset_middle_layer.height_pixels - 20);
    NGBackdropAddToScene(state->middle, 0, middle_y, 1);

    state->front = NGBackdropCreate(&NGVisualAsset_front_layer, NG_BACKDROP_WIDTH_INFINITE, 0,
                                    FIX_ONE, FIX_ONE);
    s16 front_y = (s16)(NG_SCENE_VIEWPORT_H - NGVisualAsset_front_layer.height_pixels);
    NGBackdropAddToScene(state->front, 0, front_y, 2);

    // Menu uses palette fade, no sprite limit issues
    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "SCROLL DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Toggle Zoom");
    NGMenuAddItem(state->menu, "Reset Camera");
    NGMenuAddSeparator(state->menu, "--------");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuAddItem(state->menu, "Blank Scene");
    NGMenuAddItem(state->menu, "Tilemap Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 ScrollDemoUpdate(void) {
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
                case MENU_TOGGLE_ZOOM: {
                    u8 target = NGCameraGetTargetZoom();
                    if (target == NG_CAM_ZOOM_100) {
                        NGCameraSetTargetZoom(NG_CAM_ZOOM_50);
                    } else {
                        NGCameraSetTargetZoom(NG_CAM_ZOOM_100);
                    }
                } break;
                case MENU_RESET_CAMERA:
                    NGCameraSetPos(0, 0);
                    NGCameraSetZoom(NG_CAM_ZOOM_100);
                    state->scroll_x = 0;
                    state->scroll_dir = 1;
                    state->bob_phase = 0;
                    break;
                case MENU_BALL_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BALL;
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
    } else {
        state->scroll_x = (s16)(state->scroll_x + state->scroll_dir * SCROLL_SPEED);

        if (state->scroll_x >= SCREEN_WIDTH) {
            state->scroll_x = SCREEN_WIDTH;
            state->scroll_dir = -1;
        } else if (state->scroll_x <= 0) {
            state->scroll_x = 0;
            state->scroll_dir = 1;
        }

        // Triangle wave: bob_phase 0->255 maps to -BOB_AMPLITUDE to +BOB_AMPLITUDE
        state->bob_phase += 2;
        s8 bob_y;
        if (state->bob_phase < 128) {
            bob_y = (s8)(-BOB_AMPLITUDE + ((state->bob_phase * BOB_AMPLITUDE * 2) >> 7));
        } else {
            bob_y = (s8)(BOB_AMPLITUDE - (((state->bob_phase - 128) * BOB_AMPLITUDE * 2) >> 7));
        }

        NGCameraSetPos(FIX(state->scroll_x), FIX(bob_y));
    }

    return state->switch_target;
}

void ScrollDemoCleanup(void) {
    NGFixClear(0, 3, 40, 1);

    NGBackdropRemoveFromScene(state->front);
    NGBackdropDestroy(state->front);

    NGBackdropRemoveFromScene(state->middle);
    NGBackdropDestroy(state->middle);

    NGBackdropRemoveFromScene(state->back);
    NGBackdropDestroy(state->back);

    NGMenuDestroy(state->menu);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
