/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <engine.h>
#include <ng_hardware.h>
#include <ng_arena.h>
#include <ng_palette.h>
#include <ng_fix.h>
#include <scene.h>
#include <camera.h>
#include <ng_input.h>
#include <ng_audio.h>
#include <ui.h>
#include <lighting.h>
#include <ng_interrupt.h>
#include "sdk_internal.h"

static NGMenuHandle g_active_menu = 0;
static u8 g_paused = 0;
static u8 g_paused_timer_was_enabled = 0;

// Weak default - games using progear_assets.py provide a strong definition that loads palette data
__attribute__((weak)) void NGPalInitAssets(void) {}

void NGEngineInit(void) {
    NGArenaSystemInit();
    NGPalInitDefault();
    NGTextSetFont(768); // Use game font at tile 768+ (BIOS uses 0-767)
    NGFixClearAll();
    NGSceneInit();
    NGCameraInit();
    NGInputInit();
    NGAudioInit();
    NGLightingInit();
    NGPalInitAssets();
    NGPalSetBackdrop(NG_COLOR_BLACK);
    g_active_menu = 0;
    g_paused = 0;
    g_paused_timer_was_enabled = 0;
}

void NGEnginePause(void) {
    if (g_paused)
        return;
    g_paused = 1;

    /* Stop raster effects. Their handlers reprogram VRAMADDR/VRAMMOD, so
     * leaving them armed would corrupt the menu's fix-layer and sprite writes
     * as well as keeping the effect animating. */
    g_paused_timer_was_enabled = NGTimerIsEnabled();
    if (g_paused_timer_was_enabled) {
        NGTimerDisable();
    }

    _NGGraphicApplyPause(1);
}

void NGEngineResume(void) {
    if (!g_paused)
        return;
    g_paused = 0;

    _NGGraphicApplyPause(0);

    if (g_paused_timer_was_enabled) {
        NGTimerEnable();
        g_paused_timer_was_enabled = 0;
    }
}

u8 NGEngineIsPaused(void) {
    return g_paused;
}

void NGEngineFrameStart(void) {
    NGWaitVBlank();
    _NGDebugFrameStart();
    NGWatchdogKick();

    // VBlank is the only safe window to touch VRAM: the LSPC is not scanning
    // sprite or fix-layer RAM. Commit the frame that was computed during the
    // previous iteration now, before any game logic runs.
    if (g_active_menu && NGMenuNeedsDraw(g_active_menu)) {
        NGMenuDraw(g_active_menu);
    }
    NGSceneDraw();

    NGArenaReset(&ng_arena_frame);
    NGInputUpdate();
}

void NGEngineFrameEnd(void) {
    // Advance the simulation and fold the result into the scene graphics. This
    // is all computation - the sprite VRAM commit happens at the next
    // NGEngineFrameStart() during VBlank, trading one frame of latency for
    // tear-free output.
    NGLightingUpdate();
    NGSceneUpdate();

    _NGDebugFrameEnd();
}

void NGEngineSetActiveMenu(NGMenuHandle menu) {
    g_active_menu = menu;
}

NGMenuHandle NGEngineGetActiveMenu(void) {
    return g_active_menu;
}
