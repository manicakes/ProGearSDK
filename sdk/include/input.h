/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file input.h
 * @brief Controller input.
 *
 * Simple input queries with automatic edge detection.
 * No setup required - just query button states any time.
 *
 * @example
 * // Check if player pressed jump
 * if (InputPressed(PLAYER_1, BUTTON_A)) {
 *     player_jump();
 * }
 *
 * // Get movement direction
 * s8 move_x = InputAxisX(PLAYER_1);
 * s8 move_y = InputAxisY(PLAYER_1);
 */

#ifndef INPUT_H
#define INPUT_H

#include <types.h>

/**
 * @defgroup input Input
 * @brief Controller input with edge detection.
 * @{
 */

/** Player identifiers */
#define PLAYER_1 0
#define PLAYER_2 1

/** Button masks */
#define BUTTON_UP     0x0001
#define BUTTON_DOWN   0x0002
#define BUTTON_LEFT   0x0004
#define BUTTON_RIGHT  0x0008
#define BUTTON_A      0x0010
#define BUTTON_B      0x0020
#define BUTTON_C      0x0040
#define BUTTON_D      0x0080
#define BUTTON_START  0x0100
#define BUTTON_SELECT 0x0200

/** Direction mask (all four directions) */
#define BUTTON_DIRS (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)

/** Face button mask (A, B, C, D) */
#define BUTTON_FACE (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_D)

/** All buttons */
#define BUTTON_ALL 0x03FF

/** System button masks */
#define SYSTEM_COIN1   0x0001
#define SYSTEM_COIN2   0x0002
#define SYSTEM_SERVICE 0x0004

/**
 * Check if buttons are currently held down.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @param buttons Button mask (can combine with |)
 * @return Non-zero if ALL specified buttons are held
 */
u8 InputHeld(u8 player, u16 buttons);

/**
 * Check if buttons were just pressed this frame.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @param buttons Button mask
 * @return Non-zero if ALL specified buttons were just pressed
 */
u8 InputPressed(u8 player, u16 buttons);

/**
 * Check if buttons were just released this frame.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @param buttons Button mask
 * @return Non-zero if ALL specified buttons were just released
 */
u8 InputReleased(u8 player, u16 buttons);

/**
 * Get horizontal axis direction.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @return -1 (left), 0 (neutral), or +1 (right)
 */
s8 InputAxisX(u8 player);

/**
 * Get vertical axis direction.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @return -1 (up), 0 (neutral), or +1 (down)
 */
s8 InputAxisY(u8 player);

/**
 * Get raw button state (no edge detection).
 *
 * @param player PLAYER_1 or PLAYER_2
 * @return Current button bitmask
 */
u16 InputRaw(u8 player);

/**
 * Get number of frames a button has been held.
 * Useful for charge attacks or hold-to-repeat.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @param button Single button mask
 * @return Frame count (0 if not held)
 */
u16 InputHeldFrames(u8 player, u16 button);

/**
 * Check if any button is pressed.
 *
 * @param player PLAYER_1 or PLAYER_2
 * @return Non-zero if any button is held
 */
u8 InputAny(u8 player);

/*
 * Internal functions - called by engine.
 */
void InputInit(void);
void InputUpdate(void);

/**
 * Check system buttons (coins, service).
 *
 * @param buttons System button mask
 * @return Non-zero if ALL specified buttons are held
 */
u8 SystemHeld(u16 buttons);

/**
 * Check if system buttons were just pressed.
 */
u8 SystemPressed(u16 buttons);

/** @} */

#endif /* INPUT_H */
