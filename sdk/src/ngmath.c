/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ngmath.c - Fixed-point math implementation

#include <ngmath.h>

// === Sine Lookup Table ===
// 256 entries covering 0-360 degrees
// Values are in 16.16 fixed point (range -65536 to +65536 = -1.0 to +1.0)
// Generated with: sin(i * 2 * PI / 256) * 65536

static const fixed sin_table[256] = {
         0,    804,   1607,   2410,   3211,   4011,   4807,   5601,
      6392,   7179,   7961,   8739,   9511,  10278,  11038,  11792,
     12539,  13278,  14009,  14732,  15446,  16150,  16845,  17530,
     18204,  18867,  19519,  20159,  20787,  21402,  22004,  22594,
     23169,  23731,  24278,  24811,  25329,  25831,  26318,  26789,
     27244,  27683,  28105,  28510,  28897,  29268,  29621,  29955,
     30272,  30571,  30851,  31113,  31356,  31580,  31785,  31970,
     32137,  32284,  32412,  32520,  32609,  32678,  32727,  32757,
     32767,  32757,  32727,  32678,  32609,  32520,  32412,  32284,
     32137,  31970,  31785,  31580,  31356,  31113,  30851,  30571,
     30272,  29955,  29621,  29268,  28897,  28510,  28105,  27683,
     27244,  26789,  26318,  25831,  25329,  24811,  24278,  23731,
     23169,  22594,  22004,  21402,  20787,  20159,  19519,  18867,
     18204,  17530,  16845,  16150,  15446,  14732,  14009,  13278,
     12539,  11792,  11038,  10278,   9511,   8739,   7961,   7179,
      6392,   5601,   4807,   4011,   3211,   2410,   1607,    804,
         0,   -804,  -1607,  -2410,  -3211,  -4011,  -4807,  -5601,
     -6392,  -7179,  -7961,  -8739,  -9511, -10278, -11038, -11792,
    -12539, -13278, -14009, -14732, -15446, -16150, -16845, -17530,
    -18204, -18867, -19519, -20159, -20787, -21402, -22004, -22594,
    -23169, -23731, -24278, -24811, -25329, -25831, -26318, -26789,
    -27244, -27683, -28105, -28510, -28897, -29268, -29621, -29955,
    -30272, -30571, -30851, -31113, -31356, -31580, -31785, -31970,
    -32137, -32284, -32412, -32520, -32609, -32678, -32727, -32757,
    -32767, -32757, -32727, -32678, -32609, -32520, -32412, -32284,
    -32137, -31970, -31785, -31580, -31356, -31113, -30851, -30571,
    -30272, -29955, -29621, -29268, -28897, -28510, -28105, -27683,
    -27244, -26789, -26318, -25831, -25329, -24811, -24278, -23731,
    -23169, -22594, -22004, -21402, -20787, -20159, -19519, -18867,
    -18204, -17530, -16845, -16150, -15446, -14732, -14009, -13278,
    -12539, -11792, -11038, -10278,  -9511,  -8739,  -7961,  -7179,
     -6392,  -5601,  -4807,  -4011,  -3211,  -2410,  -1607,   -804,
};

// Table: sin(i * 2 * PI / 256) * 32767
// [0]=0, [64]=32767, [128]=0, [192]=-32767

fixed NGSin(angle_t angle) {
    // Table values are 16-bit, shift up to 16.16
    return ((fixed)sin_table[angle]) << 1;
}

fixed NGCos(angle_t angle) {
    // Cosine is sine shifted by 90 degrees (64 in our 256-unit system)
    return ((fixed)sin_table[(u8)(angle + 64)]) << 1;
}

// === Atan2 Approximation ===
// Uses octant decomposition and linear approximation
// Returns angle_t (0-255)

angle_t NGAtan2(fixed y, fixed x) {
    if (x == 0 && y == 0) return 0;

    fixed abs_x = FIX_ABS(x);
    fixed abs_y = FIX_ABS(y);

    // Determine octant and compute ratio
    u8 octant = 0;
    fixed ratio;

    if (abs_x >= abs_y) {
        ratio = FIX_DIV(abs_y, abs_x);
    } else {
        ratio = FIX_DIV(abs_x, abs_y);
        octant = 1;
    }

    // Linear approximation for arctan in first octant
    // atan(x) ≈ x * 32 / PI ≈ x * 10.19 for small x
    // Simplified: angle ≈ ratio * 32 / 65536
    angle_t angle = (angle_t)((ratio * 32) >> FIX_SHIFT);

    // Adjust for octant
    if (octant) angle = 64 - angle;

    // Adjust for quadrant
    if (x < 0) angle = 128 - angle;
    if (y < 0) angle = -angle;  // Wraps correctly for u8

    return angle;
}

// === Square Root ===
// Integer square root using binary search

u16 NGSqrt(u32 x) {
    if (x == 0) return 0;

    u32 result = 0;
    u32 bit = 1UL << 30;  // Start with highest bit

    // Find highest bit that fits
    while (bit > x) bit >>= 2;

    while (bit != 0) {
        if (x >= result + bit) {
            x -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (u16)result;
}

fixed NGSqrtFix(fixed x) {
    if (x <= 0) return 0;

    // For 16.16 fixed point, we need to adjust
    // sqrt(x * 65536) = sqrt(x) * 256
    // So: result = sqrt(x as integer) << 8
    u32 val = (u32)x;
    u16 root = NGSqrt(val);

    // Shift to maintain fixed point precision
    return ((fixed)root) << (FIX_SHIFT / 2);
}

// === Utility Functions ===

fixed NGLerp(fixed a, fixed b, fixed t) {
    return a + FIX_MUL(b - a, t);
}

fixed NGClamp(fixed x, fixed min, fixed max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// === Vector Functions ===

fixed NGVec2Length(NGVec2 v) {
    fixed len_sq = NGVec2LengthSq(v);
    return NGSqrtFix(len_sq);
}

NGVec2 NGVec2Normalize(NGVec2 v) {
    fixed len = NGVec2Length(v);
    if (len == 0) return (NGVec2){ 0, 0 };
    return (NGVec2){ FIX_DIV(v.x, len), FIX_DIV(v.y, len) };
}
