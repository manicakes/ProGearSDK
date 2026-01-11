/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "blank_scene.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <input.h>
#include <arena.h>
#include <ui.h>
#include <palette.h>
#include <engine.h>
#include <progear_assets.h>

typedef struct BlankSceneState {
    NGMenuHandle menu;
    u8 menu_open;
    u8 switch_target;
} BlankSceneState;

static BlankSceneState *state;

#define MENU_RESUME       0
#define MENU_BALL_DEMO    1
#define MENU_SCROLL_DEMO  2
#define MENU_TILEMAP_DEMO 3

void BlankSceneInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, BlankSceneState);
    state->switch_target = 0;
    state->menu_open = 0;

    NGPalSetBackdrop(NG_COLOR_BLACK);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "BLANK SCENE");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuAddItem(state->menu, "Scroll Demo");
    NGMenuAddItem(state->menu, "Tilemap Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 BlankSceneUpdate(void) {
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

    return state->switch_target;
}

void BlankSceneCleanup(void) {
    NGFixClear(0, 3, 40, 1);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);
}
