/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/input.h
 * @brief Input service.
 * @internal This is an internal header - not for game developers.
 *
 * Handles raw input reading and maintains button state for edge detection.
 * Call hw_input_update() once per frame before querying button states.
 */

#ifndef HW_INPUT_H
#define HW_INPUT_H

#include <types.h>

/*
 * ============================================================================
 *  BUTTON MASKS
 * ============================================================================
 *
 * These are the canonical button masks used throughout the SDK.
 * Input hardware is active-low; these represent the processed (active-high) state.
 */

#define BTN_UP     0x0001
#define BTN_DOWN   0x0002
#define BTN_LEFT   0x0004
#define BTN_RIGHT  0x0008
#define BTN_A      0x0010
#define BTN_B      0x0020
#define BTN_C      0x0040
#define BTN_D      0x0080
#define BTN_START  0x0100
#define BTN_SELECT 0x0200

/* System buttons */
#define SYS_COIN1   0x0001
#define SYS_COIN2   0x0002
#define SYS_SERVICE 0x0004

/**
 * Initialize input system.
 */
void hw_input_init(void);

/**
 * Update input state. Call once per frame.
 * Reads hardware registers and computes edge detection.
 */
void hw_input_update(void);

/**
 * Get currently held buttons.
 * @param player 0 or 1
 * @return Bitmask of held buttons
 */
u16 hw_input_held(u8 player);

/**
 * Get buttons pressed this frame.
 * @param player 0 or 1
 * @return Bitmask of just-pressed buttons
 */
u16 hw_input_pressed(u8 player);

/**
 * Get buttons released this frame.
 * @param player 0 or 1
 * @return Bitmask of just-released buttons
 */
u16 hw_input_released(u8 player);

/**
 * Get raw button state (no edge detection).
 * @param player 0 or 1
 * @return Current button bitmask
 */
u16 hw_input_raw(u8 player);

/**
 * Get X-axis direction.
 * @param player 0 or 1
 * @return -1 (left), 0 (neutral), or +1 (right)
 */
s8 hw_input_axis_x(u8 player);

/**
 * Get Y-axis direction.
 * @param player 0 or 1
 * @return -1 (up), 0 (neutral), or +1 (down)
 */
s8 hw_input_axis_y(u8 player);

/**
 * Get frames a button has been held.
 * @param player 0 or 1
 * @param button Single button mask
 * @return Number of frames held (0 if not held)
 */
u16 hw_input_held_frames(u8 player, u16 button);

/**
 * Get system button state (coins, service).
 * @return Current system button bitmask
 */
u16 hw_input_system_held(void);

/**
 * Get system buttons pressed this frame.
 */
u16 hw_input_system_pressed(void);

#endif /* HW_INPUT_H */
