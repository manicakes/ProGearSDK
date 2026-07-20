/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <debug.h>
#include <ng_hardware.h>
#include <ng_fix.h>
#include <camera.h>
#include <hitbox.h>
#include <ng_math.h>

#include "sdk_internal.h"

static u8 g_enabled = 0;

static u16 g_frame_start_line;
static u16 g_frame_lines;      /* last completed frame */
static u16 g_peak_frame_lines; /* worst since the last reset */

static u16 g_sprites; /* last frame's busiest scanline */
static u16 g_sprite_line;
static u16 g_peak_sprites; /* worst since the last reset */
static u16 g_peak_sprite_line;

static u8 g_overran;        /* did the last frame miss its VBlank? */
static u16 g_overrun_count; /* how many have since the last reset */

/*
 * The VBlank ISR sets this flag and NGWaitVBlank clears it, so it is clear
 * for the whole of a frame's work. Finding it set at the end of that work
 * means the next VBlank already arrived and the frame is late.
 *
 * This matters because the scanline measurement saturates: a frame that runs
 * over wraps and reads just under the limit, indistinguishable from one that
 * fits. The flag answers the question directly.
 */
#define NG_VBLANK_FLAG (*(volatile u8 *)0x10FD8E)

/*
 * The LSPC reports the scanline being drawn in the top nine bits of its mode
 * register. Reading it twice a frame turns it into a clock: the difference is
 * how many scanlines of real time the work in between consumed, which is what
 * a frame budget actually means on this hardware. No instruction counting and
 * no emulator assistance, so it measures the real machine.
 */
static u16 raster_line(void) {
    return (u16)((NG_REG_LSPCMODE >> 7) & 0x1FF);
}

void NGDebugSetEnabled(u8 enabled) {
    g_enabled = enabled ? 1 : 0;
    if (!g_enabled) {
        g_frame_lines = 0;
        g_sprites = 0;
    }
}

u8 NGDebugIsEnabled(void) {
    return g_enabled;
}

void _NGDebugFrameStart(void) {
    if (!g_enabled)
        return;
    g_frame_start_line = raster_line();
}

void _NGDebugFrameEnd(void) {
    if (!g_enabled)
        return;

    /* The counter is offset by 0xF8 and cycles once per frame, so raw readings
     * stay congruent modulo the frame length across the wrap (511 + 1 = 512,
     * and 512 mod 264 = 248, the first value again). Differencing modulo the
     * frame length is therefore correct without unpicking the offset.
     *
     * Note this saturates: work exceeding a whole frame wraps and is
     * under-reported, so treat a reading near NG_DEBUG_FRAME_LINES as "at or
     * over budget" rather than as an exact figure. */
    u16 now = raster_line();
    g_frame_lines = (u16)((now + NG_DEBUG_FRAME_LINES - g_frame_start_line) % NG_DEBUG_FRAME_LINES);
    if (g_frame_lines > g_peak_frame_lines) {
        g_peak_frame_lines = g_frame_lines;
    }

    g_overran = NG_VBLANK_FLAG ? 1 : 0;
    if (g_overran) {
        g_overrun_count++;
    }

    _NGGraphicPeakSpriteLoad(&g_sprites, &g_sprite_line);
    if (g_sprites > g_peak_sprites) {
        g_peak_sprites = g_sprites;
        g_peak_sprite_line = g_sprite_line;
    }
}

u16 NGDebugFrameLines(void) {
    return g_frame_lines;
}

u16 NGDebugPeakFrameLines(void) {
    return g_peak_frame_lines;
}

u16 NGDebugPeakSpritesPerLine(void) {
    return g_sprites;
}

u16 NGDebugPeakSpriteLine(void) {
    return g_sprite_line;
}

u8 NGDebugSpritesOverBudget(void) {
    return (g_sprites > NG_DEBUG_SPRITES_PER_LINE) ? 1 : 0;
}

/* ============================================================
 * Box overlay
 * ============================================================ */

/* Cells written last call, so they can be erased without disturbing anything
 * else on the fix layer. Sized for a busy scene: a 32x32 box is 12 outline
 * cells, so this covers roughly 30 boxes. */
#define DEBUG_MAX_BOX_CELLS 384

static u16 g_cells[DEBUG_MAX_BOX_CELLS];
static u16 g_cell_count;
static u8 g_box_pal[8]; /* indexed by NGBoxKind bit position */

static void put_cell(u8 cx, u8 cy, u8 pal, const char *glyph) {
    if (cx >= NG_FIX_WIDTH || cy < NG_FIX_VISIBLE_TOP || cy > NG_FIX_VISIBLE_BOTTOM)
        return;
    if (g_cell_count >= DEBUG_MAX_BOX_CELLS)
        return;

    NGTextPrint(NGFixLayoutXY(cx, cy), pal, glyph);
    g_cells[g_cell_count++] = (u16)(((u16)cx << 8) | cy);
}

void NGDebugClearBoxes(void) {
    for (u16 i = 0; i < g_cell_count; i++) {
        u8 cx = (u8)(g_cells[i] >> 8);
        u8 cy = (u8)(g_cells[i] & 0xFF);
        NGFixClear(cx, cy, 1, 1);
    }
    g_cell_count = 0;
}

void NGDebugSetBoxPalette(u8 kind, u8 palette) {
    for (u8 bit = 0; bit < 8; bit++) {
        if (kind & (u8)(1 << bit)) {
            g_box_pal[bit] = palette;
        }
    }
}

/* Outline one screen-space rectangle, snapped outward to whole cells. */
static void draw_box_outline(s16 x, s16 y, u16 w, u16 h, u8 pal) {
    if (w == 0 || h == 0)
        return;

    /* Snap outward: a box covering any part of a cell lights that cell, so an
     * outline never reads as smaller than the box actually is. */
    s16 cx0 = (s16)(x >> 3);
    s16 cy0 = (s16)((y >> 3) + NG_FIX_VISIBLE_TOP);
    s16 cx1 = (s16)((x + (s16)w - 1) >> 3);
    s16 cy1 = (s16)(((y + (s16)h - 1) >> 3) + NG_FIX_VISIBLE_TOP);

    if (cx1 < 0 || cy1 < NG_FIX_VISIBLE_TOP || cx0 >= NG_FIX_WIDTH || cy0 > NG_FIX_VISIBLE_BOTTOM)
        return;
    if (cx0 < 0)
        cx0 = 0;
    if (cy0 < NG_FIX_VISIBLE_TOP)
        cy0 = NG_FIX_VISIBLE_TOP;

    for (s16 cx = cx0; cx <= cx1; cx++) {
        u8 top_edge = (cx == cx0 || cx == cx1) ? 1 : 0;
        put_cell((u8)cx, (u8)cy0, pal, top_edge ? "+" : "-");
        if (cy1 != cy0) {
            put_cell((u8)cx, (u8)cy1, pal, top_edge ? "+" : "-");
        }
    }
    for (s16 cy = (s16)(cy0 + 1); cy < cy1; cy++) {
        put_cell((u8)cx0, (u8)cy, pal, "|");
        if (cx1 != cx0) {
            put_cell((u8)cx1, (u8)cy, pal, "|");
        }
    }
}

void NGDebugDrawBoxes(NGActorHandle actor, u8 kinds) {
    if (!g_enabled)
        return;

    /* One kind at a time so each keeps its own colour. */
    for (u8 bit = 0; bit < 8; bit++) {
        u8 kind = (u8)(1 << bit);
        if (!(kinds & kind))
            continue;

        NGRect box;
        for (u8 i = 0; NGActorGetBox(actor, kind, i, &box); i++) {
            /* Boxes resolve to scene coordinates; the fix layer is screen
             * space, so the camera transform has to be applied. */
            s16 sx, sy;
            NGCameraWorldToScreen(FIX(box.x), FIX(box.y), &sx, &sy);
            draw_box_outline(sx, sy, box.w, box.h, g_box_pal[bit]);
        }
    }
}

u8 NGDebugFrameOverran(void) {
    return g_overran;
}

u16 NGDebugOverrunCount(void) {
    return g_overrun_count;
}

void NGDebugResetPeaks(void) {
    g_overrun_count = 0;
    g_overran = 0;
    g_peak_frame_lines = 0;
    g_peak_sprites = 0;
    g_peak_sprite_line = 0;
}

void NGDebugDrawHUD(u8 row) {
    if (!g_enabled)
        return;

    /* '!' marks a budget that has been exceeded at least once, so a spike is
     * still visible after the frame that caused it has passed. */
    char over_cpu = g_overrun_count ? '!' : ' ';
    char over_spr = (g_peak_sprites > NG_DEBUG_SPRITES_PER_LINE) ? '!' : ' ';

    NGTextPrintf(NGFixLayoutXY(1, row), 0, "CPU%3d PK%3d%c%d SPR%3d PK%3d@%3d%c", g_frame_lines,
                 g_peak_frame_lines, over_cpu, g_overrun_count, g_sprites, g_peak_sprites,
                 g_peak_sprite_line, over_spr);
}
