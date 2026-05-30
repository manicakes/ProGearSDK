/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file engine.h
 * @brief Application lifecycle and main loop management.
 *
 * The NGEngine module provides convenience functions for managing
 * the game's main loop and initialization. It consolidates the
 * various subsystem init calls and frame timing.
 */

#ifndef NG_ENGINE_H
#define NG_ENGINE_H

#include <ng_types.h>
#include <ui.h>

/**
 * @defgroup engine Engine
 * @ingroup sdk
 * @brief Application lifecycle, initialization, and main loop.
 * @{
 */

/** @name Initialization */
/** @{ */

/**
 * Initialize all engine subsystems.
 * Calls: NGArenaSystemInit, NGPalInitDefault, NGPalInitAssets,
 *        NGFixClearAll, NGSceneInit, NGCameraInit, NGInputInit, NGAudioInit
 * Also sets backdrop color to black.
 *
 * NGPalInitAssets() is called automatically - if the game includes
 * progear_assets.h, its palettes will be loaded; otherwise a weak
 * no-op default is used.
 */
void NGEngineInit(void);
/** @} */

/** @name Main Loop */
/** @{ */

/**
 * Call at the start of each frame (top of main loop).
 * Waits for VBlank, then commits the previous frame's rendering while VRAM is
 * safe to write: draws the active menu (if set) and the scene sprites. Then
 * resets the frame arena and polls input.
 *
 * Rendering is deliberately committed here rather than at frame end so that all
 * VRAM writes land during VBlank. This means a frame's gameplay changes become
 * visible on the following frame.
 */
void NGEngineFrameStart(void);

/**
 * Call at the end of each frame (bottom of main loop).
 * Advances lighting and the scene (camera, animation, graphic sync). This is
 * pure computation; the resulting frame is committed to VRAM by the next
 * NGEngineFrameStart() during VBlank.
 */
void NGEngineFrameEnd(void);
/** @} */

/** @name Active Menu */
/** @{ */

/**
 * Set the currently active menu for automatic drawing.
 * The engine will call NGMenuDraw() on this menu in NGEngineFrameEnd().
 * @param menu Menu handle, or NULL to disable menu drawing
 */
void NGEngineSetActiveMenu(NGMenuHandle menu);

/**
 * Get the currently active menu.
 * @return Current menu handle, or NULL if none set
 */
NGMenuHandle NGEngineGetActiveMenu(void);
/** @} */

/** @} */ /* end of engine group */

#endif /* NG_ENGINE_H */
