/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file mvs_demo.c
 * @brief MVS hardware features showcase
 *
 * Demonstrates:
 * - DIP switch reading
 * - MVS coin counters
 * - MVS real-time clock (if available)
 * - AES vs MVS detection
 */

#include "mvs_demo.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <ng_arena.h>
#include <ng_palette.h>
#include <ng_system.h>
#include <engine.h>
#include <ui.h>
#include <progear_assets.h>

typedef struct MVSDemoState {
    NGMenuHandle menu;
    u8 menu_open;
    u8 switch_target;
    u16 frame_counter;
} MVSDemoState;

static MVSDemoState *state;

#define MENU_RESUME        0
#define MENU_PULSE_COIN1   1
#define MENU_PULSE_COIN2   2
#define MENU_BALL_DEMO     3

/* Display positions */
#define INFO_X      2
#define VALUE_X     22
#define SYSTEM_Y    4
#define DIP_Y       8
#define RTC_Y       16
#define COIN_Y      22

static void draw_static_labels(void) {
    /* Title */
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "MVS FEATURES DEMO");

    /* System info section */
    NGTextPrint(NGFixLayoutXY(INFO_X, SYSTEM_Y), 0, "SYSTEM INFO");
    NGTextPrint(NGFixLayoutXY(INFO_X, SYSTEM_Y + 1), 0, "------------");
    NGTextPrint(NGFixLayoutXY(INFO_X, SYSTEM_Y + 2), 0, "Hardware:");
    NGTextPrint(NGFixLayoutXY(INFO_X, SYSTEM_Y + 3), 0, "Country:");

    /* DIP switch section */
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y), 0, "DIP SWITCHES");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 1), 0, "------------");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 2), 0, "Raw Value:");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 3), 0, "Test Mode:");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 4), 0, "Free Play:");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 5), 0, "Cabinet:");
    NGTextPrint(NGFixLayoutXY(INFO_X, DIP_Y + 6), 0, "Multiplay:");

    /* RTC section (MVS only) */
    NGTextPrint(NGFixLayoutXY(INFO_X, RTC_Y), 0, "REAL-TIME CLOCK");
    NGTextPrint(NGFixLayoutXY(INFO_X, RTC_Y + 1), 0, "---------------");
    NGTextPrint(NGFixLayoutXY(INFO_X, RTC_Y + 2), 0, "Date:");
    NGTextPrint(NGFixLayoutXY(INFO_X, RTC_Y + 3), 0, "Time:");

    /* Coin counter section */
    NGTextPrint(NGFixLayoutXY(INFO_X, COIN_Y), 0, "COIN COUNTERS");
    NGTextPrint(NGFixLayoutXY(INFO_X, COIN_Y + 1), 0, "-------------");
    NGTextPrint(NGFixLayoutXY(INFO_X, COIN_Y + 2), 0, "Press A: Pulse P1 Counter");
    NGTextPrint(NGFixLayoutXY(INFO_X, COIN_Y + 3), 0, "Press B: Pulse P2 Counter");
}

static void update_dynamic_values(void) {
    /* System type */
    if (NGSystemIsMVS()) {
        NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 2), 0, "MVS (Arcade)");
    } else {
        NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 2), 0, "AES (Home)  ");
    }

    /* Country */
    u8 country = NGSystemGetCountry();
    switch (country) {
        case 0:
            NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 3), 0, "Japan ");
            break;
        case 1:
            NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 3), 0, "USA   ");
            break;
        case 2:
            NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 3), 0, "Europe");
            break;
        default:
            NGTextPrint(NGFixLayoutXY(VALUE_X, SYSTEM_Y + 3), 0, "???   ");
            break;
    }

    /* DIP switches */
    u8 dip_raw = NGDipReadRaw();
    NGTextPrintf(NGFixLayoutXY(VALUE_X, DIP_Y + 2), 0, "0x%02X", dip_raw);

    NGTextPrint(NGFixLayoutXY(VALUE_X, DIP_Y + 3), 0, NGDipTestMode() ? "ON " : "OFF");
    NGTextPrint(NGFixLayoutXY(VALUE_X, DIP_Y + 4), 0, NGDipFreePlay() ? "ON " : "OFF");
    NGTextPrint(NGFixLayoutXY(VALUE_X, DIP_Y + 5), 0, NGDipIsSet(NG_DIP_CABINET) ? "VS/Cocktail" : "Normal     ");
    NGTextPrint(NGFixLayoutXY(VALUE_X, DIP_Y + 6), 0, NGDipIsSet(NG_DIP_MULTIPLAY) ? "ON " : "OFF");

    /* RTC (update every second = 60 frames) */
    if (state->frame_counter % 60 == 0) {
        NGRtcTime rtc;
        if (NGRtcRead(&rtc)) {
            NGTextPrintf(NGFixLayoutXY(VALUE_X, RTC_Y + 2), 0, "20%02d-%02d-%02d", rtc.year, rtc.month, rtc.day);
            NGTextPrintf(NGFixLayoutXY(VALUE_X, RTC_Y + 3), 0, "%02d:%02d:%02d  ", rtc.hour, rtc.minute, rtc.second);
        } else {
            NGTextPrint(NGFixLayoutXY(VALUE_X, RTC_Y + 2), 0, "N/A (AES)   ");
            NGTextPrint(NGFixLayoutXY(VALUE_X, RTC_Y + 3), 0, "N/A         ");
        }
    }
}

void MVSDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, MVSDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->frame_counter = 0;

    NGPalSetBackdrop(NG_COLOR_DARK_BLUE);

    /* Draw static labels */
    draw_static_labels();

    /* Create menu */
    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "MVS DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Pulse Coin Counter 1");
    NGMenuAddItem(state->menu, "Pulse Coin Counter 2");
    NGMenuAddItem(state->menu, "Back to Ball Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);
}

static void clear_fix_content(void) {
    NGFixClear(0, 0, 40, 28);
}

static void restore_fix_content(void) {
    draw_static_labels();
    update_dynamic_values();
}

u8 MVSDemoUpdate(void) {
    state->frame_counter++;

    /* Update dynamic display values only when menu is closed */
    if (!state->menu_open) {
        update_dynamic_values();
    }

    /* Handle direct input for coin counters */
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
        NGCoinCounterP1();
    }
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_B)) {
        NGCoinCounterP2();
    }

    /* Menu handling */
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (state->menu_open) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
            restore_fix_content();
        } else {
            clear_fix_content();
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
                    restore_fix_content();
                    break;
                case MENU_PULSE_COIN1:
                    NGCoinCounterP1();
                    break;
                case MENU_PULSE_COIN2:
                    NGCoinCounterP2();
                    break;
                case MENU_BALL_DEMO:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    restore_fix_content();
                    state->switch_target = DEMO_ID_BALL;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
            restore_fix_content();
        }
    }

    return state->switch_target;
}

void MVSDemoCleanup(void) {
    /* Clear fix layer content */
    NGFixClear(0, 0, 40, 28);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);
}
