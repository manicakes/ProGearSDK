/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/io.h
 * @brief Input/Output port hardware access.
 * @internal This is an internal header - not for game developers.
 *
 * Hardware I/O ports for controllers, system buttons, and audio communication.
 */

#ifndef HW_IO_H
#define HW_IO_H

#include <types.h>

/*
 * ============================================================================
 *  PLAYER INPUT PORTS
 * ============================================================================
 *
 * P1/P2 joystick and buttons are directly readable.
 * Active-low: 0 = pressed, 1 = released
 *
 * Bit layout:
 *   [7] D  [6] C  [5] B  [4] A  [3] Right  [2] Left  [1] Down  [0] Up
 */

#define IO_P1 (*(vu8 *)0x300000) /* Player 1 joystick + ABCD */
#define IO_P2 (*(vu8 *)0x340000) /* Player 2 joystick + ABCD */

/*
 * ============================================================================
 *  SYSTEM STATUS PORTS
 * ============================================================================
 *
 * STATUS_A: Coin switches and service button (active-low)
 *   [2] Service  [1] Coin 2  [0] Coin 1
 *
 * STATUS_B: Start and Select buttons
 *   [3] P2 Select  [2] P2 Start  [1] P1 Select  [0] P1 Start
 */

#define IO_STATUS_A (*(vu8 *)0x320001)
#define IO_STATUS_B (*(vu8 *)0x380000)

/*
 * ============================================================================
 *  INPUT BIT MASKS
 * ============================================================================
 */

/* Joystick bits (active-low in hardware, inverted by input layer) */
#define IO_UP    0x01
#define IO_DOWN  0x02
#define IO_LEFT  0x04
#define IO_RIGHT 0x08
#define IO_A     0x10
#define IO_B     0x20
#define IO_C     0x40
#define IO_D     0x80

/* STATUS_B bits */
#define IO_P1_START  0x01
#define IO_P1_SELECT 0x02
#define IO_P2_START  0x04
#define IO_P2_SELECT 0x08

/* STATUS_A bits */
#define IO_COIN1   0x01
#define IO_COIN2   0x02
#define IO_SERVICE 0x04

/*
 * ============================================================================
 *  AUDIO COMMUNICATION
 * ============================================================================
 *
 * The 68000 communicates with the Z80 sound CPU via a single byte register.
 * Write a command, then wait for the Z80 to echo it back with bit 7 set.
 */

#define IO_SOUND (*(vu8 *)0x320000)

/*
 * ============================================================================
 *  SYSTEM CONTROL
 * ============================================================================
 */

#define IO_WATCHDOG (*(vu8 *)0x300001) /* Write any value to kick */

/*
 * ============================================================================
 *  BIOS VARIABLES
 * ============================================================================
 */

#define BIOS_SYSTEM_MODE (*(vu8 *)0x10FD80) /* System mode flags */
#define BIOS_MVS_FLAG    (*(vu8 *)0x10FD82) /* 0 = AES, 1 = MVS */
#define BIOS_COUNTRY     (*(vu8 *)0x10FD83) /* 0 = Japan, 1 = USA, 2 = Europe */
#define BIOS_VBLANK_FLAG (*(vu8 *)0x10FD8E) /* Set by VBlank interrupt */

#endif /* HW_IO_H */
