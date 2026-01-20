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

void hw_sprite_write_column(u16 sprite, u8 num_rows, const u16 *tiles, const u16 *attrs,
                            u8 clear_remaining) {
    vram_declare();
    vram_setup(VRAM_SCB1 + (sprite * SCB1_SPRITE_SIZE), 1);

    for (u8 row = 0; row < num_rows; row++) {
        vram_data(tiles ? tiles[row] : 0);
        vram_data(attrs ? attrs[row] : 0);
    }

    if (clear_remaining) {
        for (u8 row = num_rows; row < 32; row++) {
            vram_data(0);
            vram_data(0);
        }
    }
}

void hw_sprite_write_scb3_range(u16 first, u8 count, u16 scb3_value) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB3 + first, 1);
    vram_fill(scb3_value, count);
}

void hw_sprite_write_scb4_range(u16 first, u8 count, s16 start_x, s16 x_step) {
    if (count == 0)
        return;

    vram_declare();
    vram_setup(VRAM_SCB4 + first, 1);

    s16 x = start_x;
    for (u8 i = 0; i < count; i++) {
        vram_data(hw_sprite_pack_scb4(x));
        x = (s16)(x + x_step);
    }
}

/* Batched session state - stored in static to maintain across calls */
static vu16 *_batch_vram;

void hw_sprite_begin_scb1(u16 sprite) {
    _batch_vram = (vu16 *)0x3C0000;
    _batch_vram[0] = VRAM_SCB1 + (sprite * SCB1_SPRITE_SIZE);
    _batch_vram[2] = 1;
}

void hw_sprite_write_scb1_data(u16 tile, u16 attr) {
    _batch_vram[1] = tile;
    _batch_vram[1] = attr;
}

void hw_sprite_begin_scb2(u16 first) {
    _batch_vram = (vu16 *)0x3C0000;
    _batch_vram[0] = VRAM_SCB2 + first;
    _batch_vram[2] = 1;
}

void hw_sprite_write_scb2_data(u16 shrink) {
    _batch_vram[1] = shrink;
}

void hw_sprite_begin_scb3(u16 first) {
    _batch_vram = (vu16 *)0x3C0000;
    _batch_vram[0] = VRAM_SCB3 + first;
    _batch_vram[2] = 1;
}

void hw_sprite_write_scb3_data(u16 scb3) {
    _batch_vram[1] = scb3;
}

void hw_sprite_begin_scb4(u16 first) {
    _batch_vram = (vu16 *)0x3C0000;
    _batch_vram[0] = (u16)(VRAM_SCB4 + first);
    _batch_vram[2] = 1;
}

void hw_sprite_write_scb4_data(u16 scb4) {
    _batch_vram[1] = scb4;
}
