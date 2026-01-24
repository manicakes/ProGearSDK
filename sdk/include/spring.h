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
 * @code
 * acceleration = -stiffness * (value - target) - damping * velocity
 * @endcode
 *
 * - Higher stiffness = faster response
 * - Higher damping = less overshoot (critical damping = no overshoot)
 */

#ifndef SPRING_H
#define SPRING_H

#include <ng_types.h>
#include <ng_math.h>

/**
 * @defgroup spring Spring Physics
 * @ingroup sdk
 * @brief Smooth spring-based value animation.
 * @{
 */

/** @name Spring Presets
 * Good defaults for common use cases.
 * Tuned for 60fps, settling in ~200-300ms (12-18 frames).
 */
/** @{ */

/** Snappy with minimal overshoot - good for UI cursors */
#define NG_SPRING_SNAPPY_STIFFNESS FIX_FROM_FLOAT(0.35)
#define NG_SPRING_SNAPPY_DAMPING   FIX_FROM_FLOAT(0.65)

/** Bouncy with noticeable overshoot - good for playful UI */
#define NG_SPRING_BOUNCY_STIFFNESS FIX_FROM_FLOAT(0.25)
#define NG_SPRING_BOUNCY_DAMPING   FIX_FROM_FLOAT(0.45)

/** Smooth with no overshoot - good for subtle transitions */
#define NG_SPRING_SMOOTH_STIFFNESS FIX_FROM_FLOAT(0.20)
#define NG_SPRING_SMOOTH_DAMPING   FIX_FROM_FLOAT(0.80)

/** Quick snap - very fast, almost instant */
#define NG_SPRING_QUICK_STIFFNESS FIX_FROM_FLOAT(0.50)
#define NG_SPRING_QUICK_DAMPING   FIX_FROM_FLOAT(0.70)
/** @} */

/** @name 1D Spring */
/** @{ */

/** 1D spring state */
typedef struct NGSpring {
    fixed value;     /**< Current animated value */
    fixed velocity;  /**< Current velocity */
    fixed target;    /**< Target value to animate toward */
    fixed stiffness; /**< Spring constant (higher = faster) */
    fixed damping;   /**< Damping ratio (higher = less bouncy) */
} NGSpring;

/** Initialize spring with starting value and default snappy physics */
void NGSpringInit(NGSpring *spring, fixed initial);

/** Initialize spring with custom physics parameters */
void NGSpringInitEx(NGSpring *spring, fixed initial, fixed stiffness, fixed damping);

/** Set target value (spring will animate toward it) */
void NGSpringSetTarget(NGSpring *spring, fixed target);

/** Snap immediately to value (no animation) */
void NGSpringSnap(NGSpring *spring, fixed value);

/** Add impulse to velocity (for responsive input feel) */
void NGSpringImpulse(NGSpring *spring, fixed impulse);

/** Update spring physics (call once per frame) */
void NGSpringUpdate(NGSpring *spring);

/** Get current value */
static inline fixed NGSpringGet(NGSpring *spring) {
    return spring->value;
}

/** Get current value as integer */
static inline s16 NGSpringGetInt(NGSpring *spring) {
    return FIX_INT(spring->value);
}

/** Check if spring has settled (near target with low velocity) */
u8 NGSpringSettled(NGSpring *spring);
/** @} */

/** @name 2D Spring */
/** @{ */

/** 2D spring state (for positions) */
typedef struct NGSpring2D {
    NGSpring x; /**< X-axis spring */
    NGSpring y; /**< Y-axis spring */
} NGSpring2D;

/** Initialize 2D spring with starting position and default snappy physics */
void NGSpring2DInit(NGSpring2D *spring, fixed x, fixed y);

/** Initialize 2D spring with custom physics parameters */
void NGSpring2DInitEx(NGSpring2D *spring, fixed x, fixed y, fixed stiffness, fixed damping);

/** Set target position */
void NGSpring2DSetTarget(NGSpring2D *spring, fixed x, fixed y);

/** Snap immediately to position */
void NGSpring2DSnap(NGSpring2D *spring, fixed x, fixed y);

/** Update both axes (call once per frame) */
void NGSpring2DUpdate(NGSpring2D *spring);

/** Get current position as integers */
static inline void NGSpring2DGetInt(NGSpring2D *spring, s16 *out_x, s16 *out_y) {
    *out_x = FIX_INT(spring->x.value);
    *out_y = FIX_INT(spring->y.value);
}

/** Check if both axes have settled */
u8 NGSpring2DSettled(NGSpring2D *spring);
/** @} */

/** @} */ /* end of spring group */

#endif // SPRING_H
