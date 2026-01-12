/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <fix.h>
#include <neogeo.h>
#include <stdarg.h>

static u16 font_base = 0;

void NGFixPut(u8 x, u8 y, u16 tile, u8 palette) {
    if (x >= NG_FIX_WIDTH || y >= NG_FIX_HEIGHT)
        return;

    /* Use optimized indexed addressing for VRAM access.
     * "move.w X,d(An)" is faster than "move.w X,xxx.L" */
    NG_VRAM_DECLARE_BASE();

    /* Fix layer is column-major: address = base + (x * 32) + y */
    NG_VRAM_SET_ADDR_FAST(NG_FIX_VRAM + (x << 5) + y);
    NG_VRAM_WRITE_FAST(((u16)palette << 12) | (tile & 0x0FFF));
}

void NGFixClear(u8 x, u8 y, u8 w, u8 h) {
    NG_VRAM_DECLARE_BASE();

    for (u8 row = 0; row < h && (y + row) < NG_FIX_HEIGHT; row++) {
        NG_VRAM_SETUP_FAST(NG_FIX_VRAM + (x << 5) + (y + row), 32);
        /* Use optimized DBF loop for clearing columns */
        if (w > 0) {
            u8 count = w;
            if ((x + w) > NG_FIX_WIDTH)
                count = NG_FIX_WIDTH - x;
            NG_VRAM_CLEAR_FAST(count);
        }
    }
    NG_VRAM_SET_MOD_FAST(1);
}

void NGFixClearAll(void) {
    /* Optimized full-screen clear using DBF loop.
     * Total tiles: 40 columns * 32 rows = 1280 words.
     * DBF loop is significantly faster than C for-loop. */
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_FIX_VRAM, 1);
    NG_VRAM_CLEAR_FAST(NG_FIX_WIDTH * NG_FIX_HEIGHT);
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

    u16 pal = (u16)palette << 12;

    /* Use optimized indexed addressing for faster VRAM writes.
     * Fix layer is column-major, VRAMMOD = 32 advances one column per write. */
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_FIX_VRAM + (x << 5) + y, 32);

    while (*str && x < NG_FIX_WIDTH) {
        u8 c = (u8)*str++;
        u16 tile = font_base + c;
        NG_VRAM_WRITE_FAST(pal | tile);
        x++;
    }

    NG_VRAM_SET_MOD_FAST(1);
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
