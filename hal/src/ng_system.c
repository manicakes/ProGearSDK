/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_system.c
 * @brief NeoGeo system features implementation
 */

#include <ng_system.h>
#include <ng_hardware.h>

/* Hardware register addresses */
#define REG_DIPSW      (*(vu8 *)0x300001) /* DIP switches (directly mapped) */
#define REG_COIN_LOCK  (*(vu8 *)0x300001) /* Coin lockout (directly mapped to output) */
#define REG_LED_MARKER (*(vu8 *)0x380000) /* LED/Marker output register */

/* System output port bits */
#define COIN_COUNTER_1 0x01
#define COIN_COUNTER_2 0x02
#define COIN_LOCKOUT_1 0x04
#define COIN_LOCKOUT_2 0x08

/* Track coin lockout state (we need to read-modify-write) */
static u8 coin_lockout_state = 0;

/* ============================================================================
 * System Detection
 * ========================================================================== */

u8 NGSystemIsMVS(void) {
    return NG_BIOS_MVS_FLAG;
}

u8 NGSystemGetCountry(void) {
    return NG_BIOS_COUNTRY;
}

/* ============================================================================
 * DIP Switches
 * ========================================================================== */

u8 NGDipReadRaw(void) {
    return REG_DIPSW;
}

u8 NGDipIsSet(NGDipBit bit) {
    /* DIP switches are active-low, invert for convenience */
    return (REG_DIPSW & bit) ? 0 : 1;
}

u8 NGDipTestMode(void) {
    return NGDipIsSet(NG_DIP_SETTING_MODE);
}

u8 NGDipFreePlay(void) {
    return NGDipIsSet(NG_DIP_FREE_PLAY);
}

/* ============================================================================
 * Coin Counters and Lockouts (MVS only)
 * ========================================================================== */

/* Internal: pulse a coin counter by toggling the bit */
static void pulse_counter(u8 bit) {
    volatile u8 *output = (volatile u8 *)0x3A0001;
    u8 i;

    /* Set bit high (counter active) */
    *output = bit;

    /* Brief delay (~50us) for mechanical counter */
    for (i = 0; i < 50; i++) {
        __asm__ volatile("nop");
    }

    /* Set bit low */
    *output = 0;
}

void NGCoinCounterP1(void) {
    if (!NGSystemIsMVS())
        return;
    pulse_counter(COIN_COUNTER_1);
}

void NGCoinCounterP2(void) {
    if (!NGSystemIsMVS())
        return;
    pulse_counter(COIN_COUNTER_2);
}

void NGCoinLockoutP1(u8 locked) {
    volatile u8 *output = (volatile u8 *)0x3A0011;
    if (!NGSystemIsMVS())
        return;

    if (locked) {
        coin_lockout_state |= COIN_LOCKOUT_1;
    } else {
        coin_lockout_state &= ~COIN_LOCKOUT_1;
    }
    *output = coin_lockout_state;
}

void NGCoinLockoutP2(u8 locked) {
    volatile u8 *output = (volatile u8 *)0x3A0011;
    if (!NGSystemIsMVS())
        return;

    if (locked) {
        coin_lockout_state |= COIN_LOCKOUT_2;
    } else {
        coin_lockout_state &= ~COIN_LOCKOUT_2;
    }
    *output = coin_lockout_state;
}

/* ============================================================================
 * Real-Time Clock (MVS only, uPD4990A)
 * ========================================================================== */

u8 NGRtcIsAvailable(void) {
    /* RTC disabled - hardware access causes issues in some emulators */
    return 0;
}

u8 NGRtcRead(NGRtcTime *time) {
    /* RTC disabled - hardware access causes issues in some emulators */
    (void)time;
    return 0;
}
