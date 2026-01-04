/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file camera.h
 * @brief Camera system for viewing the scene.
 *
 * The NGCamera views a portion of the NGScene. Its position represents
 * the top-left corner of the viewport in scene coordinates.
 *
 * @section camcoord Coordinate System
 * - Scene origin (0,0) is at top-left
 * - X increases to the right, Y increases downward
 * - Camera position is the TOP-LEFT corner of the visible area
 * - Camera at (0,0) shows scene origin at screen top-left
 * - Viewport size is 320x224 (NeoGeo resolution)
 *
 * @section camfeatures Features
 * - Position: X,Y in scene coordinates
 * - Zoom: can zoom out to see more of the scene
 * - Effects: screen shake for explosions, impacts, etc.
 *
 * @section camusage Usage
 * 1. Call NGCameraInit() at startup
 * 2. Move camera with NGCameraMove(dx, dy)
 * 3. Set zoom for dramatic effects
 * 4. Call NGCameraUpdate() each frame
 */

#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <types.h>
#include <ngmath.h>

/** @defgroup camviewport Viewport Dimensions
 *  @brief NeoGeo screen size at 100% zoom.
 *  @{
 */

#define NG_CAM_VIEWPORT_WIDTH   320  /**< Screen width in pixels */
#define NG_CAM_VIEWPORT_HEIGHT  224  /**< Screen height in pixels */

/** @} */

/** @defgroup camworld World Limits
 *  @brief Maximum world dimensions for smooth scrolling.
 *  @{
 */

/**
 * Maximum world height for smooth Y scrolling (512 pixels = 32 tiles).
 * This limit exists because NeoGeo Y positioning uses 9-bit coordinates (0-511).
 * Keeping world height within this allows pre-loading all vertical tiles
 * and using hardware Y positioning for flicker-free scrolling.
 * X scrolling has no such limit since tiles can be dynamically cycled.
 */
#define NG_CAM_MAX_WORLD_HEIGHT  512

/** @} */

/** @defgroup camzoom Zoom Levels
 *  @brief Zoom constants (same as scroll zoom levels).
 *  @{
 */

#define NG_CAM_ZOOM_100    16  /**< 100% - Full size */
#define NG_CAM_ZOOM_87     14  /**< 87.5% */
#define NG_CAM_ZOOM_75     12  /**< 75% */
#define NG_CAM_ZOOM_62     10  /**< 62.5% */
#define NG_CAM_ZOOM_50     8   /**< 50% - Half size */

/** @} */

/** @defgroup camsys Camera System
 *  @{
 */

/**
 * Initialize camera system.
 * Sets camera to position (0,0) with 100% zoom.
 */
void NGCameraInit(void);

/**
 * Set camera position (top-left corner of view).
 * Camera at (0,0) shows world origin at screen top-left.
 * @param x World X coordinate of view's left edge
 * @param y World Y coordinate of view's top edge
 */
void NGCameraSetPos(fixed x, fixed y);

/**
 * Move camera by delta.
 * @param dx X delta (positive = scroll right)
 * @param dy Y delta (positive = scroll down)
 */
void NGCameraMove(fixed dx, fixed dy);

/**
 * Get camera X position.
 * @return World X coordinate
 */
fixed NGCameraGetX(void);

/**
 * Get camera Y position.
 * @return World Y coordinate
 */
fixed NGCameraGetY(void);

/**
 * Set camera zoom level (instant).
 * Affects all world-space objects uniformly.
 * @param zoom Zoom level (NG_CAM_ZOOM_100 to NG_CAM_ZOOM_50)
 */
void NGCameraSetZoom(u8 zoom);

/**
 * Set target zoom for smooth transition.
 * Call NGCameraUpdate() each frame to animate.
 * @param zoom Target zoom level (NG_CAM_ZOOM_100 to NG_CAM_ZOOM_50)
 */
void NGCameraSetTargetZoom(u8 zoom);

/**
 * Set zoom transition speed.
 * @param speed Interpolation factor per frame (default ~0.15, higher = faster)
 */
void NGCameraSetZoomSpeed(fixed speed);

/**
 * Get current camera zoom level (integer).
 * @return Current zoom level (may be mid-transition)
 */
u8 NGCameraGetZoom(void);

/**
 * Get current camera zoom level (fixed-point for smooth transitions).
 * @return Current zoom as fixed-point value
 */
fixed NGCameraGetZoomFixed(void);

/**
 * Check if zoom is currently animating.
 * @return 1 if zoom is transitioning, 0 if stable
 */
u8 NGCameraIsZooming(void);

/**
 * Get target zoom level.
 * @return Target zoom level
 */
u8 NGCameraGetTargetZoom(void);

/**
 * Get precalculated shrink value for current zoom.
 * Returns SCB2 format: (h_shrink << 8) | v_shrink
 * Uses lookup table for zero runtime calculation.
 * @return Shrink value ready for VRAM
 */
u16 NGCameraGetShrink(void);

/**
 * Update camera (call once per frame).
 * Handles smooth zoom transitions and effects.
 */
void NGCameraUpdate(void);

/** @} */

/** @defgroup cameffects Camera Effects
 *  @{
 */

/**
 * Trigger camera shake effect.
 * Simulates screen shake for explosions, impacts, etc.
 * @param intensity Shake intensity in pixels (1-8 typical)
 * @param duration Duration in frames (e.g., 15 for quarter second)
 */
void NGCameraShake(u8 intensity, u8 duration);

/**
 * Check if shake effect is active.
 * @return 1 if shaking, 0 if stable
 */
u8 NGCameraIsShaking(void);

/**
 * Stop shake effect immediately.
 */
void NGCameraShakeStop(void);

/** @} */

/** @defgroup camutil Camera Utilities
 *  @{
 */

/**
 * Get visible world width at current zoom.
 * @return Width in world pixels
 */
u16 NGCameraGetVisibleWidth(void);

/**
 * Get visible world height at current zoom.
 * @return Height in world pixels
 */
u16 NGCameraGetVisibleHeight(void);

/**
 * Clamp camera to world bounds.
 * Prevents showing areas outside the world extent.
 * Y is automatically clamped to NG_CAM_MAX_WORLD_HEIGHT (512) for smooth scrolling.
 * @param world_width World width in pixels
 * @param world_height World height in pixels (clamped to NG_CAM_MAX_WORLD_HEIGHT)
 */
void NGCameraClampToBounds(u16 world_width, u16 world_height);

/**
 * Transform world coordinates to screen coordinates.
 * Applies camera position and zoom.
 * @param world_x World X coordinate
 * @param world_y World Y coordinate
 * @param screen_x Output screen X
 * @param screen_y Output screen Y
 */
void NGCameraWorldToScreen(fixed world_x, fixed world_y,
                           s16 *screen_x, s16 *screen_y);

/**
 * Transform screen coordinates to world coordinates.
 * Inverse of NGCameraWorldToScreen.
 * @param screen_x Screen X coordinate
 * @param screen_y Screen Y coordinate
 * @param world_x Output world X
 * @param world_y Output world Y
 */
void NGCameraScreenToWorld(s16 screen_x, s16 screen_y,
                           fixed *world_x, fixed *world_y);

/** @} */

#endif // _CAMERA_H_
