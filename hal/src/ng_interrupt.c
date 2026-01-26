/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_interrupt.c
 * @brief NeoGeo interrupt handling implementation
 */

#include <ng_interrupt.h>
#include <ng_hardware.h>

/* Custom interrupt handlers (called from crt0.s) */
NGInterruptHandler ng_vblank_handler = 0;
NGInterruptHandler ng_timer_handler = 0;

/* Timer state */
static u8 timer_enabled = 0;

/* ============================================================================
 * VBlank Interrupt
 * ========================================================================== */

void NGInterruptSetVBlankHandler(NGInterruptHandler handler) {
    ng_vblank_handler = handler;
}

NGInterruptHandler NGInterruptGetVBlankHandler(void) {
    return ng_vblank_handler;
}

/* ============================================================================
 * Timer Interrupt
 * ========================================================================== */

void NGInterruptSetTimerHandler(NGInterruptHandler handler) {
    ng_timer_handler = handler;
}

NGInterruptHandler NGInterruptGetTimerHandler(void) {
    return ng_timer_handler;
}

/* ============================================================================
 * Timer Configuration
 * ========================================================================== */

void NGTimerSetReload(u32 value) {
    /*
     * Timer is a 32-bit down-counter at 6MHz (pixel clock).
     * REG_TIMERHIGH (0x3C0008) = upper 16 bits
     * REG_TIMERLOW (0x3C000A) = lower 16 bits
     *
     * Writing to TIMERLOW triggers reload.
     * Value must be > 4 or CPU gets flooded with interrupts.
     */
    volatile u16 *timer_high = (volatile u16 *)0x3C0008;
    volatile u16 *timer_low = (volatile u16 *)0x3C000A;

    if (value < 5)
        value = 5; /* Prevent interrupt flood */

    *timer_high = (u16)(value >> 16);
    *timer_low = (u16)(value & 0xFFFF);
}

void NGTimerEnable(void) {
    /*
     * LSPCMODE (0x3C0006) timer bits (from timer_interrupt wiki):
     * - Bit 4 (0x10): Enable timer interrupt
     * - Bit 5 (0x20): Reload on TIMERLOW write
     * - Bit 6 (0x40): Reload at vblank
     * - Bit 7 (0x80): Reload when counter reaches 0
     */
    volatile u16 *irqack = (volatile u16 *)0x3C000C;

    /* Clear any pending timer interrupt first */
    *irqack = 0x0002;

    /* Enable: interrupt (4), reload on write (5), reload at vblank (6) */
    NG_REG_LSPCMODE |= 0x0070;
    timer_enabled = 1;
}

void NGTimerDisable(void) {
    /* Clear timer bits 4-7 */
    NG_REG_LSPCMODE &= ~0x00F0;
    timer_enabled = 0;
}

u8 NGTimerIsEnabled(void) {
    return timer_enabled;
}
