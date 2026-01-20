/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file camera.h
 * @brief Camera system for viewing the scene.
 *
 * The Camera views a portion of the Scene. Its position represents
 * the top-left corner of the viewport in scene coordinates.
 *
 * @section camcoord Coordinate System
 * - Scene origin (0,0) is at top-left
 * - X increases to the right, Y increases downward
 * - Camera position is the TOP-LEFT corner of the visible area
 * - Viewport size is 320x224 (NeoGeo resolution)
 *
 * @section camfeatures Features
 * - Position: X,Y in scene coordinates
 * - Zoom: can zoom out to see more of the scene
 * - Effects: screen shake for explosions, impacts, etc.
 * - Actor tracking with deadzone
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <types.h>
#include <ngmath.h>

/** @defgroup camviewport Viewport Dimensions
 *  @{
 */

#define CAM_VIEWPORT_WIDTH   320 /**< Screen width in pixels */
#define CAM_VIEWPORT_HEIGHT  224 /**< Screen height in pixels */
#define CAM_MAX_WORLD_HEIGHT 512 /**< Maximum world height for smooth scrolling */

/** @} */

/** @defgroup camzoom Zoom Levels
 *  @{
 */

#define CAM_ZOOM_100 16 /**< 100% - Full size */
#define CAM_ZOOM_87  14 /**< 87.5% */
#define CAM_ZOOM_75  12 /**< 75% */
#define CAM_ZOOM_62  10 /**< 62.5% */
#define CAM_ZOOM_50  8  /**< 50% - Half size */

/** @} */

/** @defgroup camsys Camera System
 *  @{
 */

/**
 * Initialize camera system.
 * Sets camera to position (0,0) with 100% zoom.
 */
void CameraInit(void);

/**
 * Update camera (call once per frame).
 * Handles smooth zoom transitions and effects.
 */
void CameraUpdate(void);

/**
 * Set camera position (top-left corner of view).
 * @param x World X coordinate of view's left edge
 * @param y World Y coordinate of view's top edge
 */
void CameraSetPos(fixed x, fixed y);

/**
 * Move camera by delta.
 * @param dx X delta (positive = scroll right)
 * @param dy Y delta (positive = scroll down)
 */
void CameraMove(fixed dx, fixed dy);

/**
 * Get camera X position.
 */
fixed CameraGetX(void);

/**
 * Get camera Y position.
 */
fixed CameraGetY(void);

/**
 * Set camera zoom level (instant).
 * @param zoom Zoom level (CAM_ZOOM_100 to CAM_ZOOM_50)
 */
void CameraSetZoom(u8 zoom);

/**
 * Set target zoom for smooth transition.
 * @param zoom Target zoom level
 */
void CameraSetTargetZoom(u8 zoom);

/**
 * Get current camera zoom level.
 */
u8 CameraGetZoom(void);

/**
 * Get target zoom level.
 */
u8 CameraGetTargetZoom(void);

/**
 * Check if zoom is currently animating.
 * @return 1 if zoom is transitioning, 0 if stable
 */
u8 CameraIsZooming(void);

/**
 * Get precalculated shrink value for current zoom.
 * Returns SCB2 format: (h_shrink << 8) | v_shrink
 */
u16 CameraGetShrink(void);

/**
 * Get visible world width at current zoom.
 */
u16 CameraGetVisibleWidth(void);

/**
 * Get visible world height at current zoom.
 */
u16 CameraGetVisibleHeight(void);

/**
 * Clamp camera to world bounds.
 * @param world_width World width in pixels
 * @param world_height World height in pixels
 */
void CameraClampToBounds(u16 world_width, u16 world_height);

/**
 * Transform world coordinates to screen coordinates.
 * @param world_x World X coordinate
 * @param world_y World Y coordinate
 * @param screen_x Output screen X
 * @param screen_y Output screen Y
 */
void CameraWorldToScreen(fixed world_x, fixed world_y, s16 *screen_x, s16 *screen_y);

/**
 * Transform screen coordinates to world coordinates.
 * @param screen_x Screen X coordinate
 * @param screen_y Screen Y coordinate
 * @param world_x Output world X
 * @param world_y Output world Y
 */
void CameraScreenToWorld(s16 screen_x, s16 screen_y, fixed *world_x, fixed *world_y);

/** @} */

/** @defgroup cameffects Camera Effects
 *  @{
 */

/**
 * Trigger camera shake effect.
 * @param intensity Shake intensity in pixels (1-8 typical)
 * @param duration Duration in frames
 */
void CameraShake(u8 intensity, u8 duration);

/**
 * Check if shake effect is active.
 */
u8 CameraIsShaking(void);

/**
 * Get camera X position with shake offset applied.
 */
fixed CameraGetRenderX(void);

/**
 * Get camera Y position with shake offset applied.
 */
fixed CameraGetRenderY(void);

/** @} */

/** @defgroup camtrack Actor Tracking
 *  @brief Metal Slug-style camera following with deadzone.
 *  @{
 */

#include <actor.h>

/**
 * Set an actor for the camera to track.
 * @param actor Actor handle to track, or ACTOR_INVALID to stop tracking
 */
void CameraTrackActor(Actor actor);

/**
 * Stop tracking any actor.
 */
void CameraStopTracking(void);

/**
 * Set the tracking deadzone size.
 * @param width Deadzone width in pixels (0 = always follow)
 * @param height Deadzone height in pixels (0 = always follow)
 */
void CameraSetDeadzone(u16 width, u16 height);

/**
 * Set the camera follow speed.
 * @param speed Follow speed as fixed-point (0.1 = slow, 0.5 = fast)
 */
void CameraSetFollowSpeed(fixed speed);

/**
 * Set world bounds for camera clamping during tracking.
 * @param world_width World width in pixels (0 = no X clamping)
 * @param world_height World height in pixels (0 = no Y clamping)
 */
void CameraSetBounds(u16 world_width, u16 world_height);

/**
 * Set tracking offset from actor center.
 * @param offset_x Horizontal offset in pixels
 * @param offset_y Vertical offset in pixels
 */
void CameraSetTrackOffset(s16 offset_x, s16 offset_y);

/** @} */

#endif /* CAMERA_H */
