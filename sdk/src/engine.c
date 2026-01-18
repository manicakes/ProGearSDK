/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <engine.h>
#include <neogeo.h>
#include <arena.h>
#include <palette.h>
#include <fix.h>
#include <scene.h>
#include <camera.h>
#include <input.h>
#include <audio.h>
#include <ui.h>
#include <lighting.h>

static NGMenuHandle g_active_menu = 0;

// Weak default - games using progear_assets.py provide a strong definition that loads palette data
__attribute__((weak)) void NGPalInitAssets(void) {}

void NGEngineInit(void) {
    NGArenaSystemInit();
    NGPalInitDefault();
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

    // Draw menu text immediately after vblank while VRAM is safe to write.
    // Fix layer tiles persist in VRAM, so we only write when content changes.
    if (g_active_menu && NGMenuNeedsDraw(g_active_menu)) {
        NGMenuDraw(g_active_menu);
    }

    NGArenaReset(&ng_arena_frame);
    NGInputUpdate();
}

void NGEngineFrameEnd(void) {
    NGLightingUpdate();
    NGSceneUpdate();
    NGSceneDraw();
}

void NGEngineSetActiveMenu(NGMenuHandle menu) {
    g_active_menu = menu;
}

NGMenuHandle NGEngineGetActiveMenu(void) {
    return g_active_menu;
}
