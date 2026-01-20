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

static MenuHandle g_active_menu = 0;

/* Weak default - games using progear_assets.py provide a strong definition */
__attribute__((weak)) void PalInitAssets(void) {}

void EngineInit(void) {
    ArenaSystemInit();
    PalInitDefault();
    FixClearAll();
    SceneInit();
    CameraInit();
    InputInit();
    AudioInit();
    LightingInit();
    PalInitAssets();
    PalSetBackdrop(COLOR_BLACK);
    g_active_menu = 0;
}

void EngineFrameStart(void) {
    WaitVBlank();
    WatchdogKick();

    /* Draw menu text immediately after vblank while VRAM is safe to write.
     * Fix layer tiles persist in VRAM, so we only write when content changes. */
    if (g_active_menu && MenuNeedsDraw(g_active_menu)) {
        MenuDraw(g_active_menu);
    }

    ArenaReset(&arena_frame);
    InputUpdate();
}

void EngineFrameEnd(void) {
    LightingUpdate();
    SceneUpdate();
    SceneDraw();
}

void EngineSetActiveMenu(MenuHandle menu) {
    g_active_menu = menu;
}

MenuHandle EngineGetActiveMenu(void) {
    return g_active_menu;
}
