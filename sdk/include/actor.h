/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file actor.h
 * @brief Game objects in the world.
 *
 * Actors are visual objects positioned in the game world. Create them from
 * visual assets, add them to the scene, and the engine handles the rest.
 *
 * @example
 * // Create and position an actor
 * Actor player = ActorCreate(&player_asset);
 * ActorAddToScene(player, FIX(100), FIX(50), 10);
 *
 * // Move and animate
 * ActorMove(player, FIX(2), 0);
 * ActorPlayAnim(player, "walk");
 */

#ifndef ACTOR_H
#define ACTOR_H

#include <types.h>
#include <ngmath.h>
#include <visual.h>

/**
 * @defgroup actor Actors
 * @brief Visual game objects.
 * @{
 */

/** Maximum number of active actors */
#define ACTOR_MAX 64

/** Actor handle type (simple integer for efficiency) */
typedef s8 Actor;

/** Invalid actor handle */
#define ACTOR_INVALID (-1)

/**
 * Create an actor from a visual asset.
 *
 * @param asset Visual asset to use
 * @return Actor handle, or ACTOR_INVALID if no slots available
 */
Actor ActorCreate(const VisualAsset *asset);

/**
 * Create an actor with custom display dimensions.
 * If dimensions differ from asset size, the image will tile/clip.
 *
 * @param asset Visual asset to use
 * @param width Display width in pixels (0 = use asset width)
 * @param height Display height in pixels (0 = use asset height)
 * @return Actor handle, or ACTOR_INVALID if no slots available
 */
Actor ActorCreateSized(const VisualAsset *asset, u16 width, u16 height);

/**
 * Destroy an actor and free resources.
 *
 * @param actor Actor handle
 */
void ActorDestroy(Actor actor);

/**
 * Add an actor to the scene at a position.
 *
 * @param actor Actor handle
 * @param x World X position (fixed-point)
 * @param y World Y position (fixed-point)
 * @param z Z-index for render order (0 = back, higher = front)
 */
void ActorAddToScene(Actor actor, fixed x, fixed y, u8 z);

/**
 * Remove an actor from the scene (can re-add later).
 *
 * @param actor Actor handle
 */
void ActorRemoveFromScene(Actor actor);

/**
 * Set actor position.
 *
 * @param actor Actor handle
 * @param x World X position (fixed-point)
 * @param y World Y position (fixed-point)
 */
void ActorSetPos(Actor actor, fixed x, fixed y);

/**
 * Move actor by offset.
 *
 * @param actor Actor handle
 * @param dx X offset (fixed-point)
 * @param dy Y offset (fixed-point)
 */
void ActorMove(Actor actor, fixed dx, fixed dy);

/**
 * Get actor X position.
 */
fixed ActorGetX(Actor actor);

/**
 * Get actor Y position.
 */
fixed ActorGetY(Actor actor);

/**
 * Set actor Z-index (render order).
 */
void ActorSetZ(Actor actor, u8 z);

/**
 * Get actor Z-index.
 */
u8 ActorGetZ(Actor actor);

/**
 * Show or hide an actor.
 *
 * @param actor Actor handle
 * @param visible Non-zero to show, 0 to hide
 */
void ActorSetVisible(Actor actor, u8 visible);

/**
 * Set horizontal and vertical flip.
 *
 * @param actor Actor handle
 * @param h_flip Non-zero to flip horizontally
 * @param v_flip Non-zero to flip vertically
 */
void ActorSetFlip(Actor actor, u8 h_flip, u8 v_flip);

/**
 * Set actor palette.
 *
 * @param actor Actor handle
 * @param palette Palette index (0-255)
 */
void ActorSetPalette(Actor actor, u8 palette);

/**
 * Play an animation by name.
 *
 * @param actor Actor handle
 * @param name Animation name (as defined in assets.yaml)
 * @return Non-zero if animation found, 0 if not
 */
u8 ActorPlayAnim(Actor actor, const char *name);

/**
 * Play an animation by index.
 *
 * @param actor Actor handle
 * @param index Animation index
 */
void ActorSetAnim(Actor actor, u8 index);

/**
 * Set a specific frame (stops animation playback).
 *
 * @param actor Actor handle
 * @param frame Frame index
 */
void ActorSetFrame(Actor actor, u16 frame);

/**
 * Check if a non-looping animation has finished.
 *
 * @return Non-zero if animation is done
 */
u8 ActorAnimDone(Actor actor);

/**
 * Set screen-space mode for UI elements.
 * Screen-space actors ignore camera position and zoom.
 * Position is in screen coordinates (0,0 = top-left).
 *
 * @param actor Actor handle
 * @param enabled Non-zero for screen-space, 0 for world-space
 */
void ActorSetScreenSpace(Actor actor, u8 enabled);

/** @} */

/*
 * Internal functions - used by other SDK modules.
 * Game code should not call these directly.
 */
void _ActorSystemInit(void);
void _ActorSystemUpdate(void);
u8 _ActorIsInScene(Actor actor);
u8 _ActorGetZ(Actor actor);
u8 _ActorIsScreenSpace(Actor actor);
void _ActorDraw(Actor actor, u16 first_sprite);
u8 _ActorGetSpriteCount(Actor actor);
void _ActorCollectPalettes(u8 *palette_mask);

#endif /* ACTOR_H */
