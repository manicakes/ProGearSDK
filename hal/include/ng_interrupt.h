/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_interrupt.h
 * @brief NeoGeo interrupt handling - VBlank and Timer interrupts
 *
 * The NeoGeo has two main interrupts:
 * - Level 1 (VBlank): Fires once per frame at the start of vertical blank
 * - Level 2 (Timer): Fires when the LSPC timer reaches zero
 *
 * The timer interrupt is essential for raster effects (mid-frame changes).
 *
 * Usage for raster effects:
 * @code
 * void my_raster_handler(void) {
 *     // Change palette or scroll registers at specific scanline
 *     NGPalSet(0, 0, new_color);
 *     // Set timer for next scanline interrupt
 *     NGTimerSetReload(0x100);
 * }
 *
 * void init(void) {
 *     NGInterruptSetTimerHandler(my_raster_handler);
 *     NGTimerSetReload(0x1000); // Fire at specific scanline
 *     NGTimerEnable();
 * }
 * @endcode
 */

#ifndef NG_INTERRUPT_H
#define NG_INTERRUPT_H

#include <ng_types.h>

/**
 * @defgroup interrupt Interrupt Handling
 * @ingroup hal
 * @brief VBlank and Timer interrupt management.
 * @{
 */

/**
 * Interrupt handler function type
 */
typedef void (*NGInterruptHandler)(void);

/* ============================================================================
 * VBlank Interrupt (Level 1)
 * ========================================================================== */

/**
 * Set custom VBlank handler
 * Called once per frame at the start of vertical blank.
 * The default handler sets the BIOS vblank flag and kicks the watchdog.
 *
 * @param handler Function to call on VBlank, or NULL to restore default
 */
void NGInterruptSetVBlankHandler(NGInterruptHandler handler);

/**
 * Get current VBlank handler
 *
 * @return Current handler, or NULL if using default
 */
NGInterruptHandler NGInterruptGetVBlankHandler(void);

/* ============================================================================
 * Timer Interrupt (Level 2)
 * ========================================================================== */

/**
 * Set custom Timer handler
 * Called when the LSPC timer reaches zero.
 * Essential for raster effects (palette/scroll changes mid-frame).
 *
 * @param handler Function to call on Timer interrupt, or NULL to disable
 */
void NGInterruptSetTimerHandler(NGInterruptHandler handler);

/**
 * Get current Timer handler
 *
 * @return Current handler, or NULL if none set
 */
NGInterruptHandler NGInterruptGetTimerHandler(void);

/* ============================================================================
 * Timer Configuration
 * ========================================================================== */

/**
 * Timer reload register address
 * The timer counts down and fires when it reaches zero.
 * Value represents "ticks" where each tick is ~3.13 microseconds.
 */
#define NG_REG_TIMER_HIGH (*(vu16 *)0x3C0008) /**< Timer reload high 16 bits */
#define NG_REG_TIMER_LOW  (*(vu16 *)0x3C000A) /**< Timer reload low 16 bits */

/**
 * Set timer reload value (32-bit)
 * The timer counts down at 6MHz (pixel clock) and fires at zero.
 *
 * Approximate values for scanline timing (NTSC):
 * - ~2286 ticks per scanline (6MHz / 60fps / 264 lines ≈ 379, but
 *   actually 6MHz * 63.5µs per line ≈ 381 pixels * 6 = 2286)
 * - To fire after N scanlines: reload = N * 2286
 *
 * Value must be > 4 or CPU gets flooded with interrupts.
 *
 * @param value Timer reload value (32-bit, 0x00000005 to 0xFFFFFFFF)
 */
void NGTimerSetReload(u32 value);

/**
 * Enable the timer interrupt
 * The timer will fire when it counts down to zero.
 */
void NGTimerEnable(void);

/**
 * Disable the timer interrupt
 */
void NGTimerDisable(void);

/**
 * Check if timer is enabled
 *
 * @return 1 if enabled, 0 if disabled
 */
u8 NGTimerIsEnabled(void);

/**
 * Convert scanline count to timer reload value
 * Timer runs at 6MHz pixel clock.
 * Each scanline is 384 pixels = 384 ticks.
 *
 * @param scanlines Number of scanlines to wait
 * @return Timer reload value (32-bit)
 */
static inline u32 NGTimerScanlineToReload(u16 scanlines) {
    /* 384 pixels per scanline at 6MHz pixel clock */
    return (u32)scanlines * 384;
}

/**
 * Convenience: Set timer to fire after N scanlines
 *
 * @param scanlines Number of scanlines to wait before interrupt
 */
static inline void NGTimerSetScanline(u16 scanlines) {
    NGTimerSetReload(NGTimerScanlineToReload(scanlines));
}

/** @} */ /* end of interrupt group */

#endif /* NG_INTERRUPT_H */
