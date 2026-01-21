/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <input.h>
#include <neogeo.h>

typedef struct {
    u16 current;
    u16 previous;
    u16 pressed;
    u16 released;
    u16 hold_frames[10];
    u16 release_frames[10];
} InputState;

static InputState g_input[2];

static u16 read_player_input(u8 player);
static u16 read_system_input(void);

typedef struct {
    u16 current;
    u16 previous;
    u16 pressed;
    u16 released;
} SystemState;

static SystemState g_system;

static const u16 BUTTON_BITS[] = {NG_BTN_UP, NG_BTN_DOWN, NG_BTN_LEFT, NG_BTN_RIGHT, NG_BTN_A,
                                  NG_BTN_B,  NG_BTN_C,    NG_BTN_D,    NG_BTN_START, NG_BTN_SELECT};
#define NUM_BUTTONS 10

static u8 button_to_index(u16 button) {
    for (u8 i = 0; i < NUM_BUTTONS; i++) {
        if (button == BUTTON_BITS[i])
            return i;
    }
    return 0;
}

void NGInputInit(void) {
    for (int p = 0; p < 2; p++) {
        u16 initial = read_player_input((u8)p);
        g_input[p].current = initial;
        g_input[p].previous = initial;
        g_input[p].pressed = 0;
        g_input[p].released = 0;
        for (int i = 0; i < NUM_BUTTONS; i++) {
            g_input[p].hold_frames[i] = 0;
            g_input[p].release_frames[i] = 0;
        }
    }

    u16 sys_initial = read_system_input();
    g_system.current = sys_initial;
    g_system.previous = sys_initial;
    g_system.pressed = 0;
    g_system.released = 0;
}

static u16 read_player_input(u8 player) {
    u16 result = 0;

    // P1CNT/P2CNT: active-low (0 = pressed)
    // Bits 0-7: Up, Down, Left, Right, A, B, C, D
    u8 joy = (player == 0) ? NG_REG_P1CNT : NG_REG_P2CNT;
    joy = ~joy;

    if (joy & 0x01)
        result |= NG_BTN_UP;
    if (joy & 0x02)
        result |= NG_BTN_DOWN;
    if (joy & 0x04)
        result |= NG_BTN_LEFT;
    if (joy & 0x08)
        result |= NG_BTN_RIGHT;
    if (joy & 0x10)
        result |= NG_BTN_A;
    if (joy & 0x20)
        result |= NG_BTN_B;
    if (joy & 0x40)
        result |= NG_BTN_C;
    if (joy & 0x80)
        result |= NG_BTN_D;

    // STATUS_B bits 0-3: Start P1, Select P1, Start P2, Select P2
    u8 status = NG_REG_STATUS_B;

    if (player == 0) {
        if (status & 0x01)
            result |= NG_BTN_START;
        if (status & 0x02)
            result |= NG_BTN_SELECT;
    } else {
        if (status & 0x04)
            result |= NG_BTN_START;
        if (status & 0x08)
            result |= NG_BTN_SELECT;
    }

    return result;
}

static u16 read_system_input(void) {
    u16 result = 0;

    // STATUS_A: active-low, bits 0-2 = Coin1, Coin2, Service
    u8 status_a = ~NG_REG_STATUS_A;

    if (status_a & 0x01)
        result |= NG_SYS_COIN1;
    if (status_a & 0x02)
        result |= NG_SYS_COIN2;
    if (status_a & 0x04)
        result |= NG_SYS_SERVICE;

    return result;
}

void NGInputUpdate(void) {
    for (int p = 0; p < 2; p++) {
        InputState *state = &g_input[p];

        state->previous = state->current;
        state->current = read_player_input((u8)p);
        state->pressed = state->current & ~state->previous;
        state->released = ~state->current & state->previous;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            u16 btn = BUTTON_BITS[i];

            if (state->current & btn) {
                if (state->hold_frames[i] < 0xFFFF) {
                    state->hold_frames[i]++;
                }
            } else {
                if (state->released & btn) {
                    state->release_frames[i] = state->hold_frames[i];
                } else {
                    state->release_frames[i] = 0;
                }
                state->hold_frames[i] = 0;
            }
        }
    }

    g_system.previous = g_system.current;
    g_system.current = read_system_input();
    g_system.pressed = g_system.current & ~g_system.previous;
    g_system.released = ~g_system.current & g_system.previous;
}

u8 NGInputHeld(u8 player, u16 buttons) {
    if (player > 1)
        return 0;
    return (g_input[player].current & buttons) == buttons;
}

u8 NGInputPressed(u8 player, u16 buttons) {
    if (player > 1)
        return 0;
    return (g_input[player].pressed & buttons) == buttons;
}

u8 NGInputReleased(u8 player, u16 buttons) {
    if (player > 1)
        return 0;
    return (g_input[player].released & buttons) == buttons;
}

u16 NGInputGetRaw(u8 player) {
    if (player > 1)
        return 0;
    return g_input[player].current;
}

s8 NGInputGetX(u8 player) {
    if (player > 1)
        return 0;
    u16 state = g_input[player].current;
    s8 x = 0;
    if (state & NG_BTN_LEFT)
        x -= 1;
    if (state & NG_BTN_RIGHT)
        x += 1;
    return x;
}

s8 NGInputGetY(u8 player) {
    if (player > 1)
        return 0;
    u16 state = g_input[player].current;
    s8 y = 0;
    if (state & NG_BTN_UP)
        y -= 1;
    if (state & NG_BTN_DOWN)
        y += 1;
    return y;
}

u16 NGInputHeldFrames(u8 player, u16 button) {
    if (player > 1)
        return 0;
    u8 idx = button_to_index(button);
    return g_input[player].hold_frames[idx];
}

u16 NGInputReleasedFrames(u8 player, u16 button) {
    if (player > 1)
        return 0;
    u8 idx = button_to_index(button);
    return g_input[player].release_frames[idx];
}

u8 NGSystemHeld(u16 buttons) {
    return (g_system.current & buttons) == buttons;
}

u8 NGSystemPressed(u16 buttons) {
    return (g_system.pressed & buttons) == buttons;
}

u8 NGSystemReleased(u16 buttons) {
    return (g_system.released & buttons) == buttons;
}

u16 NGSystemGetRaw(void) {
    return g_system.current;
}
