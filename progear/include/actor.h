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

/** @name Anchor */
/** @{ */

/**
 * The point within an actor's frame that its position refers to.
 *
 * The anchor declares what an actor's coordinates *mean*. Top-left is the
 * default, which is what most static art wants. Other genres want other
 * points: bottom-centre puts a character's feet on its position, so the same
 * value both places it and sorts its depth; centre suits ships, bullets and
 * explosions; a scaled sprite grows away from its anchor, which is what an
 * object rising off a road plane needs.
 *
 * Values are laid out as a 3x3 grid (index = row * 3 + column), so the pixel
 * offset is arithmetic rather than a lookup.
 */
typedef enum {
    NG_ANCHOR_TOP_LEFT = 0,
    NG_ANCHOR_TOP = 1,
    NG_ANCHOR_TOP_RIGHT = 2,
    NG_ANCHOR_LEFT = 3,
    NG_ANCHOR_CENTER = 4,
    NG_ANCHOR_RIGHT = 5,
    NG_ANCHOR_BOTTOM_LEFT = 6,
    NG_ANCHOR_BOTTOM = 7,
    NG_ANCHOR_BOTTOM_RIGHT = 8
} NGAnchor;

/**
 * Set the actor's anchor to one of the nine grid positions.
 *
 * Because the anchor defines what the actor's position means, changing it
 * moves the sprite rather than rewriting the position. Set it once after
 * creation, before placing the actor.
 *
 * @param actor Actor handle
 * @param anchor Anchor position
 */
void NGActorSetAnchor(NGActorHandle actor, NGAnchor anchor);

/**
 * Set the actor's anchor to an arbitrary point, in unscaled frame pixels
 * measured from the top-left of the frame.
 *
 * Use this when none of the nine grid positions is right - a character whose
 * pivot is a hand or a hilt, say.
 *
 * @param actor Actor handle
 * @param x Offset from the frame's left edge
 * @param y Offset from the frame's top edge
 */
void NGActorSetAnchorPixels(NGActorHandle actor, s16 x, s16 y);

/**
 * Get the actor's anchor offset in unscaled frame pixels.
 *
 * @param actor Actor handle
 * @return Offset from the frame's top-left corner
 */
NGVec2 NGActorGetAnchorPixels(NGActorHandle actor);
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
 * Position the actor so that @p anchor lands on (x, y), without changing the
 * actor's own anchor.
 *
 * Useful for one-off placement in a different frame of reference than the
 * actor normally uses - centring an explosion on an impact point, or dropping
 * a pickup so its base sits on a platform.
 *
 * @param actor Actor handle
 * @param x Scene X coordinate
 * @param y Scene Y coordinate
 * @param anchor Anchor to place at (x, y) for this call only
 */
void NGActorSetPosAnchored(NGActorHandle actor, fixed x, fixed y, NGAnchor anchor);

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

/**
 * Get the graphic backing an actor.
 * Advanced accessor for raster effects (see NGGraphicGetRasterXInfo).
 * @param actor Actor handle
 * @return Graphic, or NULL if the handle is invalid
 */
struct NGGraphic *NGActorGetGraphic(NGActorHandle actor);
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
