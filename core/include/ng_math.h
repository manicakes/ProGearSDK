/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_math.h
 * @brief Fixed-point math types and functions.
 *
 * Provides fixed-point arithmetic for systems without FPU.
 * Two formats are supported:
 * - `fixed` (16.16): 32-bit, range ~32767, precision 0.000015
 * - `fixed16` (8.8): 16-bit, range ~127, precision 0.004
 */

#ifndef NG_MATH_H
#define NG_MATH_H

#include <ng_types.h>

/**
 * @defgroup math Fixed-Point Math
 * @ingroup hal
 * @brief Fixed-point arithmetic, trigonometry, and vectors.
 * @{
 */

/** @name Fixed-Point Types */
/** @{ */

/** 16.16 fixed-point (32-bit): range +/-32767.9999 */
typedef s32 fixed;

/** 8.8 fixed-point (16-bit): range +/-127.99 */
typedef s16 fixed16;
/** @} */

/** @name 16.16 Fixed-Point Math */
/** @{ */

#define FIX_SHIFT 16                     /**< Fractional bits in fixed */
#define FIX_ONE   (1 << FIX_SHIFT)       /**< 1.0 in fixed format */
#define FIX_HALF  (1 << (FIX_SHIFT - 1)) /**< 0.5 in fixed format */

/** Convert integer or float literal to fixed: FIX(3), FIX(-1), FIX(0.5) */
#define FIX(x) ((fixed)((x) * FIX_ONE))

/** Convert fixed to integer (truncates toward zero) */
#define FIX_INT(x) ((s16)((x) >> FIX_SHIFT))

/** Convert fixed to integer (rounds to nearest) */
#define FIX_ROUND(x) ((s16)(((x) + FIX_HALF) >> FIX_SHIFT))

/**
 * Multiply two fixed values.
 * Optimized for m68000: uses four 16x16 multiplies instead of 64-bit math.
 * @param a First operand
 * @param b Second operand
 * @return a * b in fixed format
 */
static inline fixed FIX_MUL(fixed a, fixed b) {
    s16 a_hi = (s16)(a >> 16);
    u16 a_lo = (u16)a;
    s16 b_hi = (s16)(b >> 16);
    u16 b_lo = (u16)b;
    s32 result = ((s32)a_hi * b_hi) << 16;
    result += (s32)a_hi * b_lo;
    result += (s32)a_lo * b_hi;
    result += ((u32)a_lo * b_lo) >> 16;
    return result;
}

/** Divide two fixed values (slow - prefer multiply by reciprocal) */
#define FIX_DIV(a, b) ((fixed)((((long long)(a)) << FIX_SHIFT) / (b)))

/** Absolute value of fixed */
#define FIX_ABS(x) ((x) < 0 ? -(x) : (x))

/** Sign of fixed value: returns -FIX_ONE, 0, or FIX_ONE */
#define FIX_SIGN(x) ((x) > 0 ? FIX_ONE : ((x) < 0 ? -FIX_ONE : 0))
/** @} */

/** @name 8.8 Fixed-Point Math */
/** @{ */

#define FIX16_SHIFT 8                        /**< Fractional bits in fixed16 */
#define FIX16_ONE   (1 << FIX16_SHIFT)       /**< 1.0 in fixed16 format */
#define FIX16_HALF  (1 << (FIX16_SHIFT - 1)) /**< 0.5 in fixed16 format */

/** Convert integer to fixed16 */
#define FIX16(x) ((fixed16)((x) << FIX16_SHIFT))

/** Convert fixed16 to integer (truncates) */
#define FIX16_INT(x) ((s8)((x) >> FIX16_SHIFT))

/** Convert fixed16 to integer (rounds) */
#define FIX16_ROUND(x) ((s8)(((x) + FIX16_HALF) >> FIX16_SHIFT))

/** Multiply two fixed16 values */
#define FIX16_MUL(a, b) ((fixed16)(((s32)(a) * (s32)(b)) >> FIX16_SHIFT))

/** Divide two fixed16 values */
#define FIX16_DIV(a, b) ((fixed16)(((s32)(a) << FIX16_SHIFT) / (b)))

/** Convert fixed (16.16) to fixed16 (8.8) */
#define FIX_TO_FIX16(x) ((fixed16)((x) >> (FIX_SHIFT - FIX16_SHIFT)))

/** Convert fixed16 (8.8) to fixed (16.16) */
#define FIX16_TO_FIX(x) ((fixed)((x) << (FIX_SHIFT - FIX16_SHIFT)))
/** @} */

/** @name Angles (256 units per circle) */
/** @{ */

/** Angle type: 0-255 represents 0-360 degrees */
typedef u8 angle_t;

#define ANGLE_0   0   /**< 0 degrees */
#define ANGLE_45  32  /**< 45 degrees */
#define ANGLE_90  64  /**< 90 degrees */
#define ANGLE_135 96  /**< 135 degrees */
#define ANGLE_180 128 /**< 180 degrees */
#define ANGLE_225 160 /**< 225 degrees */
#define ANGLE_270 192 /**< 270 degrees */
#define ANGLE_315 224 /**< 315 degrees */
#define ANGLE_360 0   /**< 360 degrees (wraps to 0) */
/** @} */

/** @name Trigonometry */
/** @{ */

/**
 * Get sine of angle.
 * @param angle Angle (0-255)
 * @return Sine value as fixed (-FIX_ONE to +FIX_ONE)
 */
fixed NGSin(angle_t angle);

/**
 * Get cosine of angle.
 * @param angle Angle (0-255)
 * @return Cosine value as fixed (-FIX_ONE to +FIX_ONE)
 */
fixed NGCos(angle_t angle);

/**
 * Compute angle from vector.
 * @param y Y component (fixed)
 * @param x X component (fixed)
 * @return Angle from positive X axis (0-255)
 */
angle_t NGAtan2(fixed y, fixed x);
/** @} */

/** @name Math Utilities */
/** @{ */

/**
 * Integer square root.
 * @param x Input value
 * @return Floor of square root
 */
u16 NGSqrt(u32 x);

/**
 * Fixed-point square root.
 * @param x Input value (fixed)
 * @return Square root (fixed)
 */
fixed NGSqrtFix(fixed x);

/**
 * Linear interpolation.
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0=a, FIX_ONE=b)
 * @return Interpolated value
 */
fixed NGLerp(fixed a, fixed b, fixed t);

/**
 * Clamp value to range.
 * @param x Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
fixed NGClamp(fixed x, fixed min, fixed max);

/** Return minimum of a and b */
#define NG_MIN(a, b) ((a) < (b) ? (a) : (b))

/** Return maximum of a and b */
#define NG_MAX(a, b) ((a) > (b) ? (a) : (b))
/** @} */

/** @name 2D Vectors */
/** @{ */

/** 2D vector with fixed-point components */
typedef struct {
    fixed x; /**< X component */
    fixed y; /**< Y component */
} NGVec2;

/** Add two vectors */
static inline NGVec2 NGVec2Add(NGVec2 a, NGVec2 b) {
    return (NGVec2){a.x + b.x, a.y + b.y};
}

/** Subtract two vectors */
static inline NGVec2 NGVec2Sub(NGVec2 a, NGVec2 b) {
    return (NGVec2){a.x - b.x, a.y - b.y};
}

/** Scale vector by scalar */
static inline NGVec2 NGVec2Scale(NGVec2 v, fixed s) {
    return (NGVec2){FIX_MUL(v.x, s), FIX_MUL(v.y, s)};
}

/** Dot product of two vectors */
static inline fixed NGVec2Dot(NGVec2 a, NGVec2 b) {
    return FIX_MUL(a.x, b.x) + FIX_MUL(a.y, b.y);
}

/** Squared length (avoids sqrt) */
static inline fixed NGVec2LengthSq(NGVec2 v) {
    return FIX_MUL(v.x, v.x) + FIX_MUL(v.y, v.y);
}

/**
 * Vector length.
 * @param v Input vector
 * @return Length (fixed)
 */
fixed NGVec2Length(NGVec2 v);

/**
 * Normalize vector to unit length.
 * @param v Input vector
 * @return Unit vector in same direction
 */
NGVec2 NGVec2Normalize(NGVec2 v);
/** @} */

/** @} */ /* end of math group */

#endif /* NG_MATH_H */
