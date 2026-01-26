/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_bios.h
 * @brief NeoGeo BIOS call wrappers
 *
 * The NeoGeo BIOS provides several system functions that games can use.
 * These wrappers provide safe access to BIOS routines.
 *
 * IMPORTANT: BIOS calls can change system state. Use with care.
 *
 * BIOS ROM is at 0xC00000-0xC1FFFF (128KB).
 * BIOS RAM is at 0x10F000-0x10FFFF (4KB).
 */

#ifndef _NG_BIOS_H_
#define _NG_BIOS_H_

#include <ng_types.h>

/**
 * @defgroup bios BIOS Calls
 * @ingroup hal
 * @brief NeoGeo BIOS system function wrappers.
 * @{
 */

/* ============================================================================
 * BIOS Entry Points
 * ========================================================================== */

/** BIOS entry point addresses */
#define NG_BIOS_SYSTEM_RETURN 0xC00444 /**< Return to system (BIOS takes over) */
#define NG_BIOS_FIX_CLEAR     0xC004C2 /**< Clear fix layer */
#define NG_BIOS_LSP_1ST       0xC004C8 /**< Initialize LSPC */
#define NG_BIOS_MESS_OUT      0xC004CE /**< Print message to fix layer */
#define NG_BIOS_UPLOAD        0xC00546 /**< Upload data to VRAM */
#define NG_BIOS_CRED_DEC      0xC0054C /**< Decrement credit count */
#define NG_BIOS_CRED_CHK      0xC00552 /**< Check credit count */

/* ============================================================================
 * System Control
 * ========================================================================== */

/**
 * Return control to BIOS
 * Game should call this when returning to system menu.
 * Does not return.
 */
void NGBiosSystemReturn(void) __attribute__((noreturn));

/**
 * Perform soft reset
 * Resets the game without full hardware reset.
 */
void NGBiosSoftReset(void);

/**
 * Request eye-catcher (boot animation)
 * Typically used after cold boot.
 */
void NGBiosEyecatcher(void);

/* ============================================================================
 * Credit Management (Arcade)
 * ========================================================================== */

/**
 * Get current credit count
 *
 * @return Number of credits (0-99)
 */
u8 NGBiosGetCredits(void);

/**
 * Add credits
 * Typically called when coin is inserted.
 *
 * @param count Number of credits to add
 */
void NGBiosAddCredits(u8 count);

/**
 * Consume a credit
 * Called when player starts a game.
 *
 * @return 1 if credit was available and consumed, 0 if no credits
 */
u8 NGBiosUseCredit(void);

/* ============================================================================
 * BIOS Variables
 * ========================================================================== */

/**
 * Get pointer to BIOS game title string
 * Located in BIOS RAM at 0x10F800.
 *
 * @return Pointer to 16-character title string
 */
const char *NGBiosGetTitle(void);

/**
 * Get BIOS version
 *
 * @return BIOS version byte
 */
u8 NGBiosGetVersion(void);

/**
 * Check if running from development BIOS
 *
 * @return 1 if dev BIOS, 0 if retail
 */
u8 NGBiosIsDev(void);

/* ============================================================================
 * Fix Layer BIOS Calls
 * ========================================================================== */

/**
 * Clear fix layer using BIOS routine
 * Clears the entire 40x32 fix layer to blank tiles.
 */
void NGBiosFixClear(void);

/**
 * Print string to fix layer using BIOS
 * Uses BIOS_MESS_OUT for text output.
 *
 * @param x X position (0-39)
 * @param y Y position (0-31)
 * @param str String to print (null-terminated)
 */
void NGBiosFixPrint(u8 x, u8 y, const char *str);

/* ============================================================================
 * Calendar (Soft DIP equivalent)
 * ========================================================================== */

/**
 * Soft DIP structure (game settings stored in BIOS RAM)
 */
typedef struct {
    u8 time_per_credit; /**< Time limit per credit (0=infinite) */
    u8 difficulty;      /**< Game difficulty (0-3) */
    u8 lives;           /**< Number of lives (1-5) */
    u8 bonus_life;      /**< Bonus life score threshold */
    u8 demo_sound;      /**< Demo sound on/off */
    u8 reserved[3];     /**< Reserved for future use */
} NGSoftDip;

/**
 * Get pointer to soft DIP settings
 * These are configurable via the BIOS test menu.
 *
 * @return Pointer to soft DIP structure in BIOS RAM
 */
const NGSoftDip *NGBiosGetSoftDip(void);

/** @} */ /* end of bios group */

#endif /* _NG_BIOS_H_ */
