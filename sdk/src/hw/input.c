/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/input.h>
#include <hw/io.h>

/*
 * Internal input state.
 * Maintains current, previous, edge-detected states for both players.
 */

typedef struct {
    u16 current;
    u16 previous;
    u16 pressed;
    u16 released;
    u16 hold_frames[10];
} InputState;

static InputState g_input[2];
static InputState g_system;

/* Button mapping for hold frame tracking */
static const u16 BUTTON_BITS[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A,
                                  BTN_B,  BTN_C,    BTN_D,    BTN_START, BTN_SELECT};
#define NUM_BUTTONS 10

/*
 * Read raw player input from hardware.
 * Hardware is active-low, so we invert the result.
 */
static u16 read_player_input(u8 player) {
    u16 result = 0;

    /* Joystick and ABCD buttons */
    u8 joy = (player == 0) ? IO_P1 : IO_P2;
    joy = ~joy; /* Invert: 0 = released, 1 = pressed */

    if (joy & IO_UP)
        result |= BTN_UP;
    if (joy & IO_DOWN)
        result |= BTN_DOWN;
    if (joy & IO_LEFT)
        result |= BTN_LEFT;
    if (joy & IO_RIGHT)
        result |= BTN_RIGHT;
    if (joy & IO_A)
        result |= BTN_A;
    if (joy & IO_B)
        result |= BTN_B;
    if (joy & IO_C)
        result |= BTN_C;
    if (joy & IO_D)
        result |= BTN_D;

    /* Start and Select from STATUS_B */
    u8 status = IO_STATUS_B;
    if (player == 0) {
        if (status & IO_P1_START)
            result |= BTN_START;
        if (status & IO_P1_SELECT)
            result |= BTN_SELECT;
    } else {
        if (status & IO_P2_START)
            result |= BTN_START;
        if (status & IO_P2_SELECT)
            result |= BTN_SELECT;
    }

    return result;
}

/*
 * Read system buttons (coins, service).
 */
static u16 read_system_input(void) {
    u16 result = 0;
    u8 status = ~IO_STATUS_A; /* Invert: active-low */

    if (status & IO_COIN1)
        result |= SYS_COIN1;
    if (status & IO_COIN2)
        result |= SYS_COIN2;
    if (status & IO_SERVICE)
        result |= SYS_SERVICE;

    return result;
}

static u8 button_to_index(u16 button) {
    for (u8 i = 0; i < NUM_BUTTONS; i++) {
        if (button == BUTTON_BITS[i])
            return i;
    }
    return 0;
}

void hw_input_init(void) {
    for (u8 p = 0; p < 2; p++) {
        u16 initial = read_player_input(p);
        g_input[p].current = initial;
        g_input[p].previous = initial;
        g_input[p].pressed = 0;
        g_input[p].released = 0;
        for (u8 i = 0; i < NUM_BUTTONS; i++) {
            g_input[p].hold_frames[i] = 0;
        }
    }

    u16 sys_initial = read_system_input();
    g_system.current = sys_initial;
    g_system.previous = sys_initial;
    g_system.pressed = 0;
    g_system.released = 0;
}

void hw_input_update(void) {
    /* Update player input */
    for (u8 p = 0; p < 2; p++) {
        InputState *state = &g_input[p];

        state->previous = state->current;
        state->current = read_player_input(p);
        state->pressed = state->current & ~state->previous;
        state->released = ~state->current & state->previous;

        /* Update hold frame counters */
        for (u8 i = 0; i < NUM_BUTTONS; i++) {
            u16 btn = BUTTON_BITS[i];
            if (state->current & btn) {
                if (state->hold_frames[i] < 0xFFFF)
                    state->hold_frames[i]++;
            } else {
                state->hold_frames[i] = 0;
            }
        }
    }

    /* Update system input */
    g_system.previous = g_system.current;
    g_system.current = read_system_input();
    g_system.pressed = g_system.current & ~g_system.previous;
    g_system.released = ~g_system.current & g_system.previous;
}

u16 hw_input_held(u8 player) {
    if (player > 1)
        return 0;
    return g_input[player].current;
}

u16 hw_input_pressed(u8 player) {
    if (player > 1)
        return 0;
    return g_input[player].pressed;
}

u16 hw_input_released(u8 player) {
    if (player > 1)
        return 0;
    return g_input[player].released;
}

u16 hw_input_raw(u8 player) {
    if (player > 1)
        return 0;
    return g_input[player].current;
}

s8 hw_input_axis_x(u8 player) {
    if (player > 1)
        return 0;
    u16 state = g_input[player].current;
    s8 x = 0;
    if (state & BTN_LEFT)
        x -= 1;
    if (state & BTN_RIGHT)
        x += 1;
    return x;
}

s8 hw_input_axis_y(u8 player) {
    if (player > 1)
        return 0;
    u16 state = g_input[player].current;
    s8 y = 0;
    if (state & BTN_UP)
        y -= 1;
    if (state & BTN_DOWN)
        y += 1;
    return y;
}

u16 hw_input_held_frames(u8 player, u16 button) {
    if (player > 1)
        return 0;
    u8 idx = button_to_index(button);
    return g_input[player].hold_frames[idx];
}

u16 hw_input_system_held(void) {
    return g_system.current;
}

u16 hw_input_system_pressed(void) {
    return g_system.pressed;
}
