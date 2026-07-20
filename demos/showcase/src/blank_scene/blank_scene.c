/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "blank_scene.h"
#include "../demo_ids.h"
#include <scenemenu.h>
#include <ng_hardware.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <ng_arena.h>
#include <ui.h>
#include <ng_palette.h>
#include <engine.h>
#include <progear_assets.h>

typedef struct BlankSceneState {
    NGMenuHandle menu;
    NGSceneMenu scene_menu;
    u8 menu_open;
    u8 switch_target;
} BlankSceneState;

static BlankSceneState *state;

#define ACT_RESUME 0

void BlankSceneInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, BlankSceneState);
    state->switch_target = 0;
    state->menu_open = 0;

    NGPalSetBackdrop(NG_COLOR_BLACK);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGSceneMenuInit(&state->scene_menu, state->menu, "BLANK SCENE");
    NGSceneMenuAddAction(&state->scene_menu, ACT_RESUME, "Resume");
    NGSceneMenuBuild(&state->scene_menu);
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
            NGSceneMenuReset(&state->scene_menu);
            NGMenuShow(state->menu);
            state->menu_open = 1;
        }
    }

    NGMenuUpdate(state->menu);

    if (state->menu_open) {
        NGSceneMenuEvent ev = NGSceneMenuUpdate(&state->scene_menu);

        if (ev.kind == NG_SCENE_MENU_SWITCH) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
            state->switch_target = ev.scene;
        } else if (ev.kind == NG_SCENE_MENU_CLOSED) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        } else if (ev.kind == NG_SCENE_MENU_ACTION) {
            switch (ev.action) {
                case ACT_RESUME:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    break;
            }
        }
    }

    return state->switch_target;
}

void BlankSceneCleanup(void) {
    NGFixClear(0, 3, 40, 1);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);
}
