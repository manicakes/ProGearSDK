/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/lspc.h
 * @brief LSPC (Line SPrite Controller) hardware access.
 * @internal This is an internal header - not for game developers.
 *
 * The LSPC controls all sprite and fix layer graphics on the NeoGeo.
 * This header provides register definitions and optimized VRAM access macros.
 */

#ifndef HW_LSPC_H
#define HW_LSPC_H

#include <types.h>

/*
 * ============================================================================
 *  LSPC REGISTERS
 * ============================================================================
 */

#define LSPC_ADDR (*(vu16 *)0x3C0000) /* VRAM address register */
#define LSPC_DATA (*(vu16 *)0x3C0002) /* VRAM data register */
#define LSPC_MOD  (*(vu16 *)0x3C0004) /* VRAM address auto-increment */
#define LSPC_MODE (*(vu16 *)0x3C0006) /* LSPC mode register */

/*
 * ============================================================================
 *  VRAM MEMORY MAP
 * ============================================================================
 *
 * The NeoGeo VRAM is organized into regions for different purposes:
 *
 *  0x0000 - 0x7FFF : SCB1 - Sprite tile indices and attributes
 *  0x7000 - 0x74FF : Fix layer tiles (overlaps SCB1, separate access)
 *  0x8000 - 0x81FF : SCB2 - Sprite shrink values
 *  0x8200 - 0x83FF : SCB3 - Sprite Y position and height
 *  0x8400 - 0x85FF : SCB4 - Sprite X position
 */

#define VRAM_SCB1 0x0000 /* Sprite tiles + attributes (64 words/sprite) */
#define VRAM_SCB2 0x8000 /* Shrink values (1 word/sprite) */
#define VRAM_SCB3 0x8200 /* Y position + height (1 word/sprite) */
#define VRAM_SCB4 0x8400 /* X position (1 word/sprite) */
#define VRAM_FIX  0x7000 /* Fix layer tiles */

/* SCB1 layout: 64 words per sprite (32 tile rows × 2 words each) */
#define SCB1_SPRITE_SIZE    64
#define SCB1_WORDS_PER_TILE 2

/* Sprite limits */
#define SPRITE_MAX      381
#define SPRITE_PER_LINE 96

/* Fix layer dimensions */
#define FIX_WIDTH  40
#define FIX_HEIGHT 32

/*
 * ============================================================================
 *  SCB REGISTER FORMATS
 * ============================================================================
 *
 * SCB1 (per tile, 2 words):
 *   Word 0: Tile index (bits 0-15, lower 16 bits)
 *   Word 1: [15-8] Palette | [3] Tile bank | [1] V-flip | [0] H-flip
 *
 * SCB2 (per sprite):
 *   [15-12] Unused | [11-8] H-shrink | [7-0] V-shrink
 *   Full size = 0x0FFF (H=15, V=255)
 *
 * SCB3 (per sprite):
 *   [15-7] Y position | [6] Sticky bit | [5-0] Height in tiles
 *
 * SCB4 (per sprite):
 *   [15-7] X position | [6-0] Unused
 */

#define SCB1_PALETTE_SHIFT 8
#define SCB1_VFLIP_BIT     0x02
#define SCB1_HFLIP_BIT     0x01

#define SCB2_FULL_SIZE 0x0FFF

#define SCB3_STICKY_BIT  0x40
#define SCB3_Y_SHIFT     7
#define SCB3_HEIGHT_MASK 0x3F

#define SCB4_X_SHIFT 7

/*
 * ============================================================================
 *  OPTIMIZED VRAM ACCESS
 * ============================================================================
 *
 * The 68000 indexed addressing mode "move.w X,d(An)" is faster than
 * absolute long addressing "move.w X,xxx.L". By binding a register
 * to the VRAM base address, we get faster consecutive VRAM operations.
 *
 * The a5 register is used because it's typically caller-saved and
 * available for scratch use within functions.
 */

#define VRAM_BASE 0x3C0000

#ifdef __CPPCHECK__
/* Fallback for static analysis tools */
#define vram_declare() vu16 *_vram = (vu16 *)VRAM_BASE
#else
/* Optimized: bind _vram to register a5 */
#define vram_declare() register vu16 *_vram __asm__("a5") = (vu16 *)VRAM_BASE
#endif

#define vram_addr(a) (_vram[0] = (u16)(a))
#define vram_data(d) (_vram[1] = (u16)(d))
#define vram_read()  (_vram[1])
#define vram_mod(m)  (_vram[2] = (u16)(m))

/* Combined setup: set address and modifier together */
#define vram_setup(addr, mod) \
    do {                      \
        vram_addr(addr);      \
        vram_mod(mod);        \
    } while (0)

/*
 * ============================================================================
 *  BULK VRAM OPERATIONS (DBF loops)
 * ============================================================================
 *
 * The 68000 DBF (Decrement and Branch if False) instruction is perfect
 * for tight loops. It decrements a register and branches until it reaches -1.
 * These macros use inline assembly for maximum performance.
 */

#ifdef __CPPCHECK__
/* Fallback for static analysis */
#define vram_clear(count)                    \
    do {                                     \
        for (u16 _i = 0; _i < (count); _i++) \
            _vram[1] = 0;                    \
    } while (0)

#define vram_fill(val, count)                \
    do {                                     \
        u16 _v = (val);                      \
        for (u16 _i = 0; _i < (count); _i++) \
            _vram[1] = _v;                   \
    } while (0)
#else
/* Optimized DBF loops */
#define vram_clear(count)                                   \
    do {                                                    \
        register u16 _n __asm__("d0") = (u16)((count) - 1); \
        __asm__ volatile("1:\n\t"                           \
                         "    clr.w 2(%[v])\n\t"            \
                         "    dbf %[n], 1b"                 \
                         : [n] "+d"(_n)                     \
                         : [v] "a"(_vram)                   \
                         : "memory");                       \
    } while (0)

#define vram_fill(val, count)                               \
    do {                                                    \
        register u16 _d __asm__("d1") = (u16)(val);         \
        register u16 _n __asm__("d0") = (u16)((count) - 1); \
        __asm__ volatile("1:\n\t"                           \
                         "    move.w %[d], 2(%[v])\n\t"     \
                         "    dbf %[n], 1b"                 \
                         : [n] "+d"(_n)                     \
                         : [v] "a"(_vram), [d] "d"(_d)      \
                         : "memory");                       \
    } while (0)
#endif

#endif /* HW_LSPC_H */
