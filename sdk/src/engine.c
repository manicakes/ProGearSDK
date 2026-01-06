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

static NGMenuHandle g_active_menu = 0;

// Weak default - games using ngres provide a strong definition that loads palette data
__attribute__((weak)) void NGPalInitAssets(void) {}

void NGEngineInit(void) {
    NGArenaSystemInit();
    NGPalInitDefault();
    NGFixClearAll();
    NGSceneInit();
    NGCameraInit();
    NGInputInit();
    NGAudioInit();
    NGPalInitAssets();
    NGPalSetBackdrop(NG_COLOR_BLACK);
    g_active_menu = 0;
}

void NGEngineFrameStart(void) {
    NGWaitVBlank();
    NGWatchdogKick();
    NGArenaReset(&ng_arena_frame);
    NGInputUpdate();
}

void NGEngineFrameEnd(void) {
    NGSceneUpdate();
    NGSceneDraw();

    if (g_active_menu) {
        NGMenuDraw(g_active_menu);
    }
}

void NGEngineSetActiveMenu(NGMenuHandle menu) {
    g_active_menu = menu;
}

NGMenuHandle NGEngineGetActiveMenu(void) {
    return g_active_menu;
}
