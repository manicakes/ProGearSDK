/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file backdrop.h
 * @brief Backdrop (parallax layer) management.
 *
 * A Backdrop is a background or foreground visual that moves
 * based on camera movement to create depth perception.
 *
 * Key concept: Backdrops are positioned RELATIVE to the camera
 * viewport, not in scene coordinates. Their position is specified as
 * an offset from the camera's top-left corner at the time of addition.
 *
 * @section backdroprates Parallax Rates
 * - FIX_ONE (1.0): moves 1:1 with camera (foreground)
 * - FIX_FROM_FLOAT(0.5): moves at half camera speed (mid-ground)
 * - FIX_FROM_FLOAT(0.25): moves at quarter speed (distant background)
 * - 0: doesn't move with camera (fixed on viewport)
 */

#ifndef BACKDROP_H
#define BACKDROP_H

#include <types.h>
#include <ngmath.h>
#include <visual.h>

/** @defgroup backdropconst Backdrop Constants
 *  @{
 */

#define BACKDROP_MAX            4      /**< Maximum backdrop layers */
#define BACKDROP_WIDTH_INFINITE 0xFFFF /**< Infinite width value */

/** @} */

/** @defgroup backdrophandle Backdrop Handle
 *  @{
 */

/** Backdrop handle type */
typedef s8 Backdrop;

/** Invalid backdrop handle */
#define BACKDROP_INVALID (-1)

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
 * @return Backdrop handle, or BACKDROP_INVALID if no slots available
 */
Backdrop BackdropCreate(const VisualAsset *asset, u16 width, u16 height, fixed parallax_x,
                        fixed parallax_y);

/**
 * Add backdrop to the scene.
 * Position is relative to current camera viewport position.
 * @param backdrop Backdrop handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 * @param z Z-index for render order
 */
void BackdropAddToScene(Backdrop backdrop, s16 viewport_x, s16 viewport_y, u8 z);

/**
 * Remove backdrop from scene (can re-add later).
 * @param backdrop Backdrop handle
 */
void BackdropRemoveFromScene(Backdrop backdrop);

/**
 * Destroy backdrop and free resources.
 * @param backdrop Backdrop handle
 */
void BackdropDestroy(Backdrop backdrop);

/** @} */

/** @defgroup backdroppos Position Functions
 *  @{
 */

/**
 * Reposition backdrop relative to current camera viewport.
 * @param backdrop Backdrop handle
 * @param viewport_x X offset from camera viewport left edge
 * @param viewport_y Y offset from camera viewport top edge
 */
void BackdropSetViewportPos(Backdrop backdrop, s16 viewport_x, s16 viewport_y);

/**
 * Set backdrop Z-index.
 * @param backdrop Backdrop handle
 * @param z Z-index for render order
 */
void BackdropSetZ(Backdrop backdrop, u8 z);

/** @} */

/** @defgroup backdropappear Appearance Functions
 *  @{
 */

/**
 * Set backdrop visibility.
 * @param backdrop Backdrop handle
 * @param visible 1 to show, 0 to hide
 */
void BackdropSetVisible(Backdrop backdrop, u8 visible);

/**
 * Set backdrop palette.
 * @param backdrop Backdrop handle
 * @param palette Palette index (0-255)
 */
void BackdropSetPalette(Backdrop backdrop, u8 palette);

/** @} */

/*
 * Internal functions - used by other SDK modules.
 */
void _BackdropSystemInit(void);
void _BackdropSystemUpdate(void);
u8 _BackdropIsInScene(Backdrop handle);
u8 _BackdropGetZ(Backdrop handle);
void _BackdropDraw(Backdrop handle, u16 first_sprite);
u8 _BackdropGetSpriteCount(Backdrop handle);

#endif /* BACKDROP_H */
