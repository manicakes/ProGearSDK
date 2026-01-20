/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/system.h
 * @brief System services - VBlank, watchdog, initialization.
 * @internal This is an internal header - not for game developers.
 */

#ifndef HW_SYSTEM_H
#define HW_SYSTEM_H

#include <types.h>

/**
 * Initialize hardware systems.
 * Called once at startup before any other SDK functions.
 */
void hw_system_init(void);

/**
 * Wait for the next vertical blank period.
 * Blocks until the VBlank interrupt sets the BIOS flag.
 */
void hw_system_vblank(void);

/**
 * Kick the watchdog timer.
 * Must be called regularly to prevent system reset.
 * The engine calls this automatically each frame.
 */
void hw_system_watchdog(void);

/**
 * Check if running on MVS (arcade) hardware.
 * @return 1 if MVS, 0 if AES (home console)
 */
u8 hw_system_is_mvs(void);

/**
 * Get the system region.
 * @return 0 = Japan, 1 = USA, 2 = Europe
 */
u8 hw_system_get_region(void);

#endif /* HW_SYSTEM_H */
