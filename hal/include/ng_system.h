/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_system.h
 * @brief NeoGeo system features - DIP switches, coin counters, RTC
 *
 * This module provides access to:
 * - DIP switch reading (arcade configuration)
 * - Coin counter and lockout control (MVS only)
 * - Real-time clock access (MVS only)
 * - System detection (AES vs MVS)
 */

#ifndef _NG_SYSTEM_H_
#define _NG_SYSTEM_H_

#include <ng_types.h>

/**
 * @defgroup system System Features
 * @ingroup hal
 * @brief DIP switches, coin counters, and RTC access.
 * @{
 */

/* ============================================================================
 * System Detection
 * ========================================================================== */

/**
 * Check if running on MVS (arcade) hardware
 *
 * @return 1 if MVS, 0 if AES (home console)
 */
u8 NGSystemIsMVS(void);

/**
 * Get system country code
 *
 * @return 0=Japan, 1=USA, 2=Europe
 */
u8 NGSystemGetCountry(void);

/* ============================================================================
 * DIP Switches (Directly readable on both AES and MVS)
 * ========================================================================== */

/**
 * DIP switch bit definitions (directly from hardware manual)
 * Active-low on hardware, but these functions return active-high for convenience
 */
typedef enum {
    NG_DIP_SETTING_MODE = 0x01,   /**< Test mode: 0=normal, 1=test mode */
    NG_DIP_COIN_CHUTE = 0x02,     /**< Coin chute mode: 0=A+B, 1=A+A */
    NG_DIP_AUTOFIRE = 0x04,       /**< Autofire: 0=enabled, 1=disabled */
    NG_DIP_FREE_PLAY = 0x08,      /**< Free play: 0=disabled, 1=enabled */
    NG_DIP_FREEZE = 0x10,         /**< Freeze: 0=normal, 1=freeze */
    NG_DIP_SYSTEM_DISPLAY = 0x20, /**< System: 0=normal, 1=show memory card */
    NG_DIP_MULTIPLAY = 0x40,      /**< Multiplay: 0=disabled, 1=enabled */
    NG_DIP_CABINET = 0x80,        /**< Cabinet: 0=normal, 1=VS mode/cocktail */
} NGDipBit;

/**
 * Read raw DIP switch register
 * Note: Hardware is active-low, this returns raw value
 *
 * @return Raw DIP switch byte (active-low)
 */
u8 NGDipReadRaw(void);

/**
 * Check if a specific DIP switch setting is enabled
 * Handles active-low to active-high conversion
 *
 * @param bit DIP switch bit to check (NGDipBit)
 * @return 1 if enabled, 0 if disabled
 */
u8 NGDipIsSet(NGDipBit bit);

/**
 * Check if test mode is enabled via DIP switch
 *
 * @return 1 if test mode enabled, 0 if normal
 */
u8 NGDipTestMode(void);

/**
 * Check if free play is enabled via DIP switch
 *
 * @return 1 if free play enabled, 0 if coin required
 */
u8 NGDipFreePlay(void);

/* ============================================================================
 * Coin Counters and Lockouts (MVS only)
 * ========================================================================== */

/**
 * Pulse coin counter for player 1
 * Increments the mechanical coin counter on MVS cabinets.
 * No-op on AES.
 */
void NGCoinCounterP1(void);

/**
 * Pulse coin counter for player 2
 * Increments the mechanical coin counter on MVS cabinets.
 * No-op on AES.
 */
void NGCoinCounterP2(void);

/**
 * Set coin lockout state for player 1
 * When locked, the coin slot will reject coins.
 *
 * @param locked 1 to lock (reject coins), 0 to unlock (accept coins)
 */
void NGCoinLockoutP1(u8 locked);

/**
 * Set coin lockout state for player 2
 *
 * @param locked 1 to lock (reject coins), 0 to unlock (accept coins)
 */
void NGCoinLockoutP2(u8 locked);

/* ============================================================================
 * Real-Time Clock (MVS only)
 * ========================================================================== */

/**
 * RTC time structure
 */
typedef struct {
    u8 year;   /**< Year (0-99, add to base year) */
    u8 month;  /**< Month (1-12) */
    u8 day;    /**< Day of month (1-31) */
    u8 weekday; /**< Day of week (0=Sunday, 6=Saturday) */
    u8 hour;   /**< Hour (0-23) */
    u8 minute; /**< Minute (0-59) */
    u8 second; /**< Second (0-59) */
} NGRtcTime;

/**
 * Read current date/time from MVS real-time clock
 *
 * @param time Pointer to structure to fill with current time
 * @return 1 on success, 0 if RTC not available (AES or read error)
 */
u8 NGRtcRead(NGRtcTime *time);

/**
 * Check if RTC is available (MVS with working RTC chip)
 *
 * @return 1 if RTC available, 0 if not
 */
u8 NGRtcIsAvailable(void);

/** @} */ /* end of system group */

#endif /* _NG_SYSTEM_H_ */
