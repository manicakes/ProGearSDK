/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hitbox.h
 * @brief World-space collision boxes and strike resolution for actors.
 *
 * Boxes are authored per animation frame in assets.yaml and compiled into the
 * visual asset (see NGBox in visual.h). This module resolves them into scene
 * coordinates - applying the actor's position, anchor and facing - and answers
 * the questions games actually ask of them.
 *
 * The module reports geometry, not rules. It will tell you that an attack met
 * a guard box rather than a hurtbox; whether that means a parry, a chip-damage
 * block or nothing at all is the game's decision. That separation is what lets
 * the same code serve a fighting game's parry, a shmup's graze window and a
 * racer's kerb sensor.
 *
 * Typical use, once per frame, from the attacker's side:
 * @code
 * if (NGActorHasBox(hero, NG_BOX_HIT)) {           // cheap: one mask test
 *     for (u8 i = 0; i < enemy_count; i++) {
 *         NGHit hit;
 *         switch (NGCombatStrike(hero, enemies[i], &hit)) {
 *             case NG_STRIKE_GUARD:
 *                 // deflected - grade it on how deep into the window we are
 *                 if (NGActorGetAnimFrame(enemies[i]) < 3) perfect_parry(i);
 *                 else                                     blocked(i);
 *                 break;
 *             case NG_STRIKE_HIT:
 *                 damage(i, &hit);
 *                 break;
 *             case NG_STRIKE_NONE:
 *                 break;
 *         }
 *     }
 * }
 * @endcode
 */

#ifndef NG_HITBOX_H
#define NG_HITBOX_H

#include <ng_types.h>
#include <actor.h>
#include <visual.h>

/**
 * @defgroup hitbox Hitboxes
 * @ingroup sdk
 * @brief Per-frame collision boxes, overlap queries and strike resolution.
 * @{
 */

/** @name Types */
/** @{ */

/**
 * An axis-aligned rectangle in scene coordinates, in whole pixels.
 * Pixels rather than fixed-point: boxes are authored per frame at pixel
 * resolution, and overlap tests are far cheaper on integers.
 */
typedef struct {
    s16 x, y;
    u16 w, h;
} NGRect;

/**
 * The outcome of one attacker-defender test.
 *
 * Ordering matters and is guaranteed: a guard box is tested before a hurtbox,
 * so when both overlap on the same frame the guard wins. Without that
 * guarantee a parry would fail exactly on the frames where it is supposed to
 * work, and every game would have to re-derive the rule.
 */
typedef enum {
    NG_STRIKE_NONE = 0, /**< No contact */
    NG_STRIKE_HIT = 1,  /**< A hitbox met a hurtbox */
    NG_STRIKE_GUARD = 2 /**< A hitbox met a guard box - blocked, parried, deflected */
} NGStrikeResult;

/**
 * Details of a contact, filled in by NGCombatStrike().
 */
typedef struct {
    NGRect overlap; /**< Intersection of the two boxes - a good spot for an effect */
    s16 contact_x;  /**< Centre of the overlap, convenient for spawning sparks */
    s16 contact_y;
    u8 defender_kind; /**< Which kind intercepted: NG_BOX_HURT or NG_BOX_GUARD */
} NGHit;
/** @} */

/** @name Box Queries */
/** @{ */

/**
 * Test whether the actor's current frame carries any of @p kinds.
 *
 * This is a single mask test against the frame's precomputed union, so it is
 * the right way to skip work: most frames of most animations carry no hitbox.
 *
 * @param actor Actor handle
 * @param kinds One or more NGBoxKind values, OR-ed together
 * @return 1 if the current frame has at least one matching box
 */
u8 NGActorHasBox(NGActorHandle actor, u8 kinds);

/**
 * Count the actor's boxes of the given kinds on its current frame.
 *
 * @param actor Actor handle
 * @param kinds One or more NGBoxKind values, OR-ed together
 * @return Number of matching boxes
 */
u8 NGActorBoxCount(NGActorHandle actor, u8 kinds);

/**
 * Resolve one of the actor's boxes into scene coordinates.
 *
 * Applies the actor's position, anchor and horizontal flip. @p index counts
 * only boxes matching @p kinds, so asking for index 1 of NG_BOX_HIT gives the
 * second hitbox regardless of how many other boxes the frame has.
 *
 * @param actor Actor handle
 * @param kinds One or more NGBoxKind values, OR-ed together
 * @param index Which matching box (0-based)
 * @param out Receives the resolved rectangle
 * @return 1 if a box was found, 0 otherwise (@p out untouched)
 */
u8 NGActorGetBox(NGActorHandle actor, u8 kinds, u8 index, NGRect *out);

/**
 * Get the actor's frame index within its current animation.
 *
 * Useful for grading timing windows - how many frames into a parry an attack
 * landed, for instance - without the game tracking a parallel timer.
 *
 * @param actor Actor handle
 * @return Frame index within the current animation, 0 if the actor is invalid
 */
u16 NGActorGetAnimFrame(NGActorHandle actor);
/** @} */

/** @name Overlap */
/** @{ */

/**
 * Test two rectangles for overlap.
 *
 * @param a First rectangle
 * @param b Second rectangle
 * @param out Receives the intersection, or NULL if not needed
 * @return 1 if they overlap
 */
u8 NGRectOverlap(const NGRect *a, const NGRect *b, NGRect *out);

/**
 * Test any box of @p kinds_a on @p a against any box of @p kinds_b on @p b.
 *
 * The general-purpose overlap query: pushboxes against pushboxes, a trigger
 * against a body, a bullet against a ship.
 *
 * @param a First actor
 * @param kinds_a Kinds to consider on @p a
 * @param b Second actor
 * @param kinds_b Kinds to consider on @p b
 * @param out Receives the intersection of the first overlapping pair, or NULL
 * @return 1 if any pair overlaps
 */
u8 NGActorOverlap(NGActorHandle a, u8 kinds_a, NGActorHandle b, u8 kinds_b, NGRect *out);
/** @} */

/** @name Strikes */
/** @{ */

/**
 * Begin a new attack instance for @p attacker.
 *
 * Clears the record of who this attacker has already connected with, so the
 * next attack can hit them again. Call it when an attack animation starts -
 * not every frame.
 *
 * Without this, a hitbox that stays active for six frames connects six times.
 * Every action game needs the rule; keeping it here means no game has to
 * rediscover it.
 *
 * @param attacker Actor handle
 */
void NGCombatBeginAttack(NGActorHandle attacker);

/**
 * Resolve @p attacker's hitboxes against @p defender.
 *
 * Tests the attacker's NG_BOX_HIT boxes against the defender's guard boxes
 * first and hurtboxes second, so a guard shadows a hurtbox on frames where
 * both are active. Returns at most one result per attack instance per
 * defender: once this pair has resolved, further calls return NG_STRIKE_NONE
 * until the next NGCombatBeginAttack().
 *
 * @param attacker Actor handle with active hitboxes
 * @param defender Actor handle to test against
 * @param out Receives contact details, or NULL if not needed
 * @return What was struck
 */
NGStrikeResult NGCombatStrike(NGActorHandle attacker, NGActorHandle defender, NGHit *out);

/**
 * Forget that @p attacker has connected with @p defender.
 *
 * Rarely needed - for a lingering hazard that should damage repeatedly on a
 * timer, clear the pair each time the timer elapses rather than starting a new
 * attack instance.
 *
 * @param attacker Actor handle
 * @param defender Actor handle
 */
void NGCombatClearPair(NGActorHandle attacker, NGActorHandle defender);
/** @} */

/** @} */ /* end of hitbox group */

#endif /* NG_HITBOX_H */
