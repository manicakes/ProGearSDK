/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file actor.h
 * @brief Scene actor management.
 *
 * An NGActor is an in-game object positioned in scene coordinates.
 * Actors are initialized from NGVisualAssets and can be animated.
 *
 * Actor dimensions:
 * - Default: same as the visual asset
 * - If smaller: image is clipped
 * - If larger: image repeats/tiles
 * - Max height: 512 pixels
 * - Width: can be infinite (0xFFFF)
 *
 * @section actorusage Usage
 * 1. Create actor with NGActorCreate()
 * 2. Add to scene with NGActorAddToScene()
 * 3. Set animation, position as needed
 * 4. Scene automatically updates and draws actors
 */

#ifndef NG_ACTOR_H
#define NG_ACTOR_H

#include <ng_types.h>
#include <ng_math.h>
#include <visual.h>

/**
 * @defgroup actor Actor System
 * @ingroup sdk
 * @brief Game objects with position, animation, and appearance.
 * @{
 */

/** @name Constants */
/** @{ */

#define NG_ACTOR_MAX            64     /**< Maximum active actors */
#define NG_ACTOR_WIDTH_INFINITE 0xFFFF /**< Infinite width value */
/** @} */

/** @name Handle Type */
/** @{ */

/** Actor handle type */
typedef s8 NGActorHandle;

/** Invalid actor handle */
#define NG_ACTOR_INVALID (-1)
/** @} */

/** @name Lifecycle */
/** @{ */

/**
 * Create an actor from a visual asset.
 * @param asset Visual asset to use
 * @param width Display width (0 = asset width, 0xFFFF = infinite)
 * @param height Display height (0 = asset height, max 512)
 * @return Actor handle, or NG_ACTOR_INVALID if no slots available
 */
NGActorHandle NGActorCreate(const NGVisualAsset *asset, u16 width, u16 height);

/**
 * Add actor to the scene.
 * @param actor Actor handle
 * @param x Scene X position (fixed-point)
 * @param y Scene Y position (fixed-point)
 * @param z Z-index for render order (0 = back, higher = front)
 */
void NGActorAddToScene(NGActorHandle actor, fixed x, fixed y, u8 z);

/**
 * Remove actor from scene (can re-add later).
 * @param actor Actor handle
 */
void NGActorRemoveFromScene(NGActorHandle actor);

/**
 * Destroy actor and free resources.
 * @param actor Actor handle
 */
void NGActorDestroy(NGActorHandle actor);
/** @} */

/** @name Position */
/** @{ */

/**
 * Set actor position in scene.
 * @param actor Actor handle
 * @param x Scene X position (fixed-point)
 * @param y Scene Y position (fixed-point)
 */
void NGActorSetPos(NGActorHandle actor, fixed x, fixed y);

/**
 * Move actor by offset.
 * @param actor Actor handle
 * @param dx X offset (fixed-point)
 * @param dy Y offset (fixed-point)
 */
void NGActorMove(NGActorHandle actor, fixed dx, fixed dy);

/**
 * Set actor Z-index.
 * @param actor Actor handle
 * @param z Z-index for render order
 */
void NGActorSetZ(NGActorHandle actor, u8 z);

/**
 * Get actor position as a vector.
 * @param actor Actor handle
 * @return Position vector (fixed-point x, y)
 */
NGVec2 NGActorGetPos(NGActorHandle actor);

/**
 * Get actor X position.
 * @param actor Actor handle
 * @return X position (fixed-point)
 */
fixed NGActorGetX(NGActorHandle actor);

/**
 * Get actor Y position.
 * @param actor Actor handle
 * @return Y position (fixed-point)
 */
fixed NGActorGetY(NGActorHandle actor);

/**
 * Get actor Z-index.
 * @param actor Actor handle
 * @return Z-index
 */
u8 NGActorGetZ(NGActorHandle actor);
/** @} */

/** @name Animation */
/** @{ */

/**
 * Set animation by index.
 * @param actor Actor handle
 * @param anim_index Animation index
 */
void NGActorSetAnim(NGActorHandle actor, u8 anim_index);

/**
 * Set animation by name.
 * @param actor Actor handle
 * @param name Animation name to find
 * @return 1 if found, 0 if not found
 */
u8 NGActorSetAnimByName(NGActorHandle actor, const char *name);

/**
 * Set specific frame (stops animation).
 * @param actor Actor handle
 * @param frame Frame index
 */
void NGActorSetFrame(NGActorHandle actor, u16 frame);

/**
 * Check if non-looping animation has finished.
 * @param actor Actor handle
 * @return 1 if done, 0 if still playing
 */
u8 NGActorAnimDone(NGActorHandle actor);
/** @} */

/** @name Appearance */
/** @{ */

/**
 * Set actor visibility.
 * @param actor Actor handle
 * @param visible 1 to show, 0 to hide
 */
void NGActorSetVisible(NGActorHandle actor, u8 visible);

/**
 * Set actor palette.
 * @param actor Actor handle
 * @param palette Palette index (0-255)
 */
void NGActorSetPalette(NGActorHandle actor, u8 palette);

/**
 * Set horizontal flip.
 * @param actor Actor handle
 * @param flip 1 to flip, 0 for normal
 */
void NGActorSetHFlip(NGActorHandle actor, u8 flip);

/**
 * Set vertical flip.
 * @param actor Actor handle
 * @param flip 1 to flip, 0 for normal
 */
void NGActorSetVFlip(NGActorHandle actor, u8 flip);

/**
 * Set screen-space mode (for UI elements).
 * Screen-space actors ignore camera position and zoom.
 * Their x,y position is in screen coordinates (0,0 = top-left).
 * @param actor Actor handle
 * @param enabled 1 for screen-space, 0 for world-space (default)
 */
void NGActorSetScreenSpace(NGActorHandle actor, u8 enabled);
/** @} */

/** @name Audio */
/** @{ */

/**
 * Play a sound effect at an actor's position.
 * Automatically calculates stereo pan based on actor position.
 * @param actor Actor handle
 * @param sfx_index Sound effect index (from generated NGSFX_* constants)
 */
void NGActorPlaySfx(NGActorHandle actor, u8 sfx_index);
/** @} */

/** @} */ /* end of actor group */

#endif /* NG_ACTOR_H */
