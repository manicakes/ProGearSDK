/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/fix.h>
#include <hw/lspc.h>

/*
 * Fix layer VRAM is organized in column-major order:
 *   Address = 0x7000 + (x * 32) + y
 *
 * Each tile is one word:
 *   [15-12] Palette | [11-0] Tile index
 */

void hw_fix_put(u8 x, u8 y, u16 tile, u8 palette) {
    if (x >= FIX_WIDTH || y >= FIX_HEIGHT)
        return;

    vram_declare();

    /* Column-major: address = base + (x * 32) + y */
    vram_addr(VRAM_FIX + ((u16)x << 5) + y);
    vram_data(((u16)palette << 12) | (tile & 0x0FFF));
}

void hw_fix_clear(u8 x, u8 y, u8 w, u8 h) {
    if (w == 0 || h == 0)
        return;

    vram_declare();

    /* Clear row by row, using VRAMMOD=32 to step through columns */
    for (u8 row = 0; row < h && (y + row) < FIX_HEIGHT; row++) {
        vram_setup(VRAM_FIX + ((u16)x << 5) + (y + row), 32);

        u8 count = w;
        if ((x + w) > FIX_WIDTH)
            count = FIX_WIDTH - x;

        vram_clear(count);
    }

    /* Restore default modifier */
    vram_mod(1);
}

void hw_fix_clear_all(void) {
    vram_declare();

    /* 40 columns × 32 rows = 1280 tiles */
    vram_setup(VRAM_FIX, 1);
    vram_clear(FIX_WIDTH * FIX_HEIGHT);
}

void hw_fix_row(u8 x, u8 y, const u16 *tiles, u8 count, u8 palette) {
    if (x >= FIX_WIDTH || y >= FIX_HEIGHT || count == 0)
        return;

    vram_declare();

    /* VRAMMOD=32 advances one column per write (column-major layout) */
    vram_setup(VRAM_FIX + ((u16)x << 5) + y, 32);

    u16 pal = (u16)palette << 12;
    for (u8 i = 0; i < count && (x + i) < FIX_WIDTH; i++) {
        vram_data(pal | (tiles[i] & 0x0FFF));
    }

    vram_mod(1);
}

void hw_fix_text(u8 x, u8 y, const char *str, u8 palette, u16 font_base) {
    if (x >= FIX_WIDTH || y >= FIX_HEIGHT || !str)
        return;

    vram_declare();

    /* VRAMMOD=32 advances one column per write */
    vram_setup(VRAM_FIX + ((u16)x << 5) + y, 32);

    u16 pal = (u16)palette << 12;
    while (*str && x < FIX_WIDTH) {
        u16 tile = font_base + (u8)*str;
        vram_data(pal | (tile & 0x0FFF));
        str++;
        x++;
    }

    vram_mod(1);
}

u8 hw_fix_text_len(u8 x, u8 y, const char *str, u8 palette, u16 font_base) {
    if (x >= FIX_WIDTH || y >= FIX_HEIGHT || !str)
        return 0;

    vram_declare();
    vram_setup(VRAM_FIX + ((u16)x << 5) + y, 32);

    u16 pal = (u16)palette << 12;
    u8 count = 0;

    while (*str && x < FIX_WIDTH) {
        u16 tile = font_base + (u8)*str;
        vram_data(pal | (tile & 0x0FFF));
        str++;
        x++;
        count++;
    }

    vram_mod(1);
    return count;
}
