/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file spring.h
 * @brief Spring physics animation system.
 *
 * Springs provide smooth, natural-feeling animations with overshoot and settling.
 * Use for UI elements, camera movement, actor motion, or any value that should
 * animate smoothly toward a target.
 *
 * The spring physics model:
 *   acceleration = -stiffness * (value - target) - damping * velocity
 *
 * Higher stiffness = faster response
 * Higher damping = less overshoot (critical damping = no overshoot)
 */

#ifndef SPRING_H
#define SPRING_H

#include <types.h>
#include <ngmath.h>

/** @defgroup springpresets Spring Presets
 *  @brief Good defaults for common use cases.
 *  Tuned for 60fps, settling in ~200-300ms (12-18 frames)
 *  @{
 */

/** Snappy with minimal overshoot - good for UI cursors */
#define SPRING_SNAPPY_STIFFNESS FIX_FROM_FLOAT(0.35)
#define SPRING_SNAPPY_DAMPING   FIX_FROM_FLOAT(0.65)

/** Bouncy with noticeable overshoot - good for playful UI */
#define SPRING_BOUNCY_STIFFNESS FIX_FROM_FLOAT(0.25)
#define SPRING_BOUNCY_DAMPING   FIX_FROM_FLOAT(0.45)

/** Smooth with no overshoot - good for subtle transitions */
#define SPRING_SMOOTH_STIFFNESS FIX_FROM_FLOAT(0.20)
#define SPRING_SMOOTH_DAMPING   FIX_FROM_FLOAT(0.80)

/** Quick snap - very fast, almost instant */
#define SPRING_QUICK_STIFFNESS FIX_FROM_FLOAT(0.50)
#define SPRING_QUICK_DAMPING   FIX_FROM_FLOAT(0.70)

/** @} */

/** @defgroup spring1d 1D Spring
 *  @{
 */

typedef struct Spring {
    fixed value;     /**< Current animated value */
    fixed velocity;  /**< Current velocity */
    fixed target;    /**< Target value to animate toward */
    fixed stiffness; /**< Spring constant (higher = faster) */
    fixed damping;   /**< Damping ratio (higher = less bouncy) */
} Spring;

/** Initialize spring with starting value and default snappy physics */
void SpringInit(Spring *spring, fixed initial);

/** Initialize spring with custom physics parameters */
void SpringInitEx(Spring *spring, fixed initial, fixed stiffness, fixed damping);

/** Set target value (spring will animate toward it) */
void SpringSetTarget(Spring *spring, fixed target);

/** Snap immediately to value (no animation) */
void SpringSnap(Spring *spring, fixed value);

/** Add impulse to velocity (for responsive input feel) */
void SpringImpulse(Spring *spring, fixed impulse);

/** Update spring physics (call once per frame) */
void SpringUpdate(Spring *spring);

/** Get current value */
static inline fixed SpringGet(Spring *spring) {
    return spring->value;
}

/** Get current value as integer */
static inline s16 SpringGetInt(Spring *spring) {
    return FIX_INT(spring->value);
}

/** Check if spring has settled (near target with low velocity) */
u8 SpringSettled(Spring *spring);

/** @} */

/** @defgroup spring2d 2D Spring
 *  @{
 */

typedef struct Spring2D {
    Spring x;
    Spring y;
} Spring2D;

/** Initialize 2D spring with starting position and default snappy physics */
void Spring2DInit(Spring2D *spring, fixed x, fixed y);

/** Initialize 2D spring with custom physics parameters */
void Spring2DInitEx(Spring2D *spring, fixed x, fixed y, fixed stiffness, fixed damping);

/** Set target position */
void Spring2DSetTarget(Spring2D *spring, fixed x, fixed y);

/** Snap immediately to position */
void Spring2DSnap(Spring2D *spring, fixed x, fixed y);

/** Update both axes (call once per frame) */
void Spring2DUpdate(Spring2D *spring);

/** Get current position as integers */
static inline void Spring2DGetInt(Spring2D *spring, s16 *out_x, s16 *out_y) {
    *out_x = FIX_INT(spring->x.value);
    *out_y = FIX_INT(spring->y.value);
}

/** Check if both axes have settled */
u8 Spring2DSettled(Spring2D *spring);

/** @} */

#endif /* SPRING_H */
