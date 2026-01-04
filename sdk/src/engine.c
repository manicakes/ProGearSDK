/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// engine.c - Application lifecycle and main loop management

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

// Active menu for automatic drawing
static NGMenuHandle g_active_menu = 0;

// Weak default for NGPalInitAssets - does nothing
// Games using ngres-generated assets will provide a strong definition
// that overrides this and loads all palette data
__attribute__((weak)) void NGPalInitAssets(void) {}

void NGEngineInit(void) {
    // Initialize all subsystems
    NGArenaSystemInit();
    NGPalInitDefault();
    NGFixClearAll();
    NGSceneInit();
    NGCameraInit();
    NGInputInit();
    NGAudioInit();

    // Load generated palette assets (if any)
    // This calls the weak default (no-op) unless ngres_generated_assets.h
    // provides a strong definition with actual palette data
    NGPalInitAssets();

    // Set default backdrop color
    NGPalSetBackdrop(NG_COLOR_BLACK);

    // Clear active menu
    g_active_menu = 0;
}

void NGEngineFrameStart(void) {
    // Wait for vertical blank
    NGWaitVBlank();

    // Kick the watchdog
    NGWatchdogKick();

    // Reset frame arena for temporary allocations
    NGArenaReset(&ng_arena_frame);

    // Poll input state
    NGInputUpdate();
}

void NGEngineFrameEnd(void) {
    // Update and draw scene
    NGSceneUpdate();
    NGSceneDraw();

    // Draw active menu if set
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
