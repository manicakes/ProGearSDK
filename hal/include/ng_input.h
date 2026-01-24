/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_input.h
 * @brief NeoGeo input handling.
 *
 * Provides button state polling with edge detection for responsive controls.
 *
 * @section inputusage Usage
 * 1. Call NGInputInit() once at startup
 * 2. Call NGInputUpdate() once per frame (after vblank)
 * 3. Use NGInputPressed() for actions (jump, shoot)
 * 4. Use NGInputHeld() for continuous input (movement)
 *
 * @section inputedge Edge Detection
 * - NGInputPressed(): True only on first frame button is pressed
 * - NGInputReleased(): True only on frame button is released
 * - NGInputHeld(): True every frame button is down
 */

#ifndef _NG_INPUT_H_
#define _NG_INPUT_H_

#include <ng_types.h>

/** @defgroup btnmask Button Masks
 *  @brief Bitmasks for player buttons.
 *  @{
 */

#define NG_BTN_UP     0x0001 /**< D-pad up */
#define NG_BTN_DOWN   0x0002 /**< D-pad down */
#define NG_BTN_LEFT   0x0004 /**< D-pad left */
#define NG_BTN_RIGHT  0x0008 /**< D-pad right */
#define NG_BTN_A      0x0010 /**< A button */
#define NG_BTN_B      0x0020 /**< B button */
#define NG_BTN_C      0x0040 /**< C button */
#define NG_BTN_D      0x0080 /**< D button */
#define NG_BTN_START  0x0100 /**< Start button */
#define NG_BTN_SELECT 0x0200 /**< Select button */

#define NG_BTN_DIR  (NG_BTN_UP | NG_BTN_DOWN | NG_BTN_LEFT | NG_BTN_RIGHT) /**< All directions */
#define NG_BTN_FACE (NG_BTN_A | NG_BTN_B | NG_BTN_C | NG_BTN_D)            /**< All face buttons */
#define NG_BTN_ALL  0x03FF                                                 /**< All buttons */

/** @} */

/** @defgroup sysbtn System Button Masks
 *  @brief Bitmasks for machine-level inputs.
 *  @{
 */

#define NG_SYS_COIN1   0x0001 /**< Player 1 coin slot */
#define NG_SYS_COIN2   0x0002 /**< Player 2 coin slot */
#define NG_SYS_SERVICE 0x0004 /**< Service button */
#define NG_SYS_TEST    0x0008 /**< Test button */
#define NG_SYS_ALL     0x000F /**< All system inputs */

/** @} */

/** @defgroup playerid Player Identifiers
 *  @{
 */

#define NG_PLAYER_1 0 /**< Player 1 */
#define NG_PLAYER_2 1 /**< Player 2 */

/** @} */

/** @defgroup inputsys Input System Functions
 *  @{
 */

/**
 * Initialize input system.
 * Call once at startup.
 */
void NGInputInit(void);

/**
 * Update input state.
 * Call once per frame after vblank.
 */
void NGInputUpdate(void);

/** @} */

/** @defgroup inputstate State Queries
 *  @{
 */

/**
 * Check if buttons are currently held.
 * @param player Player index (NG_PLAYER_1 or NG_PLAYER_2)
 * @param buttons Button mask to check
 * @return Non-zero if ALL specified buttons are held
 */
u8 NGInputHeld(u8 player, u16 buttons);

/**
 * Check if buttons were just pressed this frame.
 * @param player Player index
 * @param buttons Button mask to check
 * @return Non-zero if ALL specified buttons were just pressed
 */
u8 NGInputPressed(u8 player, u16 buttons);

/**
 * Check if buttons were just released this frame.
 * @param player Player index
 * @param buttons Button mask to check
 * @return Non-zero if ALL specified buttons were just released
 */
u8 NGInputReleased(u8 player, u16 buttons);

/**
 * Get raw button state as bitmask.
 * @param player Player index
 * @return Button bitmask
 */
u16 NGInputGetRaw(u8 player);

/** @} */

/** @defgroup inputdir Direction Helpers
 *  @{
 */

/**
 * Get horizontal direction.
 * @param player Player index
 * @return -1 (left), 0 (neutral), or +1 (right)
 */
s8 NGInputGetX(u8 player);

/**
 * Get vertical direction.
 * @param player Player index
 * @return -1 (up), 0 (neutral), or +1 (down)
 */
s8 NGInputGetY(u8 player);

/** @} */

/** @defgroup inputcharge Charge/Hold Duration
 *  @{
 */

/**
 * Get frames a button has been held.
 * @param player Player index
 * @param button Single button mask
 * @return Frame count, or 0 if not held
 */
u16 NGInputHeldFrames(u8 player, u16 button);

/**
 * Get how long button was held before release.
 * Only valid on the frame the button was released.
 * @param player Player index
 * @param button Single button mask
 * @return Frame count held, or 0 if not just released
 */
u16 NGInputReleasedFrames(u8 player, u16 button);

/** @} */

/** @defgroup sysquery System Input Queries
 *  @brief Machine-level inputs (coins, service, test).
 *  @{
 */

/**
 * Check if system buttons are held.
 * @param buttons System button mask
 * @return Non-zero if held
 */
u8 NGSystemHeld(u16 buttons);

/**
 * Check if system buttons were just pressed.
 * Use for coin insertion detection.
 * @param buttons System button mask
 * @return Non-zero if just pressed
 */
u8 NGSystemPressed(u16 buttons);

/**
 * Check if system buttons were just released.
 * @param buttons System button mask
 * @return Non-zero if just released
 */
u8 NGSystemReleased(u16 buttons);

/**
 * Get raw system input state.
 * @return System input bitmask
 */
u16 NGSystemGetRaw(void);

/** @} */

#endif // _NG_INPUT_H_
