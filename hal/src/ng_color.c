/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <ng_color.h>

NGColor NGColorBlend(NGColor a, NGColor b, u8 ratio) {
    if (ratio == 0)
        return a;
    if (ratio == 255)
        return b;

    u8 inv_ratio = 255 - ratio;

    u8 ra = NGColorGetRed(a);
    u8 ga = NGColorGetGreen(a);
    u8 ba = NGColorGetBlue(a);

    u8 rb = NGColorGetRed(b);
    u8 gb = NGColorGetGreen(b);
    u8 bb = NGColorGetBlue(b);

    u8 r = (u8)(((u16)ra * inv_ratio + (u16)rb * ratio + 128) >> 8);
    u8 g = (u8)(((u16)ga * inv_ratio + (u16)gb * ratio + 128) >> 8);
    u8 b_out = (u8)(((u16)ba * inv_ratio + (u16)bb * ratio + 128) >> 8);

    return NG_RGB(r, g, b_out);
}

NGColor NGColorInvert(NGColor c) {
    u8 r = 31 - NGColorGetRed(c);
    u8 g = 31 - NGColorGetGreen(c);
    u8 b = 31 - NGColorGetBlue(c);

    return NG_RGB(r, g, b);
}

NGColor NGColorGrayscale(NGColor c) {
    u8 r = NGColorGetRed(c);
    u8 g = NGColorGetGreen(c);
    u8 b = NGColorGetBlue(c);

    // Luminance: Y = 0.299*R + 0.587*G + 0.114*B
    u16 lum = (u16)((77 * r + 150 * g + 29 * b) >> 8);
    if (lum > 31)
        lum = 31;

    return NG_RGB(lum, lum, lum);
}

NGColor NGColorAdjustBrightness(NGColor c, s8 amount) {
    u8 abs_amount = (u8)(amount >= 0 ? amount : -amount);
    if (abs_amount > 31)
        abs_amount = 31;

    u8 r = NGColorGetRed(c);
    u8 g = NGColorGetGreen(c);
    u8 b = NGColorGetBlue(c);

    if (amount >= 0) {
        r = (u8)((r + abs_amount > 31) ? 31 : r + abs_amount);
        g = (u8)((g + abs_amount > 31) ? 31 : g + abs_amount);
        b = (u8)((b + abs_amount > 31) ? 31 : b + abs_amount);
    } else {
        r = (r > abs_amount) ? r - abs_amount : 0;
        g = (g > abs_amount) ? g - abs_amount : 0;
        b = (b > abs_amount) ? b - abs_amount : 0;
    }

    return NG_RGB(r, g, b);
}

NGColor NGColorFromHSV(u8 h, u8 s, u8 v) {
    if (s == 0) {
        u8 gray = v >> 3;
        return NG_RGB(gray, gray, gray);
    }

    u8 sector = h / 43;
    u8 remainder = (u8)((h - (sector * 43)) * 6);

    u8 p = (u8)(((u16)v * (255 - s)) >> 8);
    u8 q = (u8)(((u16)v * (255 - ((u16)s * remainder >> 8))) >> 8);
    u8 t = (u8)(((u16)v * (255 - ((u16)s * (255 - remainder) >> 8))) >> 8);

    v = v >> 3;
    p = p >> 3;
    q = q >> 3;
    t = t >> 3;

    switch (sector) {
        case 0:
            return NG_RGB(v, t, p);
        case 1:
            return NG_RGB(q, v, p);
        case 2:
            return NG_RGB(p, v, t);
        case 3:
            return NG_RGB(p, q, v);
        case 4:
            return NG_RGB(t, p, v);
        default:
            return NG_RGB(v, p, q);
    }
}
