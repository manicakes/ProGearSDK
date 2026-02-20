/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file raster_demo.c
 * @brief Raster effects showcase
 *
 * Demonstrates timer interrupts for mid-frame effects:
 * - Gradient sky effect (palette change per scanline band)
 * - Water reflection (palette swap on lower half)
 * - Scanline counter display
 */

#include "raster_demo.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <ng_arena.h>
#include <ng_palette.h>
#include <ng_color.h>
#include <ng_interrupt.h>
#include <engine.h>
#include <ui.h>
#include <progear_assets.h>

/* Effect modes */
typedef enum {
    EFFECT_GRADIENT_SKY,
    EFFECT_WATER_REFLECT,
    EFFECT_SCANLINE_DARK,
    EFFECT_COUNT
} RasterEffect;

typedef struct RasterDemoState {
    NGMenuHandle menu;
    u8 menu_open;
    u8 switch_target;
    RasterEffect current_effect;
    u16 frame_counter;
    u8 effect_enabled;
} RasterDemoState;

static RasterDemoState *state;

#define MENU_RESUME      0
#define MENU_TOGGLE_FX   1
#define MENU_NEXT_EFFECT 2
#define MENU_BALL_DEMO   3

/* Scanlines per band for gradient (224 / 8 = 28 scanlines per band) */
#define SCANLINES_PER_BAND 28

/* Raster interrupt state */
static volatile u8 raster_band = 0;
static volatile u8 water_line = 112; /* Current water line position */
static volatile u8 anim_offset = 0;  /* Animation offset for gradient */

/* Sky gradient colors (8 bands from dark blue to light) */
static const u16 sky_gradient[8] = {
    0x0001, /* Very dark blue */
    0x0012, /* Dark blue */
    0x0023, /* Blue */
    0x0034, /* Medium blue */
    0x0145, /* Light blue */
    0x0256, /* Lighter blue */
    0x0367, /* Very light blue */
    0x0478, /* Near cyan */
};

/**
 * Timer interrupt handler - called at configured scanline
 */
static void raster_interrupt_handler(void) {
    volatile u16 *backdrop = (volatile u16 *)0x401FFE;

    switch (state->current_effect) {
        case EFFECT_GRADIENT_SKY:
            /* Change backdrop color for each band with animation offset */
            if (raster_band < 8) {
                u8 color_idx = (u8)((raster_band + anim_offset) & 7);
                *backdrop = sky_gradient[color_idx];
                raster_band++;
                /* Set timer for next band (28 scanlines each) */
                if (raster_band < 8) {
                    NGTimerSetReload(NGTimerScanlineToReload(28));
                }
            }
            break;

        case EFFECT_WATER_REFLECT:
            /* Water effect - darker blue for "water" below the line */
            *backdrop = 0x0023; /* Dark blue for water */
            break;

        case EFFECT_SCANLINE_DARK:
            /* CRT scanline effect - every other pair of lines is darker */
            if ((raster_band + (anim_offset >> 2)) & 1) {
                *backdrop = 0x0222; /* Dark */
            } else {
                *backdrop = 0x0666; /* Light */
            }
            raster_band++;
            /* Set timer for next band (4 scanlines for tighter effect) */
            if (raster_band < 56) { /* 224 / 4 = 56 bands */
                NGTimerSetReload(NGTimerScanlineToReload(4));
            }
            break;

        default:
            break;
    }
}

static void enable_raster_effect(void) {
    /* Reset raster state */
    raster_band = 0;

    /* Set up timer interrupt handler */
    NGInterruptSetTimerHandler(raster_interrupt_handler);

    /* Configure initial timer based on effect */
    switch (state->current_effect) {
        case EFFECT_GRADIENT_SKY:
            /* First interrupt after 28 scanlines */
            NGTimerSetReload(NGTimerScanlineToReload(28));
            break;
        case EFFECT_WATER_REFLECT:
            /* Single interrupt at middle of screen (112 scanlines) */
            NGTimerSetReload(NGTimerScanlineToReload(112));
            break;
        case EFFECT_SCANLINE_DARK:
            /* First interrupt after 8 scanlines */
            NGTimerSetReload(NGTimerScanlineToReload(8));
            break;
        default:
            NGTimerSetReload(NGTimerScanlineToReload(112));
            break;
    }

    NGTimerEnable();
    state->effect_enabled = 1;
}

static void disable_raster_effect(void) {
    NGTimerDisable();
    NGInterruptSetTimerHandler(0);
    state->effect_enabled = 0;

    /* Reset backdrop to solid color */
    NG_REG_BACKDROP = NG_COLOR_BLACK;
}

static const char *get_effect_name(RasterEffect effect) {
    switch (effect) {
        case EFFECT_GRADIENT_SKY:
            return "Gradient Sky   ";
        case EFFECT_WATER_REFLECT:
            return "Water Reflect  ";
        case EFFECT_SCANLINE_DARK:
            return "Scanline Dark  ";
        default:
            return "Unknown        ";
    }
}

static void draw_info(void) {
    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "RASTER EFFECTS DEMO");

    NGTextPrint(NGFixLayoutXY(2, 4), 0, "RASTER INTERRUPTS");
    NGTextPrint(NGFixLayoutXY(2, 5), 0, "-----------------");
    NGTextPrint(NGFixLayoutXY(2, 7), 0, "Effect:");
    NGTextPrint(NGFixLayoutXY(2, 8), 0, "Status:");
    NGTextPrint(NGFixLayoutXY(2, 10), 0, "Timer interrupts allow");
    NGTextPrint(NGFixLayoutXY(2, 11), 0, "mid-frame register changes");
    NGTextPrint(NGFixLayoutXY(2, 12), 0, "for palette/scroll effects.");

    NGTextPrint(NGFixLayoutXY(2, 14), 0, "Press A to toggle effect");
    NGTextPrint(NGFixLayoutXY(2, 15), 0, "Press B for next effect");
}

static void update_info(void) {
    NGTextPrint(NGFixLayoutXY(12, 7), 0, get_effect_name(state->current_effect));
    NGTextPrint(NGFixLayoutXY(12, 8), 0, state->effect_enabled ? "ENABLED " : "DISABLED");
}

static void clear_fix_content(void) {
    NGFixClear(0, 0, 40, 28);
}

static void restore_fix_content(void) {
    draw_info();
    update_info();
}

void RasterDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, RasterDemoState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->current_effect = EFFECT_GRADIENT_SKY;
    state->frame_counter = 0;
    state->effect_enabled = 0;

    NGPalSetBackdrop(NG_COLOR_BLACK);

    draw_info();

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "RASTER DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Toggle Effect");
    NGMenuAddItem(state->menu, "Next Effect");
    NGMenuAddItem(state->menu, "Back to Ball Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);
}

u8 RasterDemoUpdate(void) {
    state->frame_counter++;

    /* Set up raster effect at start of each frame */
    if (state->effect_enabled) {
        /* Update animation every few frames */
        if ((state->frame_counter & 7) == 0) {
            anim_offset++;
        }

        /* Animate water line (oscillate between 80 and 144) */
        if (state->current_effect == EFFECT_WATER_REFLECT) {
            /* Simple sine-like oscillation using frame counter */
            s16 wave = (s16)(state->frame_counter & 63);
            if (wave > 31)
                wave = (s16)(63 - wave);
            water_line = (u8)(96 + wave); /* Oscillates 96-127 */
        }

        /* Reset raster band counter */
        raster_band = 0;

        /* Set initial backdrop color based on effect */
        switch (state->current_effect) {
            case EFFECT_GRADIENT_SKY:
                NG_REG_BACKDROP = sky_gradient[anim_offset & 7];
                raster_band = 1; /* First interrupt will set band 1 */
                NGTimerSetReload(NGTimerScanlineToReload(28));
                break;
            case EFFECT_WATER_REFLECT:
                NG_REG_BACKDROP = 0x0478; /* Light cyan sky */
                NGTimerSetReload(NGTimerScanlineToReload(water_line));
                break;
            case EFFECT_SCANLINE_DARK:
                NG_REG_BACKDROP = 0x0666; /* Light gray */
                NGTimerSetReload(NGTimerScanlineToReload(4));
                break;
            default:
                NG_REG_BACKDROP = NG_COLOR_BLACK;
                break;
        }
    }

    /* Update info display only when menu is closed */
    if (!state->menu_open) {
        update_info();
    }

    /* Direct button input */
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
        if (state->effect_enabled) {
            disable_raster_effect();
        } else {
            enable_raster_effect();
        }
    }

    if (NGInputPressed(NG_PLAYER_1, NG_BTN_B)) {
        u8 was_enabled = state->effect_enabled;
        if (was_enabled) {
            disable_raster_effect();
        }
        state->current_effect = (RasterEffect)((state->current_effect + 1) % EFFECT_COUNT);
        if (was_enabled) {
            enable_raster_effect();
        }
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
                case MENU_TOGGLE_FX:
                    if (state->effect_enabled) {
                        disable_raster_effect();
                    } else {
                        enable_raster_effect();
                    }
                    break;
                case MENU_NEXT_EFFECT: {
                    u8 was_enabled = state->effect_enabled;
                    if (was_enabled) {
                        disable_raster_effect();
                    }
                    state->current_effect =
                        (RasterEffect)((state->current_effect + 1) % EFFECT_COUNT);
                    if (was_enabled) {
                        enable_raster_effect();
                    }
                } break;
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

void RasterDemoCleanup(void) {
    /* Disable raster effects */
    disable_raster_effect();

    /* Clear fix layer */
    NGFixClear(0, 0, 40, 28);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);
}
