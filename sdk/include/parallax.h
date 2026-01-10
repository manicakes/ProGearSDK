/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file parallax.h
 * @brief Parallax effect management.
 *
 * An NGParallaxEffect is a background or foreground visual that moves
 * based on camera movement to create depth perception.
 *
 * Key concept: Parallax effects are positioned RELATIVE to the camera
 * viewport, not in scene coordinates. Their position is specified as
 * an offset from the camera's top-left corner at the time of addition.
 *
 * The Z-index is still in scene coordinates, determining render order
 * with other scene objects (actors and parallax effects).
 *
 * Dimensions:
 * - Default: same as visual asset
 * - If larger: image repeats/tiles
 * - Max height: 512 pixels
 * - Width: can be infinite (0xFFFF) for endless scrolling
 *
 * @section parallaxusage Usage
 * 1. Create parallax with NGParallaxCreate()
 * 2. Add to scene with NGParallaxAddToScene()
 * 3. Scene automatically updates and draws parallax effects
 *
 * @section parallaxrates Parallax Rates
 * - FIX_ONE (1.0): moves 1:1 with camera (foreground)
 * - FIX_FROM_FLOAT(0.5): moves at half camera speed (mid-ground)
 * - FIX_FROM_FLOAT(0.25): moves at quarter speed (distant background)
 * - 0: doesn't move with camera (fixed on viewport)
 */

#ifndef _PARALLAX_H_
#define _PARALLAX_H_

#include <types.h>
#include <ngmath.h>
#include <visual.h>

/** @defgroup parallaxconst Parallax Constants
 *  @{
 */

#define NG_PARALLAX_MAX            4      /**< Maximum parallax layers */
#define NG_PARALLAX_WIDTH_INFINITE 0xFFFF /**< Infinite width value */

/** @} */

/** @defgroup parallaxhandle Parallax Handle
 *  @{
 */

/** Parallax handle type */
typedef s8 NGParallaxHandle;

/** Invalid parallax handle */
#define NG_PARALLAX_INVALID (-1)

/** @} */

/** @defgroup parallaxlife Lifecycle Functions
 *  @{
 */

/**
 * Create a parallax effect from a visual asset.
 * @param asset Visual asset to use
 * @param width Display width (0 = asset width, 0xFFFF = infinite)
 * @param height Display height (0 = asset height, max 512)
 * @param parallax_x Horizontal movement rate (FIX_ONE = 1:1 with camera)
 * @param parallax_y Vertical movement rate (FIX_ONE = 1:1 with camera)
 * @return Parallax handle, or NG_PARALLAX_INVALID if no slots available
 */
NGParallaxHandle NGParallaxCreate(const NGVisualAsset *asset, u16 width, u16 height,
                                  fixed parallax_x, fixed parallax_y);

/**
 * Add parallax effect to the scene.
 * Position is relative to current camera viewport position.
 * @param parallax Parallax handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 * @param z Z-index for render order (scene coordinate, affects sorting with actors)
 */
void NGParallaxAddToScene(NGParallaxHandle parallax, s16 viewport_x, s16 viewport_y, u8 z);

/**
 * Remove parallax from scene (can re-add later).
 * @param parallax Parallax handle
 */
void NGParallaxRemoveFromScene(NGParallaxHandle parallax);

/**
 * Destroy parallax effect and free resources.
 * @param parallax Parallax handle
 */
void NGParallaxDestroy(NGParallaxHandle parallax);

/** @} */

/** @defgroup parallaxpos Position Functions
 *  @{
 */

/**
 * Reposition parallax relative to current camera viewport.
 * Use this when you want to adjust position; the new position
 * is relative to where the camera is NOW.
 * @param parallax Parallax handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 */
void NGParallaxSetViewportPos(NGParallaxHandle parallax, s16 viewport_x, s16 viewport_y);

/**
 * Set parallax Z-index.
 * @param parallax Parallax handle
 * @param z Z-index for render order
 */
void NGParallaxSetZ(NGParallaxHandle parallax, u8 z);

/** @} */

/** @defgroup parallaxappear Appearance Functions
 *  @{
 */

/**
 * Set parallax visibility.
 * @param parallax Parallax handle
 * @param visible 1 to show, 0 to hide
 */
void NGParallaxSetVisible(NGParallaxHandle parallax, u8 visible);

/**
 * Set parallax palette.
 * @param parallax Parallax handle
 * @param palette Palette index (0-255)
 */
void NGParallaxSetPalette(NGParallaxHandle parallax, u8 palette);

/** @} */

#endif /* _PARALLAX_H_ */
