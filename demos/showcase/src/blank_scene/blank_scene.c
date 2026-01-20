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
    MenuHandle menu;
    u8 menu_open;
    u8 switch_target;
} BlankSceneState;

static BlankSceneState *state;

#define MENU_RESUME       0
#define MENU_BALL_DEMO    1
#define MENU_SCROLL_DEMO  2
#define MENU_TILEMAP_DEMO 3

void BlankSceneInit(void) {
    state = ARENA_ALLOC(&arena_state, BlankSceneState);
    state->switch_target = 0;
    state->menu_open = 0;

    PalSetBackdrop(COLOR_BLACK);

    state->menu = MenuCreateDefault(&arena_state, 10);
    MenuSetTitle(state->menu, "BLANK SCENE");
    MenuAddItem(state->menu, "Resume");
    MenuAddItem(state->menu, "Ball Demo");
    MenuAddItem(state->menu, "Scroll Demo");
    MenuAddItem(state->menu, "Tilemap Demo");
    MenuSetDefaultSounds(state->menu);
    EngineSetActiveMenu(state->menu);

    TextPrint(FixLayoutAlign(ALIGN_CENTER, ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 BlankSceneUpdate(void) {
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
                case MENU_BALL_DEMO:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BALL;
                    break;
                case MENU_SCROLL_DEMO:
                    MenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_SCROLL;
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
    }

    return state->switch_target;
}

void BlankSceneCleanup(void) {
    FixClear(0, 3, 40, 1);

    MenuDestroy(state->menu);

    PalSetBackdrop(COLOR_BLACK);
}
