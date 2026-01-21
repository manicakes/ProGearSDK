/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "scroll_demo.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <scene.h>
#include <backdrop.h>
#include <input.h>
#include <camera.h>
#include <arena.h>
#include <ui.h>
#include <engine.h>
#include <progear_assets.h>

#define SCROLL_SPEED  2
#define BOB_AMPLITUDE 10

typedef struct ScrollDemoState {
    Backdrop back;
    Backdrop middle;
    Backdrop front;
    MenuHandle menu;
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
    state = ARENA_ALLOC(&arena_state, ScrollDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->scroll_x = 0;
    state->scroll_dir = 1;
    state->bob_phase = 0;

    state->back = BackdropCreate(&VisualAsset_back_layer, BACKDROP_WIDTH_INFINITE, 0,
                                 FIX_FROM_FLOAT(0.25), FIX_FROM_FLOAT(0.25));
    BackdropAddToScene(state->back, 0, 0, 0);

    state->middle = BackdropCreate(&VisualAsset_middle_layer, BACKDROP_WIDTH_INFINITE, 0,
                                   FIX_FROM_FLOAT(0.5), FIX_FROM_FLOAT(0.5));
    s16 middle_y = (s16)(SCENE_VIEWPORT_H - VisualAsset_middle_layer.height_pixels - 20);
    BackdropAddToScene(state->middle, 0, middle_y, 1);

    state->front =
        BackdropCreate(&VisualAsset_front_layer, BACKDROP_WIDTH_INFINITE, 0, FIX_ONE, FIX_ONE);
    s16 front_y = (s16)(SCENE_VIEWPORT_H - VisualAsset_front_layer.height_pixels);
    BackdropAddToScene(state->front, 0, front_y, 2);

    // Menu uses palette fade, no sprite limit issues
    state->menu = MenuCreateDefault(&arena_state, 10);
    MenuSetTitle(state->menu, "SCROLL DEMO");
    MenuAddItem(state->menu, "Resume");
    MenuAddItem(state->menu, "Toggle Zoom");
    MenuAddItem(state->menu, "Reset Camera");
    MenuAddSeparator(state->menu, "--------");
    MenuAddItem(state->menu, "Ball Demo");
    MenuAddItem(state->menu, "Blank Scene");
    MenuAddItem(state->menu, "Tilemap Demo");
    MenuSetDefaultSounds(state->menu);
    EngineSetActiveMenu(state->menu);

    TextPrint(FixLayoutAlign(ALIGN_CENTER, ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 ScrollDemoUpdate(void) {
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
                case MENU_TOGGLE_ZOOM: {
                    u8 target = CameraGetTargetZoom();
                    if (target == CAM_ZOOM_100) {
                        CameraSetTargetZoom(CAM_ZOOM_50);
                    } else {
                        CameraSetTargetZoom(CAM_ZOOM_100);
                    }
                } break;
                case MENU_RESET_CAMERA:
                    CameraSetPos(0, 0);
                    CameraSetZoom(CAM_ZOOM_100);
                    state->scroll_x = 0;
                    state->scroll_dir = 1;
                    state->bob_phase = 0;
                    break;
                case MENU_BALL_DEMO:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BALL;
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
    } else {
        state->scroll_x = (s16)(state->scroll_x + state->scroll_dir * SCROLL_SPEED);

        if (state->scroll_x >= CAM_VIEWPORT_WIDTH) {
            state->scroll_x = CAM_VIEWPORT_WIDTH;
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

        CameraSetPos(FIX(state->scroll_x), FIX(bob_y));
    }

    return state->switch_target;
}

void ScrollDemoCleanup(void) {
    FixClear(0, 3, 40, 1);

    BackdropRemoveFromScene(state->front);
    BackdropDestroy(state->front);

    BackdropRemoveFromScene(state->middle);
    BackdropDestroy(state->middle);

    BackdropRemoveFromScene(state->back);
    BackdropDestroy(state->back);

    MenuDestroy(state->menu);

    CameraSetPos(0, 0);
    CameraSetZoom(CAM_ZOOM_100);
}
