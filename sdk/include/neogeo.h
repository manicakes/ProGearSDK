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
 * hardware registers, system functions, and memory map definitions.
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

/** @defgroup hwregs Hardware Registers
 *  @brief Direct hardware register access.
 *  @{
 */

#define NG_REG_LSPCMODE (*(vu16 *)0x3C0006) /**< LSPC mode register */
#define NG_REG_IRQACK   (*(vu16 *)0x3C000C) /**< IRQ acknowledge */
#define NG_REG_WATCHDOG (*(vu8 *)0x300001)  /**< Watchdog kick */

#define NG_REG_VRAMADDR (*(vu16 *)0x3C0000) /**< VRAM address */
#define NG_REG_VRAMDATA (*(vu16 *)0x3C0002) /**< VRAM data read/write */
#define NG_REG_VRAMMOD  (*(vu16 *)0x3C0004) /**< VRAM address auto-increment */

/** @} */

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
static inline void NGWatchdogKick(void) {
    NG_REG_WATCHDOG = 0;
}

/** @} */

#endif // _NEOGEO_H_
