/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file neogeo.h
 * @brief Main ProGearSDK header.
 *
 * Include this header to access core SDK functionality including
 * system functions and memory map definitions.
 *
 * @section memmap Memory Map
 * - P-ROM: 0x000000 - 0x0FFFFF (1MB, banked beyond)
 * - Work RAM: 0x100000 - 0x10FFFF (64KB)
 * - BIOS RAM: 0x10F000 - 0x10FFFF (reserved)
 */

#ifndef _NEOGEO_H_
#define _NEOGEO_H_

#include <types.h>
#include <color.h>
#include <palette.h>

/** @defgroup biosvar BIOS Variables
 *  @brief BIOS work RAM variables.
 *  @{
 */

#define NG_BIOS_SYSTEM_MODE (*(vu8 *)0x10FD80) /**< System mode */
#define NG_BIOS_MVS_FLAG    (*(vu8 *)0x10FD82) /**< 0=AES, 1=MVS */
#define NG_BIOS_COUNTRY     (*(vu8 *)0x10FD83) /**< 0=Japan, 1=USA, 2=Europe */
#define NG_BIOS_VBLANK_FLAG (*(vu8 *)0x10FD8E) /**< Set by VBlank handler */

/** @} */

/** @defgroup sysfunc System Functions
 *  @brief Core system operations.
 *  @{
 */

/**
 * Wait for vertical blank period.
 * Blocks until the next VBlank interrupt occurs.
 */
void NGWaitVBlank(void);

/**
 * Kick the watchdog timer.
 * Must be called regularly to prevent system reset.
 */
void NGWatchdogKick(void);

/** @} */

/** @defgroup m68kopt 68000 Optimization Helpers
 *  @brief Inline assembly helpers for 68000-specific optimizations.
 *
 *  These helpers implement optimizations from the NeoGeo dev wiki:
 *  - MOVEQ #0,Dn is faster than CLR.L Dn (saves 2 cycles)
 *  - SUB.L An,An is faster than MOVE.L #0,An (saves 4 cycles)
 *  - ADD.W Dn,Dn is faster than LSL.W #1,Dn (saves 4 cycles)
 *  - LEA d(An),An is faster than ADDA.W #d,An for small constants
 *  @{
 */

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

/** @defgroup optdoc Optimization Guidelines
 *  @brief 68000 optimization tips for NeoGeo development.
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
 *
 *  @{
 */
/** @} */

/** @defgroup sysclean Clean API Aliases
 *  @brief Shorter names without NG_ prefix for cleaner code.
 *  @{
 */

/* System function aliases */
#define WaitVBlank   NGWaitVBlank
#define WatchdogKick NGWatchdogKick

/** @} */

#endif // _NEOGEO_H_
