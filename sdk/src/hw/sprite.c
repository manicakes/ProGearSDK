/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/sprite.h>
#include <hw/lspc.h>

void hw_sprite_write_tiles(u16 sprite, u8 height, const u16 *tiles, const u16 *attrs) {
    vram_declare();

    /* SCB1 address for this sprite */
    vram_setup(VRAM_SCB1 + (sprite * SCB1_SPRITE_SIZE), 1);

    /* Write tile/attr pairs */
    for (u8 row = 0; row < height; row++) {
        vram_data(tiles[row]);
        vram_data(attrs[row]);
    }

    /* Clear remaining rows to avoid garbage */
    for (u8 row = height; row < 32; row++) {
        vram_data(0);
        vram_data(0);
    }
}

void hw_sprite_write_tile(u16 sprite, u8 row, u16 tile, u16 attr) {
    vram_declare();

    /* Calculate address for specific tile */
    u16 addr = VRAM_SCB1 + (sprite * SCB1_SPRITE_SIZE) + (row * SCB1_WORDS_PER_TILE);
    vram_setup(addr, 1);
    vram_data(tile);
    vram_data(attr);
}

void hw_sprite_write_shrink(u16 first, u8 count, u16 shrink) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB2 + first, 1);
    vram_fill(shrink, count);
}

void hw_sprite_write_ychain(u16 first, u8 count, s16 y, u8 height) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB3 + first, 1);

    /* First sprite: position + height */
    u16 scb3 = hw_sprite_pack_scb3(y, height);
    vram_data(scb3);

    /* Remaining sprites: sticky bit only */
    for (u8 i = 1; i < count; i++) {
        vram_data(hw_sprite_scb3_sticky());
    }
}

void hw_sprite_write_xchain(u16 first, u8 count, s16 x, u8 zoom) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB4 + first, 1);

    for (u8 col = 0; col < count; col++) {
        /* Each column offset by 16 pixels scaled by zoom */
        s16 col_x = (s16)(x + ((col * 16 * zoom) >> 4));
        vram_data(hw_sprite_pack_scb4(col_x));
    }
}

void hw_sprite_hide(u16 first, u8 count) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB3 + first, 1);
    vram_clear(count);
}

void hw_sprite_hide_all(void) {
    vram_declare();
    vram_setup(VRAM_SCB3, 1);
    vram_clear(SPRITE_MAX);
}
