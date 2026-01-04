/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// color.c - Color manipulation functions

#include <color.h>

// Blend two colors (ratio: 0=all a, 255=all b)
NGColor NGColorBlend(NGColor a, NGColor b, u8 ratio) {
    // Handle edge cases to avoid precision loss
    if (ratio == 0) return a;
    if (ratio == 255) return b;

    u8 inv_ratio = 255 - ratio;

    // Extract components from both colors
    u8 ra = NGColorGetRed(a);
    u8 ga = NGColorGetGreen(a);
    u8 ba = NGColorGetBlue(a);

    u8 rb = NGColorGetRed(b);
    u8 gb = NGColorGetGreen(b);
    u8 bb = NGColorGetBlue(b);

    // Blend each component (add 128 for rounding)
    u8 r = ((u16)ra * inv_ratio + (u16)rb * ratio + 128) >> 8;
    u8 g = ((u16)ga * inv_ratio + (u16)gb * ratio + 128) >> 8;
    u8 b_out = ((u16)ba * inv_ratio + (u16)bb * ratio + 128) >> 8;

    return NG_RGB5(r, g, b_out);
}

// Darken a color (amount: 0=no change, 31=black)
NGColor NGColorDarken(NGColor c, u8 amount) {
    if (amount > 31) amount = 31;

    u8 r = NGColorGetRed(c);
    u8 g = NGColorGetGreen(c);
    u8 b = NGColorGetBlue(c);

    r = (r > amount) ? r - amount : 0;
    g = (g > amount) ? g - amount : 0;
    b = (b > amount) ? b - amount : 0;

    return NG_RGB5(r, g, b);
}

// Lighten a color (amount: 0=no change, 31=white)
NGColor NGColorLighten(NGColor c, u8 amount) {
    if (amount > 31) amount = 31;

    u8 r = NGColorGetRed(c);
    u8 g = NGColorGetGreen(c);
    u8 b = NGColorGetBlue(c);

    r = (r + amount > 31) ? 31 : r + amount;
    g = (g + amount > 31) ? 31 : g + amount;
    b = (b + amount > 31) ? 31 : b + amount;

    return NG_RGB5(r, g, b);
}

// Invert a color
NGColor NGColorInvert(NGColor c) {
    u8 r = 31 - NGColorGetRed(c);
    u8 g = 31 - NGColorGetGreen(c);
    u8 b = 31 - NGColorGetBlue(c);

    return NG_RGB5(r, g, b);
}

// Convert to grayscale using luminance weights
// Approximate: Y = 0.299*R + 0.587*G + 0.114*B
// Using integer approximation: Y = (77*R + 150*G + 29*B) >> 8
NGColor NGColorGrayscale(NGColor c) {
    u8 r = NGColorGetRed(c);
    u8 g = NGColorGetGreen(c);
    u8 b = NGColorGetBlue(c);

    // Scale up to get better precision, then scale down
    u16 lum = (77 * r + 150 * g + 29 * b) >> 8;
    if (lum > 31) lum = 31;

    return NG_RGB5(lum, lum, lum);
}

// Adjust brightness (-31 to +31)
NGColor NGColorAdjustBrightness(NGColor c, s8 amount) {
    if (amount >= 0) {
        return NGColorLighten(c, (u8)amount);
    } else {
        return NGColorDarken(c, (u8)(-amount));
    }
}

// Create a color from HSV values
// h: 0-255 (hue), s: 0-255 (saturation), v: 0-255 (value)
NGColor NGColorFromHSV(u8 h, u8 s, u8 v) {
    if (s == 0) {
        // Grayscale
        u8 gray = v >> 3;  // Convert 0-255 to 0-31
        return NG_RGB5(gray, gray, gray);
    }

    // Sector (0-5)
    u8 sector = h / 43;
    u8 remainder = (h - (sector * 43)) * 6;

    u8 p = ((u16)v * (255 - s)) >> 8;
    u8 q = ((u16)v * (255 - ((u16)s * remainder >> 8))) >> 8;
    u8 t = ((u16)v * (255 - ((u16)s * (255 - remainder) >> 8))) >> 8;

    // Convert to 5-bit range
    v = v >> 3;
    p = p >> 3;
    q = q >> 3;
    t = t >> 3;

    switch (sector) {
        case 0:  return NG_RGB5(v, t, p);
        case 1:  return NG_RGB5(q, v, p);
        case 2:  return NG_RGB5(p, v, t);
        case 3:  return NG_RGB5(p, q, v);
        case 4:  return NG_RGB5(t, p, v);
        default: return NG_RGB5(v, p, q);
    }
}
