/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <ngmath.h>

// sin(i * 2 * PI / 256) * 32767, 256 entries covering 0-360 degrees
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

fixed NGSin(angle_t angle) {
    return ((fixed)sin_table[angle]) << 1;
}

fixed NGCos(angle_t angle) {
    return ((fixed)sin_table[(u8)(angle + 64)]) << 1;
}

angle_t NGAtan2(fixed y, fixed x) {
    if (x == 0 && y == 0) return 0;

    fixed abs_x = FIX_ABS(x);
    fixed abs_y = FIX_ABS(y);

    u8 octant = 0;
    fixed ratio;

    if (abs_x >= abs_y) {
        ratio = FIX_DIV(abs_y, abs_x);
    } else {
        ratio = FIX_DIV(abs_x, abs_y);
        octant = 1;
    }

    // Linear approximation: atan(x) ≈ x * 32 / PI
    angle_t angle = (angle_t)((ratio * 32) >> FIX_SHIFT);

    if (octant) angle = 64 - angle;
    if (x < 0) angle = 128 - angle;
    if (y < 0) angle = -angle;

    return angle;
}

u16 NGSqrt(u32 x) {
    if (x == 0) return 0;

    u32 result = 0;
    u32 bit = 1UL << 30;

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

    // For 16.16 fixed point: sqrt(x * 65536) = sqrt(x) * 256
    u32 val = (u32)x;
    u16 root = NGSqrt(val);
    return ((fixed)root) << (FIX_SHIFT / 2);
}

fixed NGLerp(fixed a, fixed b, fixed t) {
    return a + FIX_MUL(b - a, t);
}

fixed NGClamp(fixed x, fixed min, fixed max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

fixed NGVec2Length(NGVec2 v) {
    fixed len_sq = NGVec2LengthSq(v);
    return NGSqrtFix(len_sq);
}

NGVec2 NGVec2Normalize(NGVec2 v) {
    fixed len = NGVec2Length(v);
    if (len == 0) return (NGVec2){ 0, 0 };
    return (NGVec2){ FIX_DIV(v.x, len), FIX_DIV(v.y, len) };
}
