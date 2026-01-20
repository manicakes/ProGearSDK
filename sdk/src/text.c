/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <text.h>
#include <hw/fix.h>
#include <stdarg.h>

static u16 font_base = 0;

void TextSetFont(u16 base_tile) {
    font_base = base_tile;
}

void TextPrint(u8 x, u8 y, u8 palette, const char *str) {
    hw_fix_text(x, y, str, palette, font_base);
}

/*
 * Minimal printf implementation for embedded use.
 * Supports: %d, %i, %u, %x, %X, %s, %c, %%
 * Width specifiers with optional '0' padding.
 */

static u8 str_len(const char *str) {
    u8 len = 0;
    while (*str++)
        len++;
    return len;
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

void TextPrintf(u8 x, u8 y, u8 palette, const char *fmt, ...) {
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
                u8 len = int_to_str((s32)va_arg(args, u32), tmp, 10, 0);
                out = emit_padded_str(out, end, tmp, len, pad_char, width);
                break;
            }
            case 'x':
            case 'X': {
                char tmp[12];
                u8 len = int_to_str((s32)va_arg(args, u32), tmp, 16, 0);
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

    TextPrint(x, y, palette, buf);
}

/* Safe area bounds for alignment */
#define SAFE_LEFT   1
#define SAFE_RIGHT  38
#define SAFE_TOP    2
#define SAFE_BOTTOM 29

static u8 calc_x(HAlign h, u8 text_len) {
    s16 x;
    switch (h) {
        case ALIGN_CENTER:
            x = (s16)((SAFE_LEFT + SAFE_RIGHT + 1 - text_len) / 2);
            break;
        case ALIGN_RIGHT:
            x = SAFE_RIGHT + 1 - text_len;
            break;
        case ALIGN_LEFT:
        default:
            x = SAFE_LEFT;
            break;
    }
    if (x < 0)
        x = 0;
    if (x >= FIX_WIDTH)
        x = FIX_WIDTH - 1;
    return (u8)x;
}

static u8 calc_y(VAlign v) {
    s16 y;
    switch (v) {
        case ALIGN_MIDDLE:
            y = (SAFE_TOP + SAFE_BOTTOM) / 2;
            break;
        case ALIGN_BOTTOM:
            y = SAFE_BOTTOM;
            break;
        case ALIGN_TOP:
        default:
            y = SAFE_TOP;
            break;
    }
    if (y < 0)
        y = 0;
    if (y >= FIX_HEIGHT)
        y = FIX_HEIGHT - 1;
    return (u8)y;
}

void TextPrintAligned(HAlign h, VAlign v, u8 palette, const char *str) {
    u8 text_len = str_len(str);
    u8 x = calc_x(h, text_len);
    u8 y = calc_y(v);
    TextPrint(x, y, palette, str);
}

void TextPrintOffset(HAlign h, VAlign v, s8 offset_x, s8 offset_y, u8 palette, const char *str) {
    u8 text_len = str_len(str);
    s16 x = (s16)(calc_x(h, text_len) + offset_x);
    s16 y = (s16)(calc_y(v) + offset_y);

    if (x < 0)
        x = 0;
    if (x >= FIX_WIDTH)
        x = FIX_WIDTH - 1;
    if (y < 0)
        y = 0;
    if (y >= FIX_HEIGHT)
        y = FIX_HEIGHT - 1;

    TextPrint((u8)x, (u8)y, palette, str);
}

void TextClear(u8 x, u8 y, u8 w, u8 h) {
    hw_fix_clear(x, y, w, h);
}

void TextClearAll(void) {
    hw_fix_clear_all();
}
