/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file scene.h
 * @brief Scene management.
 *
 * The NGScene is the "stage" where all visual objects exist.
 * It has a coordinate system with X, Y, and Z axes:
 * - X: 0 = leftmost, increases going right (no limit)
 * - Y: 0 = topmost, increases going down (max 512 due to hardware)
 * - Z: render order, 0 = back (rendered first), higher = front
 *
 * @section sceneusage Usage
 * 1. Call NGSceneInit() at startup
 * 2. Create actors and parallax effects, add them to scene
 * 3. Call NGSceneUpdate() and NGSceneDraw() each frame
 */

#ifndef _SCENE_H_
#define _SCENE_H_

#include <types.h>

/** @defgroup sceneconst Scene Constants
 *  @{
 */

#define NG_SCENE_MAX_HEIGHT     512     /**< Maximum scene height in pixels */
#define NG_SCENE_VIEWPORT_W     320     /**< Viewport width (NeoGeo resolution) */
#define NG_SCENE_VIEWPORT_H     224     /**< Viewport height (NeoGeo resolution) */

/** @} */

/** @defgroup scenefunc Scene Functions
 *  @{
 */

/**
 * Initialize the scene system.
 * Call once at startup before creating any actors or parallax effects.
 */
void NGSceneInit(void);

/**
 * Update all scene objects.
 * Call once per frame. Updates animations and processes scene logic.
 */
void NGSceneUpdate(void);

/**
 * Draw all scene objects.
 * Call once per frame during VBlank. Renders objects in Z-order.
 */
void NGSceneDraw(void);

/**
 * Reset scene to empty state.
 * Destroys all actors and parallax effects, clears hardware sprites.
 * Call when transitioning between levels/screens.
 */
void NGSceneReset(void);

/** @} */

#endif /* _SCENE_H_ */
