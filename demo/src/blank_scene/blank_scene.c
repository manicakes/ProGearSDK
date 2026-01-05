/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// blank_scene.c - Blank scene demo implementation

#include "blank_scene.h"
#include "../demo_ids.h"
#include <neogeo.h>
#include <fix.h>
#include <input.h>
#include <arena.h>
#include <ui.h>
#include <palette.h>
#include <engine.h>
#include <ngres_generated_assets.h>

// Demo state
static NGMenuHandle menu;
static u8 menu_open;
static u8 switch_target;  // 0 = none, 1 = ball, 2 = scroll

// Menu item indices
#define MENU_RESUME       0
#define MENU_BALL_DEMO    1
#define MENU_SCROLL_DEMO  2
#define MENU_TILEMAP_DEMO 3

void BlankSceneInit(void) {
    switch_target = 0;
    menu_open = 0;

    // Set background color to black
    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Create menu
    menu = NGMenuCreate(
        &ng_arena_state,
        &NGVisualAsset_ui_panel,
        &NGVisualAsset_ui_cursor,
        10  // Dim amount
    );
    NGMenuSetTitle(menu, "BLANK SCENE");
    NGMenuAddItem(menu, "Resume");
    NGMenuAddItem(menu, "Ball Demo");
    NGMenuAddItem(menu, "Scroll Demo");
    NGMenuAddItem(menu, "Tilemap Demo");
    NGMenuSetSounds(menu, NGSFX_UI_CLICK, NGSFX_UI_SELECT);
    NGEngineSetActiveMenu(menu);

    // Display title
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PRESS START FOR MENU");
}

u8 BlankSceneUpdate(void) {
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

    return switch_target;
}

void BlankSceneCleanup(void) {
    // Clear fix layer title
    NGFixClear(0, 3, 40, 1);

    NGMenuDestroy(menu);

    // Reset backdrop color to black
    NGPalSetBackdrop(NG_COLOR_BLACK);
}
