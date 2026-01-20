/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file engine.h
 * @brief Application lifecycle and main loop management.
 *
 * The Engine module provides convenience functions for managing
 * the game's main loop and initialization. It consolidates the
 * various subsystem init calls and frame timing.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include <types.h>
#include <ui.h>

/** @defgroup engineinit Initialization
 *  @{
 */

/**
 * Initialize all engine subsystems.
 * Calls: ArenaSystemInit, PalInitDefault, PalInitAssets,
 *        TextClearAll, SceneInit, CameraInit, InputInit, AudioInit
 * Also sets backdrop color to black.
 *
 * PalInitAssets() is called automatically - if the game includes
 * progear_assets.h, its palettes will be loaded; otherwise a weak
 * no-op default is used.
 */
void EngineInit(void);

/** @} */

/** @defgroup engineloop Main Loop
 *  @{
 */

/**
 * Call at the start of each frame (top of main loop).
 * Calls: hw_wait_vblank, hw_watchdog_kick, ArenaReset(&arena_frame), InputUpdate
 */
void EngineFrameStart(void);

/**
 * Call at the end of each frame (bottom of main loop).
 * Calls: SceneUpdate, SceneDraw, and draws the active menu if set.
 */
void EngineFrameEnd(void);

/** @} */

/** @defgroup enginemenu Active Menu
 *  @{
 */

/**
 * Set the currently active menu for automatic drawing.
 * The engine will call MenuDraw() on this menu in EngineFrameEnd().
 * @param menu Menu handle, or NULL to disable menu drawing
 */
void EngineSetActiveMenu(MenuHandle menu);

/**
 * Get the currently active menu.
 * @return Current menu handle, or NULL if none set
 */
MenuHandle EngineGetActiveMenu(void);

/** @} */

#endif /* ENGINE_H */
