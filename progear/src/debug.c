/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <debug.h>
#include <ng_hardware.h>
#include <ng_fix.h>

#include "sdk_internal.h"

static u8 g_enabled = 0;

static u16 g_frame_start_line;
static u16 g_frame_lines;      /* last completed frame */
static u16 g_peak_frame_lines; /* worst since the last reset */

static u16 g_sprites; /* last frame's busiest scanline */
static u16 g_sprite_line;
static u16 g_peak_sprites; /* worst since the last reset */
static u16 g_peak_sprite_line;

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

void NGDebugResetPeaks(void) {
    g_peak_frame_lines = 0;
    g_peak_sprites = 0;
    g_peak_sprite_line = 0;
}

void NGDebugDrawHUD(u8 row) {
    if (!g_enabled)
        return;

    /* '!' marks a budget that has been exceeded at least once, so a spike is
     * still visible after the frame that caused it has passed. */
    char over_cpu = (g_peak_frame_lines >= NG_DEBUG_FRAME_LINES) ? '!' : ' ';
    char over_spr = (g_peak_sprites > NG_DEBUG_SPRITES_PER_LINE) ? '!' : ' ';

    NGTextPrintf(NGFixLayoutXY(1, row), 0, "CPU%3d PK%3d%c SPR%3d PK%3d@%3d%c", g_frame_lines,
                 g_peak_frame_lines, over_cpu, g_sprites, g_peak_sprites, g_peak_sprite_line,
                 over_spr);
}
