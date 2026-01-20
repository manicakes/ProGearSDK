/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <fix.h>
#include <hw/fix.h>
#include <stdarg.h>

static u16 font_base = 0;

void NGFixPut(u8 x, u8 y, u16 tile, u8 palette) {
    hw_fix_put(x, y, tile, palette);
}

void NGFixClear(u8 x, u8 y, u8 w, u8 h) {
    hw_fix_clear(x, y, w, h);
}

void NGFixClearAll(void) {
    hw_fix_clear_all();
}

NGFixLayout NGFixLayoutAlign(NGFixHAlign h, NGFixVAlign v) {
    NGFixLayout layout = {h, v, 0, 0};
    return layout;
}

NGFixLayout NGFixLayoutOffset(NGFixHAlign h, NGFixVAlign v, s8 offset_x, s8 offset_y) {
    NGFixLayout layout = {h, v, offset_x, offset_y};
    return layout;
}

void NGTextSetFont(u16 font_base_tile) {
    font_base = font_base_tile;
}

static u8 str_len(const char *str) {
    u8 len = 0;
    while (*str++)
        len++;
    return len;
}

static u8 calc_x(NGFixLayout layout, u8 text_len) {
    s16 x;
    switch (layout.h_align) {
        case NG_ALIGN_CENTER:
            x = (s16)((NG_FIX_SAFE_LEFT + NG_FIX_SAFE_RIGHT + 1 - text_len) / 2);
            break;
        case NG_ALIGN_RIGHT:
            x = NG_FIX_SAFE_RIGHT + 1 - text_len;
            break;
        case NG_ALIGN_LEFT:
        default:
            x = NG_FIX_SAFE_LEFT;
            break;
    }
    x += layout.offset_x;
    if (x < 0)
        x = 0;
    if (x >= NG_FIX_WIDTH)
        x = NG_FIX_WIDTH - 1;
    return (u8)x;
}

static u8 calc_y(NGFixLayout layout) {
    s16 y;
    switch (layout.v_align) {
        case NG_ALIGN_MIDDLE:
            y = (NG_FIX_SAFE_TOP + NG_FIX_SAFE_BOTTOM) / 2;
            break;
        case NG_ALIGN_BOTTOM:
            y = NG_FIX_SAFE_BOTTOM;
            break;
        case NG_ALIGN_TOP:
        default:
            y = NG_FIX_SAFE_TOP;
            break;
    }
    y += layout.offset_y;
    if (y < 0)
        y = 0;
    if (y >= NG_FIX_HEIGHT)
        y = NG_FIX_HEIGHT - 1;
    return (u8)y;
}

void NGTextPrint(NGFixLayout layout, u8 palette, const char *str) {
    u8 text_len = str_len(str);
    u8 x = calc_x(layout, text_len);
    u8 y = calc_y(layout);

    hw_fix_text(x, y, str, palette, font_base);
}

static u8 int_to_str(s32 value, char *buf, u8 base, u8 is_signed) {
    char tmp[12];
    u8 i = 0;
    u8 neg = 0;
    u32 uval;

    if (is_signed && value < 0) {
        neg = 1;
        uval = (u32)(-value);
    } else {
        uval = (u32)value;
    }

    if (uval == 0) {
        tmp[i++] = '0';
    } else {
        while (uval > 0) {
            u8 digit = (u8)(uval % base);
            tmp[i++] = (char)((digit < 10) ? ('0' + digit) : ('A' + digit - 10));
            uval /= base;
        }
    }

    u8 len = 0;
    if (neg)
        buf[len++] = '-';
    while (i > 0)
        buf[len++] = tmp[--i];
    buf[len] = '\0';

    return len;
}

static char *emit_padded_str(char *out, char *end, const char *str, u8 len, char pad_char,
                             u8 width) {
    while (len < width && out < end) {
        *out++ = pad_char;
        width--;
    }
    while (*str && out < end)
        *out++ = *str++;
    return out;
}

void NGTextPrintf(NGFixLayout layout, u8 palette, const char *fmt, ...) {
    char buf[128];
    char *out = buf;
    char *end = buf + sizeof(buf) - 1;

    va_list args;
    va_start(args, fmt);

    while (*fmt && out < end) {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }

        fmt++;

        char pad_char = ' ';
        u8 width = 0;

        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = (u8)(width * 10 + (*fmt - '0'));
            fmt++;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                char tmp[12];
                u8 len = int_to_str(va_arg(args, s32), tmp, 10, 1);
                out = emit_padded_str(out, end, tmp, len, pad_char, width);
                break;
            }
            case 'u': {
                char tmp[12];
                u8 len = int_to_str(va_arg(args, u32), tmp, 10, 0);
                out = emit_padded_str(out, end, tmp, len, pad_char, width);
                break;
            }
            case 'x':
            case 'X': {
                char tmp[12];
                u8 len = int_to_str(va_arg(args, u32), tmp, 16, 0);
                out = emit_padded_str(out, end, tmp, len, pad_char, width);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (s)
                    while (*s && out < end)
                        *out++ = *s++;
                break;
            }
            case 'c':
                *out++ = (char)va_arg(args, int);
                break;
            case '%':
                *out++ = '%';
                break;
            default:
                break;
        }
        fmt++;
    }

    va_end(args);
    *out = '\0';

    NGTextPrint(layout, palette, buf);
}
