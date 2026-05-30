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

static NGMenuHandle g_active_menu = 0;

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
}

void NGEngineFrameStart(void) {
    NGWaitVBlank();
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
}

void NGEngineSetActiveMenu(NGMenuHandle menu) {
    g_active_menu = menu;
}

NGMenuHandle NGEngineGetActiveMenu(void) {
    return g_active_menu;
}
