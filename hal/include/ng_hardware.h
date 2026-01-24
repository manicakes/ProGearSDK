/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_hardware.h
 * @brief NeoGeo hardware register definitions and low-level access.
 *
 * Include this header to access hardware registers, VRAM macros,
 * BIOS variables, and 68000 optimization helpers.
 *
 * @section memmap Memory Map
 * - P-ROM: 0x000000 - 0x0FFFFF (1MB, banked beyond)
 * - Work RAM: 0x100000 - 0x10FFFF (64KB)
 * - BIOS RAM: 0x10F000 - 0x10FFFF (reserved)
 */

#ifndef _NG_HARDWARE_H_
#define _NG_HARDWARE_H_

#include <ng_types.h>
#include <ng_color.h>
#include <ng_palette.h>

/**
 * @defgroup hardware Hardware Access
 * @ingroup hal
 * @brief NeoGeo hardware registers, VRAM, and 68000 optimizations.
 * @{
 */

/** @name Screen Dimensions */
/** @{ */

#define SCREEN_WIDTH  320 /**< Screen width in pixels */
#define SCREEN_HEIGHT 224 /**< Screen height in pixels */
/** @} */

/** @name Hardware Registers */
/** @{ */

/* Video / LSPC registers */
#define NG_REG_LSPCMODE (*(vu16 *)0x3C0006) /**< LSPC mode register */
#define NG_REG_IRQACK   (*(vu16 *)0x3C000C) /**< IRQ acknowledge */

/* System registers */
#define NG_REG_WATCHDOG (*(vu8 *)0x300001) /**< Watchdog kick */

/* Input registers (directly mapped, active-low) */
#define NG_REG_P1CNT    (*(vu8 *)0x300000) /**< Player 1 controller: UDLRABCD */
#define NG_REG_P2CNT    (*(vu8 *)0x340000) /**< Player 2 controller: UDLRABCD */
#define NG_REG_STATUS_A (*(vu8 *)0x320001) /**< Status A: Coin1, Coin2, Service */
#define NG_REG_STATUS_B (*(vu8 *)0x380000) /**< Status B: Start/Select P1 & P2 */

/* Audio registers (68k/Z80 communication) */
#define NG_REG_SOUND       (*(vu8 *)0x320000) /**< Sound cmd (W) / reply (R) */
#define NG_REG_SOUND_REPLY (*(vu8 *)0x320001) /**< Z80 reply (read only) */

/* Palette RAM */
#define NG_REG_BACKDROP (*(vu16 *)0x401FFE) /**< Backdrop color */
/** @} */

/** @name VRAM Optimization
 *
 *  These macros implement the optimization from the NeoGeo dev wiki:
 *  Using indexed addressing with a base register is faster than
 *  absolute long addressing for consecutive VRAM operations.
 *
 *  "move.w X,d(An)" loads faster than "move.w X,xxx.L"
 *
 *  Usage:
 *  @code
 *  NG_VRAM_DECLARE_BASE();              // Declare and load base register
 *  NG_VRAM_SET_MOD_FAST(1);             // Set auto-increment
 *  NG_VRAM_SET_ADDR_FAST(0x7000);       // Set address
 *  NG_VRAM_WRITE_FAST(tile_data);       // Write data (auto-increments)
 *  @endcode
 */
/** @{ */

/** Base address of VRAM registers (VRAMADDR at +0, VRAMDATA at +2, VRAMMOD at +4) */
#define NG_VRAM_BASE 0x3C0000

/**
 * Declare and initialize the VRAM base register.
 * Call once at the start of a function that does multiple VRAM operations.
 * Uses register a5 which is typically caller-saved.
 *
 * Note: The __asm__ syntax is GCC-specific. A fallback is provided for
 * static analysis tools (cppcheck) that don't understand this syntax.
 */
#ifdef __CPPCHECK__
/* cppcheck-compatible fallback (no register binding) */
#define NG_VRAM_DECLARE_BASE() volatile u16 *_ng_vram_base = (volatile u16 *)NG_VRAM_BASE
#else
/* GCC optimized version: binds _ng_vram_base to register a5 */
#define NG_VRAM_DECLARE_BASE() \
    register volatile u16 *_ng_vram_base __asm__("a5") = (volatile u16 *)NG_VRAM_BASE
#endif

/**
 * Set VRAM address using indexed addressing (faster than absolute).
 * Requires NG_VRAM_DECLARE_BASE() to be called first.
 */
#define NG_VRAM_SET_ADDR_FAST(addr) (_ng_vram_base[0] = (u16)(addr))

/**
 * Write to VRAM data register using indexed addressing.
 * Address auto-increments by VRAMMOD after each write.
 */
#define NG_VRAM_WRITE_FAST(data) (_ng_vram_base[1] = (u16)(data))

/**
 * Read from VRAM data register using indexed addressing.
 */
#define NG_VRAM_READ_FAST() (_ng_vram_base[1])

/**
 * Set VRAM auto-increment modifier using indexed addressing.
 */
#define NG_VRAM_SET_MOD_FAST(mod) (_ng_vram_base[2] = (u16)(mod))

/**
 * Combined: set address and modifier in sequence (common pattern).
 */
#define NG_VRAM_SETUP_FAST(addr, mod)   \
    do {                                \
        _ng_vram_base[0] = (u16)(addr); \
        _ng_vram_base[2] = (u16)(mod);  \
    } while (0)

/**
 * Write N consecutive zero words to VRAM (optimized clear).
 * Uses DBF loop for minimal overhead.
 * @param count Number of words to write (1-65536)
 */
#ifdef __CPPCHECK__
/* cppcheck-compatible fallback */
#define NG_VRAM_CLEAR_FAST(count)              \
    do {                                       \
        for (u16 _i = 0; _i < (count); _i++) { \
            _ng_vram_base[1] = 0;              \
        }                                      \
    } while (0)
#else
#define NG_VRAM_CLEAR_FAST(count)                             \
    do {                                                      \
        register u16 _cnt __asm__("d0") = (u16)((count) - 1); \
        __asm__ volatile("1:\n\t"                             \
                         "    clr.w 2(%[base])\n\t"           \
                         "    dbf %[cnt], 1b\n\t"             \
                         : [cnt] "+d"(_cnt)                   \
                         : [base] "a"(_ng_vram_base)          \
                         : "memory");                         \
    } while (0)
#endif

/**
 * Write N consecutive copies of a value to VRAM (optimized fill).
 * Uses DBF loop for minimal overhead.
 * @param value Word value to write
 * @param count Number of words to write (1-65536)
 */
#ifdef __CPPCHECK__
/* cppcheck-compatible fallback */
#define NG_VRAM_FILL_FAST(value, count)        \
    do {                                       \
        u16 _val = (u16)(value);               \
        for (u16 _i = 0; _i < (count); _i++) { \
            _ng_vram_base[1] = _val;           \
        }                                      \
    } while (0)
#else
#define NG_VRAM_FILL_FAST(value, count)                               \
    do {                                                              \
        register u16 _val __asm__("d1") = (u16)(value);               \
        register u16 _cnt __asm__("d0") = (u16)((count) - 1);         \
        __asm__ volatile("1:\n\t"                                     \
                         "    move.w %[val], 2(%[base])\n\t"          \
                         "    dbf %[cnt], 1b\n\t"                     \
                         : [cnt] "+d"(_cnt)                           \
                         : [base] "a"(_ng_vram_base), [val] "d"(_val) \
                         : "memory");                                 \
    } while (0)
#endif
/** @} */

/** @name BIOS Variables */
/** @{ */

#define NG_BIOS_SYSTEM_MODE (*(vu8 *)0x10FD80) /**< System mode */
#define NG_BIOS_MVS_FLAG    (*(vu8 *)0x10FD82) /**< 0=AES, 1=MVS */
#define NG_BIOS_COUNTRY     (*(vu8 *)0x10FD83) /**< 0=Japan, 1=USA, 2=Europe */
#define NG_BIOS_VBLANK_FLAG (*(vu8 *)0x10FD8E) /**< Set by VBlank handler */
/** @} */

/** @name System Functions */
/** @{ */

/**
 * Wait for vertical blank period.
 * Blocks until the next VBlank interrupt occurs.
 */
void NGWaitVBlank(void);

/**
 * Kick the watchdog timer.
 * Must be called regularly to prevent system reset.
 */
static inline void NGWatchdogKick(void) {
    NG_REG_WATCHDOG = 0;
}
/** @} */

/** @name 68000 Optimization Helpers
 *
 *  These helpers implement optimizations from the NeoGeo dev wiki:
 *  - MOVEQ #0,Dn is faster than CLR.L Dn (saves 2 cycles)
 *  - SUB.L An,An is faster than MOVE.L #0,An (saves 4 cycles)
 *  - ADD.W Dn,Dn is faster than LSL.W #1,Dn (saves 4 cycles)
 *  - `LEA d(An),An` is faster than `ADDA.W #d,An` for small constants
 */
/** @{ */

/**
 * Clear a data register to zero using MOVEQ (faster than CLR).
 * Saves 2 cycles compared to clr.l.
 */
#ifdef __CPPCHECK__
#define NG_CLEAR_D(reg) ((reg) = 0)
#else
#define NG_CLEAR_D(reg) __asm__ volatile("moveq #0, %0" : "=d"(reg))
#endif

/**
 * Clear an address register to zero using SUB (faster than MOVE #0).
 * Saves 4 cycles compared to move.l #0,An.
 */
#ifdef __CPPCHECK__
#define NG_CLEAR_A(reg) ((reg) = 0)
#else
#define NG_CLEAR_A(reg) __asm__ volatile("sub.l %0, %0" : "=a"(reg))
#endif

/**
 * Multiply by 2 using ADD (faster than LSL #1).
 * Saves 4 cycles compared to lsl.w #1.
 */
#ifdef __CPPCHECK__
#define NG_MUL2_W(val) ((val) = (val) * 2)
#else
#define NG_MUL2_W(val) __asm__ volatile("add.w %0, %0" : "+d"(val))
#endif

/**
 * Multiply by 2 for 32-bit value using ADD (faster than LSL #1).
 * Saves 4 cycles compared to lsl.l #1.
 */
#ifdef __CPPCHECK__
#define NG_MUL2_L(val) ((val) = (val) * 2)
#else
#define NG_MUL2_L(val) __asm__ volatile("add.l %0, %0" : "+d"(val))
#endif

/**
 * Multiply by 4 using two ADDs (faster than LSL #2).
 * Saves 2 cycles compared to lsl.w #2.
 */
#ifdef __CPPCHECK__
#define NG_MUL4_W(val) ((val) = (val) * 4)
#else
#define NG_MUL4_W(val)                  \
    __asm__ volatile("add.w %0, %0\n\t" \
                     "add.w %0, %0"     \
                     : "+d"(val))
#endif

/**
 * Fast 16-bit unsigned multiply using hardware MULU.
 * The 68000 MULU takes 38-70 cycles but is faster than
 * software multiplication for larger values.
 * Result is 32-bit (low word in d0, high word preserved).
 */
#ifdef __CPPCHECK__
#define NG_MULU16(a, b, result) ((result) = (u32)(a) * (u32)(b))
#else
#define NG_MULU16(a, b, result) \
    __asm__ volatile("mulu.w %2, %0" : "=d"(result) : "0"((u16)(a)), "d"((u16)(b)))
#endif

/**
 * Fast 16-bit unsigned divide using hardware DIVU.
 * The 68000 DIVU takes 38-140 cycles.
 * @param dividend 32-bit dividend
 * @param divisor 16-bit divisor
 * @param quotient Result: 16-bit quotient
 * @param remainder Result: 16-bit remainder (in upper word)
 */
#ifdef __CPPCHECK__
#define NG_DIVU16(dividend, divisor, quotient, remainder) \
    do {                                                  \
        quotient = (u16)((dividend) / (divisor));         \
        remainder = (u16)((dividend) % (divisor));        \
    } while (0)
#else
#define NG_DIVU16(dividend, divisor, quotient, remainder)                                \
    do {                                                                                 \
        u32 _tmp = (u32)(dividend);                                                      \
        __asm__ volatile("divu.w %2, %0" : "+d"(_tmp) : "0"(_tmp), "d"((u16)(divisor))); \
        quotient = (u16)_tmp;                                                            \
        remainder = (u16)(_tmp >> 16);                                                   \
    } while (0)
#endif
/** @} */

/** @name Optimization Guidelines */
/** @{ */
/**
 *  ## VRAM Access
 *  - Use NG_VRAM_DECLARE_BASE() and NG_VRAM_*_FAST macros for multiple
 *    consecutive VRAM operations. Indexed addressing (d(An)) is faster
 *    than absolute long addressing (xxx.L).
 *  - Batch VRAM writes when possible. Set VRAMMOD once, then write
 *    multiple values without re-setting the address.
 *
 *  ## Register Operations
 *  - Use MOVEQ #0,Dn instead of CLR.L Dn (2 cycles faster)
 *  - Use SUB.L An,An instead of MOVE.L #0,An (4 cycles faster)
 *  - Use ADD Dn,Dn instead of LSL #1,Dn (4 cycles faster)
 *  - Post-increment (An)+ is faster than pre-decrement -(An) except for MOVE
 *
 *  ## Loops
 *  - Use DBF for countdown loops (decrement and branch if not -1)
 *  - Unroll tight loops when iteration count is small and known
 *  - Move invariant calculations outside loops
 *
 *  ## Comparisons
 *  - For small constants (-128 to +127), load into register first
 *  - Use SUBQ/ADDQ (-8 to +8) instead of CMP when value can be modified
 *
 *  ## Multiplication
 *  - 68000 MULU.W takes 38-70 cycles (faster for larger multiplicands)
 *  - For power-of-2 multiplies, use shifts or ADD chains
 *  - For multiply by 3: ADD + original (x*3 = x*2 + x)
 *
 *  ## Memory Access
 *  - Word/long access must be aligned to even addresses
 *  - First element access is faster than subsequent elements
 *  - Use MOVEM for saving/restoring multiple registers
 */
/** @} */

/** @} */ /* end of hardware group */

#endif // _NG_HARDWARE_H_
