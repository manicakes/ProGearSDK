/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_sprite.c
 * @brief Low-level NeoGeo sprite hardware abstraction.
 *
 * Implements VRAM/SCB write patterns used by actor, backdrop, terrain,
 * and UI modules. Uses optimized indexed addressing for performance.
 */

#include <ng_sprite.h>
#include <ng_hardware.h>

/* ============================================================
 * SCB1: Tile Column Writing
 * ============================================================ */

void NGSpriteTileBegin(u16 sprite_idx) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB1_BASE + (sprite_idx * 64), 1);
}

void NGSpriteTileWrite(u16 tile_idx, u8 palette, u8 h_flip, u8 v_flip) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_WRITE_FAST(tile_idx);
    u16 attr = ((u16)palette << 8);
    if (h_flip)
        attr |= 0x01;
    if (v_flip)
        attr |= 0x02;
    NG_VRAM_WRITE_FAST(attr);
}

void NGSpriteTileWriteRaw(u16 tile_idx, u16 attr) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_WRITE_FAST(tile_idx);
    NG_VRAM_WRITE_FAST(attr);
}

void NGSpriteTileWriteEmpty(void) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_WRITE_FAST(0);
    NG_VRAM_WRITE_FAST(0);
}

void NGSpriteTilePadTo32(u8 rows_written) {
    if (rows_written >= 32)
        return;
    NG_VRAM_DECLARE_BASE();
    u8 remaining = 32 - rows_written;
    NG_VRAM_CLEAR_FAST(remaining * 2);
}

/* ============================================================
 * SCB2: Shrink Values
 * ============================================================ */

void NGSpriteShrinkSet(u16 first_sprite, u8 count, u16 shrink) {
    if (count == 0)
        return;

    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB2_BASE + first_sprite, 1);

    u8 h_shrink_8 = (u8)(shrink >> 8);  /* Full 8-bit horizontal */
    u8 v_shrink = (u8)(shrink & 0xFF);  /* 8-bit vertical */

    /* Single sprite: no distribution needed */
    if (count == 1) {
        u16 scb2 = (u16)(((h_shrink_8 >> 4) << 8) | v_shrink);
        NG_VRAM_WRITE_FAST(scb2);
        return;
    }

    /* Distribute h_shrink across sprites using error accumulation.
     * This achieves smooth scaling by varying per-sprite h_shrink
     * values to approximate the full 8-bit precision, even though
     * hardware only supports 4-bit h_shrink per sprite.
     * See: https://wiki.neogeodev.org/index.php?title=Scaling_sprite_groups */
    u8 base_h = h_shrink_8 >> 4;   /* Integer part (0-15) */
    u8 frac = h_shrink_8 & 0x0F;   /* Fractional part (0-15) */
    u8 error = count >> 1;         /* Start centered for even distribution */

    for (u8 i = 0; i < count; i++) {
        u8 h = base_h;
        error += frac;
        if (error >= count) {
            error -= count;
            if (h < 15)
                h++;
        }
        u16 scb2 = (u16)((h << 8) | v_shrink);
        NG_VRAM_WRITE_FAST(scb2);
    }
}

/* ============================================================
 * SCB3: Y Position and Height
 * ============================================================ */

void NGSpriteYSet(u16 sprite_idx, s16 screen_y, u8 height) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB3_BASE + sprite_idx, 1);
    NG_VRAM_WRITE_FAST(NGSpriteSCB3(screen_y, height));
}

void NGSpriteYSetChain(u16 first_sprite, u8 count, s16 screen_y, u8 height) {
    if (count == 0)
        return;
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB3_BASE + first_sprite, 1);
    NG_VRAM_WRITE_FAST(NGSpriteSCB3(screen_y, height));
    if (count > 1) {
        NG_VRAM_FILL_FAST(NGSpriteSCB3Sticky(), count - 1);
    }
}

void NGSpriteYSetUniform(u16 first_sprite, u8 count, s16 screen_y, u8 height) {
    if (count == 0)
        return;
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB3_BASE + first_sprite, 1);
    NG_VRAM_FILL_FAST(NGSpriteSCB3(screen_y, height), count);
}

/* ============================================================
 * SCB4: X Positions
 * ============================================================ */

void NGSpriteXSet(u16 sprite_idx, s16 screen_x) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB4_BASE + sprite_idx, 1);
    NG_VRAM_WRITE_FAST(NGSpriteSCB4(screen_x));
}

void NGSpriteXSetSpaced(u16 first_sprite, u8 count, s16 base_x, s16 spacing) {
    if (count == 0)
        return;
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB4_BASE + first_sprite, 1);
    s16 x = base_x;
    for (u8 col = 0; col < count; col++) {
        NG_VRAM_WRITE_FAST(NGSpriteSCB4(x));
        x += spacing;
    }
}

void NGSpriteXBegin(u16 first_sprite) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_SETUP_FAST(NG_SCB4_BASE + first_sprite, 1);
}

void NGSpriteXWriteNext(s16 screen_x) {
    NG_VRAM_DECLARE_BASE();
    NG_VRAM_WRITE_FAST(NGSpriteSCB4(screen_x));
}

/* ============================================================
 * Combined High-Level Operations
 * ============================================================ */

void NGSpriteSetupStrip(u16 first_sprite, u8 num_cols, s16 screen_x, s16 screen_y, u8 height,
                        s16 tile_width, u16 shrink) {
    NGSpriteShrinkSet(first_sprite, num_cols, shrink);
    NGSpriteYSetChain(first_sprite, num_cols, screen_y, height);
    NGSpriteXSetSpaced(first_sprite, num_cols, screen_x, tile_width);
}

void NGSpriteSetupGrid(u16 first_sprite, u8 num_cols, s16 screen_x, s16 screen_y, u8 height,
                       s16 tile_width, u16 shrink) {
    NGSpriteShrinkSet(first_sprite, num_cols, shrink);
    NGSpriteYSetUniform(first_sprite, num_cols, screen_y, height);
    NGSpriteXSetSpaced(first_sprite, num_cols, screen_x, tile_width);
}
