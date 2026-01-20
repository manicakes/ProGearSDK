/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/system.h>
#include <hw/io.h>
#include <hw/lspc.h>

void hw_system_init(void) {
    /* Clear VBlank flag */
    BIOS_VBLANK_FLAG = 0;
}

void hw_system_vblank(void) {
    /* Wait for VBlank interrupt to set the flag */
    while (BIOS_VBLANK_FLAG == 0) {
        /* spin */
    }
    BIOS_VBLANK_FLAG = 0;
}

void hw_system_watchdog(void) {
    IO_WATCHDOG = 0;
}

u8 hw_system_is_mvs(void) {
    return BIOS_MVS_FLAG;
}

u8 hw_system_get_region(void) {
    return BIOS_COUNTRY;
}
