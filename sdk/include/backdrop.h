/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file backdrop.h
 * @brief Backdrop (parallax layer) management.
 *
 * An NGBackdrop is a background or foreground visual that moves
 * based on camera movement to create depth perception.
 *
 * Key concept: Backdrops are positioned RELATIVE to the camera
 * viewport, not in scene coordinates. Their position is specified as
 * an offset from the camera's top-left corner at the time of addition.
 *
 * The Z-index is still in scene coordinates, determining render order
 * with other scene objects (actors and backdrops).
 *
 * Dimensions:
 * - Default: same as visual asset
 * - If larger: image repeats/tiles
 * - Max height: 512 pixels
 * - Width: can be infinite (0xFFFF) for endless scrolling
 *
 * @section backdropusage Usage
 * 1. Create backdrop with NGBackdropCreate()
 * 2. Add to scene with NGBackdropAddToScene()
 * 3. Scene automatically updates and draws backdrops
 *
 * @section backdroprates Parallax Rates
 * - FIX_ONE (1.0): moves 1:1 with camera (foreground)
 * - FIX_FROM_FLOAT(0.5): moves at half camera speed (mid-ground)
 * - FIX_FROM_FLOAT(0.25): moves at quarter speed (distant background)
 * - 0: doesn't move with camera (fixed on viewport)
 */

#ifndef _BACKDROP_H_
#define _BACKDROP_H_

#include <ng_types.h>
#include <ng_math.h>
#include <visual.h>

/** @defgroup backdropconst Backdrop Constants
 *  @{
 */

#define NG_BACKDROP_MAX            4      /**< Maximum backdrop layers */
#define NG_BACKDROP_WIDTH_INFINITE 0xFFFF /**< Infinite width value */

/** @} */

/** @defgroup backdrophandle Backdrop Handle
 *  @{
 */

/** Backdrop handle type */
typedef s8 NGBackdropHandle;

/** Invalid backdrop handle */
#define NG_BACKDROP_INVALID (-1)

/** @} */

/** @defgroup backdroplife Lifecycle Functions
 *  @{
 */

/**
 * Create a backdrop from a visual asset.
 * @param asset Visual asset to use
 * @param width Display width (0 = asset width, 0xFFFF = infinite)
 * @param height Display height (0 = asset height, max 512)
 * @param parallax_x Horizontal movement rate (FIX_ONE = 1:1 with camera)
 * @param parallax_y Vertical movement rate (FIX_ONE = 1:1 with camera)
 * @return Backdrop handle, or NG_BACKDROP_INVALID if no slots available
 */
NGBackdropHandle NGBackdropCreate(const NGVisualAsset *asset, u16 width, u16 height,
                                  fixed parallax_x, fixed parallax_y);

/**
 * Add backdrop to the scene.
 * Position is relative to current camera viewport position.
 * @param backdrop Backdrop handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 * @param z Z-index for render order (scene coordinate, affects sorting with actors)
 */
void NGBackdropAddToScene(NGBackdropHandle backdrop, s16 viewport_x, s16 viewport_y, u8 z);

/**
 * Remove backdrop from scene (can re-add later).
 * @param backdrop Backdrop handle
 */
void NGBackdropRemoveFromScene(NGBackdropHandle backdrop);

/**
 * Destroy backdrop and free resources.
 * @param backdrop Backdrop handle
 */
void NGBackdropDestroy(NGBackdropHandle backdrop);

/** @} */

/** @defgroup backdroppos Position Functions
 *  @{
 */

/**
 * Reposition backdrop relative to current camera viewport.
 * Use this when you want to adjust position; the new position
 * is relative to where the camera is NOW.
 * @param backdrop Backdrop handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 */
void NGBackdropSetViewportPos(NGBackdropHandle backdrop, s16 viewport_x, s16 viewport_y);

/**
 * Set backdrop Z-index.
 * @param backdrop Backdrop handle
 * @param z Z-index for render order
 */
void NGBackdropSetZ(NGBackdropHandle backdrop, u8 z);

/** @} */

/** @defgroup backdropappear Appearance Functions
 *  @{
 */

/**
 * Set backdrop visibility.
 * @param backdrop Backdrop handle
 * @param visible 1 to show, 0 to hide
 */
void NGBackdropSetVisible(NGBackdropHandle backdrop, u8 visible);

/**
 * Set backdrop palette.
 * @param backdrop Backdrop handle
 * @param palette Palette index (0-255)
 */
void NGBackdropSetPalette(NGBackdropHandle backdrop, u8 palette);

/** @} */

#endif /* _BACKDROP_H_ */
