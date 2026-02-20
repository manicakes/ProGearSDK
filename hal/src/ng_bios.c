/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_bios.c
 * @brief NeoGeo BIOS call implementations
 */

#include <ng_bios.h>
#include <ng_hardware.h>

/* BIOS RAM addresses */
#define BIOS_CREDITS  (*(vu8 *)0x10FE00)            /* Credit count */
#define BIOS_TITLE    ((const char *)0x10F800)      /* Game title */
#define BIOS_VERSION  (*(vu8 *)0x10FDFA)            /* BIOS version */
#define BIOS_SOFT_DIP ((const NGSoftDip *)0x10FD84) /* Soft DIP settings */

/* BIOS entry points (as function pointers) */
typedef void (*BiosVoidFunc)(void);
typedef void (*BiosFixPrintFunc)(u8 x, u8 y, const char *str);

#define BIOS_SYSTEM_RETURN ((BiosVoidFunc)0xC00444)
#define BIOS_FIX_CLEAR     ((BiosVoidFunc)0xC004C2)

/* ============================================================================
 * System Control
 * ========================================================================== */

void NGBiosSystemReturn(void) {
    /* Clear game control bit before returning to BIOS */
    NG_BIOS_SYSTEM_MODE &= ~0x80;

    /* Jump to BIOS system return - does not return */
    __asm__ volatile("jmp 0xC00444.l" : : : "memory");
    __builtin_unreachable();
}

void NGBiosSoftReset(void) {
    /* Jump to reset vector */
    __asm__ volatile("move.l 4.w, %%a0\n\t"
                     "jmp (%%a0)"
                     :
                     :
                     : "a0", "memory");
}

void NGBiosEyecatcher(void) {
    /* Send eyecatcher command to Z80 */
    NG_REG_SOUND = 0x02;
}

/* ============================================================================
 * Credit Management
 * ========================================================================== */

u8 NGBiosGetCredits(void) {
    return BIOS_CREDITS;
}

void NGBiosAddCredits(u8 count) {
    u8 credits = BIOS_CREDITS;
    u16 total = (u16)credits + count;
    if (total > 99)
        total = 99;
    BIOS_CREDITS = (u8)total;
}

u8 NGBiosUseCredit(void) {
    if (BIOS_CREDITS == 0)
        return 0;
    BIOS_CREDITS--;
    return 1;
}

/* ============================================================================
 * BIOS Variables
 * ========================================================================== */

const char *NGBiosGetTitle(void) {
    return BIOS_TITLE;
}

u8 NGBiosGetVersion(void) {
    return BIOS_VERSION;
}

u8 NGBiosIsDev(void) {
    /* Dev BIOS has specific version patterns */
    u8 ver = BIOS_VERSION;
    return (ver == 0x00 || ver >= 0xF0) ? 1 : 0;
}

/* ============================================================================
 * Fix Layer BIOS Calls
 * ========================================================================== */

void NGBiosFixClear(void) {
    /* Call BIOS fix clear routine */
    __asm__ volatile("jsr 0xC004C2.l" : : : "d0", "d1", "a0", "a1", "memory");
}

void NGBiosFixPrint(u8 x, u8 y, const char *str) {
    (void)x;
    (void)y;
    (void)str;
    /*
     * BIOS_MESS_OUT expects:
     * - D0.W: packed position (x in high byte, y in low byte)
     * - A0: pointer to string
     *
     * This is a simplified implementation. The actual BIOS
     * routine has specific string format requirements.
     */
    /* For now, use HAL fix layer functions instead of BIOS */
}

/* ============================================================================
 * Soft DIP
 * ========================================================================== */

const NGSoftDip *NGBiosGetSoftDip(void) {
    return BIOS_SOFT_DIP;
}
