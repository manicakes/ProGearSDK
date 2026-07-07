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
 * - Heat wave (per-band sprite X displacement for flame/heat shimmer)
 * - Rain (sheared, falling texture - no per-drop sprites)
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
#include <ng_sprite.h>
#include <ng_math.h>
#include <graphic.h>
#include <engine.h>
#include <ui.h>
#include <progear_assets.h>

/* Effect modes */
typedef enum {
    EFFECT_GRADIENT_SKY,
    EFFECT_WATER_REFLECT,
    EFFECT_SCANLINE_DARK,
    EFFECT_HEAT_WAVE,
    EFFECT_RAIN,
    EFFECT_COUNT
} RasterEffect;

typedef struct RasterDemoState {
    NGMenuHandle menu;
    u8 menu_open;
    u8 switch_target;
    RasterEffect current_effect;
    u16 frame_counter;
    u8 effect_enabled;
    NGGraphic *strip; /* Full-screen checkerboard: heat wave target, rain backdrop */
    NGGraphic *rain;  /* Transparent streak overlay displaced by the rain effect */
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

/* Full-screen strips for the heat wave and rain effects. The timer
 * interrupt displaces the sprite columns of one strip horizontally in bands
 * of a few scanlines; what varies per effect is the offset formula. */
#define STRIP_WIDTH      384 /* 32px margin each side of the 320px screen */
#define STRIP_HEIGHT     224 /* Full screen height */
#define STRIP_X          -32 /* Margin keeps displacement from exposing the edge */
#define STRIP_BAND_LINES 4   /* Scanlines per displacement band */
#define STRIP_BANDS      56  /* 224 screen lines / 4 */

/* Hardware sprites the interrupt displaces (heat: checkerboard strip,
 * rain: streak overlay) */
static volatile u16 fx_sprite_first = 0;
static volatile u8 fx_sprite_count = 0;

/* Heat wave: a sine offset per band that drifts each frame makes the image
 * shimmer like air above flames. */
#define HEAT_AMPLITUDE  5 /* Max displacement in pixels */
#define HEAT_ANGLE_STEP 9 /* Wave angle advance per band (~2 waves down the screen) */
#define HEAT_PHASE_STEP 6 /* Wave angle advance per frame (shimmer speed) */

static volatile u8 heat_phase = 0;

static s16 heat_wave_offset(u8 band) {
    angle_t a = (angle_t)(heat_phase + band * HEAT_ANGLE_STEP);
    return (s16)((NGSin(a) * HEAT_AMPLITUDE) >> FIX_SHIFT);
}

/* Rain: a transparent overlay of sparse vertical dashes drawn over the
 * checkerboard backdrop. A linear X ramp per band shears the dashes into
 * diagonal streaks, while per-frame X/Y phases slide the overlay along the
 * streak direction so it appears to fall. Offsets wrap at the texture
 * repeat (32px horizontally, 64px vertically), which is seamless and stays
 * inside the strip margin. */
#define RAIN_HEIGHT 288 /* Screen + one 64px texture repeat for vertical scroll */
#define RAIN_ROWS   18  /* RAIN_HEIGHT in 16px tiles (for SCB3 writes) */
#define RAIN_SHEAR  1   /* Extra X pixels per band (slant of the streaks) */
#define RAIN_FALL   8   /* Fall speed in pixels per frame */
#define RAIN_DRIFT  2   /* X pixels per frame: RAIN_FALL * RAIN_SHEAR / STRIP_BAND_LINES */
#define RAIN_X_WRAP 31  /* X offsets wrap at the 32px horizontal repeat */
#define RAIN_Y_WRAP 63  /* Y offsets wrap at the 64px vertical repeat */

static volatile u8 rain_x_phase = 0;
static u8 rain_y_phase = 0; /* Only read by the main thread */

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

        case EFFECT_HEAT_WAVE:
            /* Shift the background strip for the band the beam is entering.
             * The X change takes effect on the following scanlines. */
            if (raster_band < STRIP_BANDS && fx_sprite_count > 0) {
                s16 x = (s16)(STRIP_X + heat_wave_offset(raster_band));
                NGSpriteXSetSpaced(fx_sprite_first, fx_sprite_count, x, 16);
                raster_band++;
                if (raster_band < STRIP_BANDS) {
                    NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
                }
            }
            break;

        case EFFECT_RAIN:
            /* Linear shear on the overlay: each band is shifted a little
             * further right, turning vertical dashes into diagonal streaks */
            if (raster_band < STRIP_BANDS && fx_sprite_count > 0) {
                s16 x = (s16)(STRIP_X + ((rain_x_phase + raster_band * RAIN_SHEAR) & RAIN_X_WRAP));
                NGSpriteXSetSpaced(fx_sprite_first, fx_sprite_count, x, 16);
                raster_band++;
                if (raster_band < STRIP_BANDS) {
                    NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
                }
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
        case EFFECT_HEAT_WAVE:
            /* Strip covers the whole screen; bands start immediately */
            NGGraphicSetSource(state->strip, &NGVisualAsset_checkerboard, NGPAL_CHECKERBOARD);
            NGGraphicSetVisible(state->strip, 1);
            NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
            break;
        case EFFECT_RAIN:
            /* Storm-recolored checkerboard as a static backdrop, with the
             * transparent streak overlay sheared on top */
            NGGraphicSetSource(state->strip, &NGVisualAsset_checkerboard, NGPAL_CHECKERBOARD_RAIN);
            NGGraphicSetVisible(state->strip, 1);
            NGGraphicSetVisible(state->rain, 1);
            NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
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

    NGGraphicSetVisible(state->strip, 0);
    NGGraphicSetVisible(state->rain, 0);
    fx_sprite_count = 0;

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
        case EFFECT_HEAT_WAVE:
            return "Heat Wave      ";
        case EFFECT_RAIN:
            return "Rain           ";
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

    /* Full-screen checkerboard: displaced by the heat wave effect and used
     * as a static backdrop under the rain. Kept hidden until an effect is
     * enabled; one texture repeat wider than the screen so displacement
     * never exposes the backdrop at the edges. */
    NGGraphicConfig strip_cfg = {
        .width = STRIP_WIDTH,
        .height = STRIP_HEIGHT,
        .tile_mode = NG_GRAPHIC_TILE_REPEAT,
        .layer = NG_GRAPHIC_LAYER_BACKGROUND,
        .z_order = 0,
    };
    state->strip = NGGraphicCreate(&strip_cfg);
    NGGraphicSetSource(state->strip, &NGVisualAsset_checkerboard,
                       NGVisualAsset_checkerboard.palette);
    NGGraphicSetPosition(state->strip, STRIP_X, 0);
    NGGraphicSetVisible(state->strip, 0);

    /* Transparent rain streak overlay, one texture repeat taller than the
     * screen so the vertical scroll wrap is hidden */
    NGGraphicConfig rain_cfg = {
        .width = STRIP_WIDTH,
        .height = RAIN_HEIGHT,
        .tile_mode = NG_GRAPHIC_TILE_REPEAT,
        .layer = NG_GRAPHIC_LAYER_FOREGROUND,
        .z_order = 0,
    };
    state->rain = NGGraphicCreate(&rain_cfg);
    NGGraphicSetSource(state->rain, &NGVisualAsset_rain_streaks, NGPAL_RAIN_STREAKS);
    NGGraphicSetPosition(state->rain, STRIP_X, -64);
    NGGraphicSetVisible(state->rain, 0);

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
            case EFFECT_HEAT_WAVE: {
                heat_phase = (u8)(heat_phase + HEAT_PHASE_STEP);

                /* Re-query the sprite range - the graphic system may
                 * reallocate sprites whenever visible graphics change. */
                u16 first;
                u8 count;
                if (NGGraphicGetSpriteRange(state->strip, &first, &count)) {
                    fx_sprite_first = first;
                    fx_sprite_count = count;
                    /* Apply band 0 now (during VBlank); the first interrupt
                     * fires at the band 1 boundary */
                    NGSpriteXSetSpaced(first, count, (s16)(STRIP_X + heat_wave_offset(0)), 16);
                } else {
                    fx_sprite_count = 0; /* Not allocated until first draw */
                }
                raster_band = 1;
                NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
            } break;
            case EFFECT_RAIN: {
                /* Slide the overlay along the streak direction: down by the
                 * fall speed, right by the matching shear drift */
                rain_x_phase = (u8)((rain_x_phase + RAIN_DRIFT) & RAIN_X_WRAP);
                rain_y_phase = (u8)((rain_y_phase + RAIN_FALL) & RAIN_Y_WRAP);

                u16 first;
                u8 count;
                if (NGGraphicGetSpriteRange(state->rain, &first, &count)) {
                    fx_sprite_first = first;
                    fx_sprite_count = count;
                    /* Vertical scroll within one texture repeat (SCB3), then
                     * band 0 X; the first interrupt fires at band 1 */
                    NGSpriteYSetUniform(first, count, (s16)(rain_y_phase - 64), RAIN_ROWS);
                    NGSpriteXSetSpaced(first, count, (s16)(STRIP_X + rain_x_phase), 16);
                } else {
                    fx_sprite_count = 0; /* Not allocated until first draw */
                }
                raster_band = 1;
                NGTimerSetReload(NGTimerScanlineToReload(STRIP_BAND_LINES));
            } break;
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

    NGGraphicDestroy(state->rain);
    NGGraphicDestroy(state->strip);

    /* Clear fix layer */
    NGFixClear(0, 0, 40, 28);

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);
}
