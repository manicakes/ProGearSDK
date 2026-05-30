/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <ng_math.h>

// sin(i * 2 * PI / 256) * 65536 (16.16 fixed), 256 entries covering 0-360
// degrees. Stored at full scale so NGSin/NGCos are a plain table lookup and
// peak at exactly +/-FIX_ONE.
static const fixed sin_table[256] = {
    0,      1608,   3216,   4821,   6424,   8022,   9616,   11204,  12785,  14359,  15924,  17479,
    19024,  20557,  22078,  23586,  25080,  26558,  28020,  29466,  30893,  32303,  33692,  35062,
    36410,  37736,  39040,  40320,  41576,  42806,  44011,  45190,  46341,  47464,  48559,  49624,
    50660,  51665,  52639,  53581,  54491,  55368,  56212,  57022,  57798,  58538,  59244,  59914,
    60547,  61145,  61705,  62228,  62714,  63162,  63572,  63944,  64277,  64571,  64827,  65043,
    65220,  65358,  65457,  65516,  65536,  65516,  65457,  65358,  65220,  65043,  64827,  64571,
    64277,  63944,  63572,  63162,  62714,  62228,  61705,  61145,  60547,  59914,  59244,  58538,
    57798,  57022,  56212,  55368,  54491,  53581,  52639,  51665,  50660,  49624,  48559,  47464,
    46341,  45190,  44011,  42806,  41576,  40320,  39040,  37736,  36410,  35062,  33692,  32303,
    30893,  29466,  28020,  26558,  25080,  23586,  22078,  20557,  19024,  17479,  15924,  14359,
    12785,  11204,  9616,   8022,   6424,   4821,   3216,   1608,   0,      -1608,  -3216,  -4821,
    -6424,  -8022,  -9616,  -11204, -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
    -25080, -26558, -28020, -29466, -30893, -32303, -33692, -35062, -36410, -37736, -39040, -40320,
    -41576, -42806, -44011, -45190, -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
    -54491, -55368, -56212, -57022, -57798, -58538, -59244, -59914, -60547, -61145, -61705, -62228,
    -62714, -63162, -63572, -63944, -64277, -64571, -64827, -65043, -65220, -65358, -65457, -65516,
    -65536, -65516, -65457, -65358, -65220, -65043, -64827, -64571, -64277, -63944, -63572, -63162,
    -62714, -62228, -61705, -61145, -60547, -59914, -59244, -58538, -57798, -57022, -56212, -55368,
    -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464, -46341, -45190, -44011, -42806,
    -41576, -40320, -39040, -37736, -36410, -35062, -33692, -32303, -30893, -29466, -28020, -26558,
    -25080, -23586, -22078, -20557, -19024, -17479, -15924, -14359, -12785, -11204, -9616,  -8022,
    -6424,  -4821,  -3216,  -1608,
};

fixed NGSin(angle_t angle) {
    return sin_table[angle];
}

fixed NGCos(angle_t angle) {
    return sin_table[(u8)(angle + 64)];
}

angle_t NGAtan2(fixed y, fixed x) {
    if (x == 0 && y == 0)
        return 0;

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

    if (octant)
        angle = 64 - angle;
    if (x < 0)
        angle = 128 - angle;
    if (y < 0)
        angle = -angle;

    return angle;
}

u16 NGSqrt(u32 x) {
    if (x == 0)
        return 0;

    u32 result = 0;
    u32 bit = 1UL << 30;

    while (bit > x)
        bit >>= 2;

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
    if (x <= 0)
        return 0;

    // For 16.16 fixed point: sqrt(x * 65536) = sqrt(x) * 256
    u32 val = (u32)x;
    u16 root = NGSqrt(val);
    return ((fixed)root) << (FIX_SHIFT / 2);
}

fixed NGLerp(fixed a, fixed b, fixed t) {
    return a + FIX_MUL(b - a, t);
}

fixed NGClamp(fixed x, fixed min, fixed max) {
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

fixed NGVec2Length(NGVec2 v) {
    fixed len_sq = NGVec2LengthSq(v);
    return NGSqrtFix(len_sq);
}

NGVec2 NGVec2Normalize(NGVec2 v) {
    fixed len = NGVec2Length(v);
    if (len == 0)
        return (NGVec2){0, 0};
    return (NGVec2){FIX_DIV(v.x, len), FIX_DIV(v.y, len)};
}
