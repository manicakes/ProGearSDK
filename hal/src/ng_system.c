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
#define REG_DIPSW      (*(vu8 *)0x300001)  /* DIP switches (directly mapped) */
#define REG_COIN_LOCK  (*(vu8 *)0x300001)  /* Coin lockout (directly mapped to output) */
#define REG_LED_MARKER (*(vu8 *)0x380000)  /* LED/Marker output register */

/* RTC registers (directly mapped 4-bit uPD4990A via LSPC) */
#define REG_RTC_CTRL   (*(vu8 *)0x320001)  /* RTC control port */
#define REG_RTC_DATA   (*(vu8 *)0x380021)  /* RTC data port */

/* System output port bits */
#define COIN_COUNTER_1  0x01
#define COIN_COUNTER_2  0x02
#define COIN_LOCKOUT_1  0x04
#define COIN_LOCKOUT_2  0x08

/* RTC control bits */
#define RTC_CLK   0x01
#define RTC_STB   0x02
#define RTC_DATA  0x04
#define RTC_CMD   0x08

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

/* Send a 4-bit command to the RTC */
static void rtc_send_cmd(u8 cmd) {
    u8 i;
    volatile u8 *ctrl = (volatile u8 *)0x380021;

    for (i = 0; i < 4; i++) {
        u8 bit = (cmd >> i) & 1;
        /* Set data bit */
        *ctrl = bit ? RTC_DATA : 0;
        /* Clock pulse */
        *ctrl |= RTC_CLK;
        *ctrl &= ~RTC_CLK;
    }
    /* Strobe to latch command */
    *ctrl = RTC_STB;
    *ctrl = 0;
}

/* Read a single bit from RTC */
static u8 rtc_read_bit(void) {
    volatile u8 *data = (volatile u8 *)0x320001;
    return (*data >> 4) & 1;
}

/* Read 4 bits (one BCD digit) from RTC */
static u8 rtc_read_nibble(void) {
    volatile u8 *ctrl = (volatile u8 *)0x380021;
    u8 value = 0;
    u8 i;

    for (i = 0; i < 4; i++) {
        if (rtc_read_bit()) {
            value |= (1 << i);
        }
        /* Clock to shift next bit */
        *ctrl = RTC_CLK;
        *ctrl = 0;
    }
    return value;
}

u8 NGRtcIsAvailable(void) {
    /* RTC disabled - hardware access causes issues in some emulators */
    return 0;
}

u8 NGRtcRead(NGRtcTime *time) {
    /* RTC disabled - hardware access causes issues in some emulators */
    (void)time;
    return 0;

#if 0 /* Disabled RTC code */
    u8 lo, hi;

    if (!time)
        return 0;

    if (!NGSystemIsMVS())
        return 0;

    /* Send "Register Hold" command (0x0E) to freeze time for reading */
    rtc_send_cmd(0x0E);

    /* Send "Time Read" command (0x0D) */
    rtc_send_cmd(0x0D);

    /* Read time data (52 bits total, but we read main fields)
     * Format: SS:MM:HH:WW:DD:MM:YY (BCD)
     */

    /* Seconds (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->second = (u8)(hi * 10 + lo);

    /* Minutes (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->minute = (u8)(hi * 10 + lo);

    /* Hours (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->hour = (u8)(hi * 10 + lo);

    /* Day of week (1 nibble, but only 3 bits used) */
    time->weekday = rtc_read_nibble() & 0x07;

    /* Day (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->day = (u8)(hi * 10 + lo);

    /* Month (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->month = (u8)(hi * 10 + lo);

    /* Year (2 BCD digits) */
    lo = rtc_read_nibble();
    hi = rtc_read_nibble();
    time->year = (u8)(hi * 10 + lo);

    /* Send "Register Free" command (0x0F) to unfreeze */
    rtc_send_cmd(0x0F);

    return 1;
#endif /* Disabled RTC code */
}
