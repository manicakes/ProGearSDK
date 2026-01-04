/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// input.c - NeoGeo input handling implementation

#include <input.h>

// Hardware registers (directly mapped, directly readable)
// Note: 68000 is big-endian, odd addresses for low byte
#define REG_P1CNT    (*(volatile u8*)0x300000)
#define REG_STATUS_A (*(volatile u8*)0x320001)  // Odd byte for coins/service
#define REG_P2CNT    (*(volatile u8*)0x340000)
#define REG_STATUS_B (*(volatile u8*)0x380000)  // Start/Select/MVS flag

// Internal state per player
typedef struct {
    u16 current;      // Current frame state
    u16 previous;     // Previous frame state
    u16 pressed;      // Just pressed this frame
    u16 released;     // Just released this frame
    u16 hold_frames[10];      // Hold duration per button (indexed by bit position)
    u16 release_frames[10];   // Duration held before release
} InputState;

static InputState g_input[2];

// Forward declarations
static u16 read_player_input(u8 player);
static u16 read_system_input(void);

// System input state (coins, service, test)
typedef struct {
    u16 current;
    u16 previous;
    u16 pressed;
    u16 released;
} SystemState;

static SystemState g_system;

// Button bit positions for indexing
static const u16 BUTTON_BITS[] = {
    NG_BTN_UP, NG_BTN_DOWN, NG_BTN_LEFT, NG_BTN_RIGHT,
    NG_BTN_A, NG_BTN_B, NG_BTN_C, NG_BTN_D,
    NG_BTN_START, NG_BTN_SELECT
};
#define NUM_BUTTONS 10

// Helper: get button index from mask (for single button)
static u8 button_to_index(u16 button) {
    for (u8 i = 0; i < NUM_BUTTONS; i++) {
        if (button == BUTTON_BITS[i]) return i;
    }
    return 0;
}

void NGInputInit(void) {
    for (int p = 0; p < 2; p++) {
        // Seed with current hardware state so held buttons don't trigger "pressed"
        u16 initial = read_player_input(p);
        g_input[p].current = initial;
        g_input[p].previous = initial;
        g_input[p].pressed = 0;
        g_input[p].released = 0;
        for (int i = 0; i < NUM_BUTTONS; i++) {
            g_input[p].hold_frames[i] = 0;
            g_input[p].release_frames[i] = 0;
        }
    }

    // Initialize system input state
    u16 sys_initial = read_system_input();
    g_system.current = sys_initial;
    g_system.previous = sys_initial;
    g_system.pressed = 0;
    g_system.released = 0;
}

// Read hardware and convert to our button format
static u16 read_player_input(u8 player) {
    u16 result = 0;

    // Read joystick/buttons from P1CNT or P2CNT
    // These registers are active-low: 0 = pressed
    u8 joy = (player == 0) ? REG_P1CNT : REG_P2CNT;

    // Invert so 1 = pressed
    joy = ~joy;

    // P1CNT/P2CNT bit layout:
    // Bit 0: Up
    // Bit 1: Down
    // Bit 2: Left
    // Bit 3: Right
    // Bit 4: A
    // Bit 5: B
    // Bit 6: C
    // Bit 7: D
    if (joy & 0x01) result |= NG_BTN_UP;
    if (joy & 0x02) result |= NG_BTN_DOWN;
    if (joy & 0x04) result |= NG_BTN_LEFT;
    if (joy & 0x08) result |= NG_BTN_RIGHT;
    if (joy & 0x10) result |= NG_BTN_A;
    if (joy & 0x20) result |= NG_BTN_B;
    if (joy & 0x40) result |= NG_BTN_C;
    if (joy & 0x80) result |= NG_BTN_D;

    // Start/Select from STATUS_B register (directly active-high in MAME)
    // Bit 0: Start P1
    // Bit 1: Select P1
    // Bit 2: Start P2
    // Bit 3: Select P2
    u8 status = REG_STATUS_B;

    if (player == 0) {
        if (status & 0x01) result |= NG_BTN_START;
        if (status & 0x02) result |= NG_BTN_SELECT;
    } else {
        if (status & 0x04) result |= NG_BTN_START;
        if (status & 0x08) result |= NG_BTN_SELECT;
    }

    return result;
}

// Read system inputs (coins, service, test)
static u16 read_system_input(void) {
    u16 result = 0;

    // STATUS_A register ($320001) layout (active-low):
    // Bit 0: Coin 1
    // Bit 1: Coin 2
    // Bit 2: Service button
    // Bit 3: Coin 3 (4-slot)
    // Bit 4: Coin 4 (4-slot)
    // Bit 5: 4-slot/6-slot flag
    // Bit 6-7: RTC
    u8 status_a = ~REG_STATUS_A;  // Invert for active-high

    if (status_a & 0x01) result |= NG_SYS_COIN1;
    if (status_a & 0x02) result |= NG_SYS_COIN2;
    if (status_a & 0x04) result |= NG_SYS_SERVICE;

    // Note: Test switch location TBD - may require different register

    return result;
}

void NGInputUpdate(void) {
    for (int p = 0; p < 2; p++) {
        InputState *state = &g_input[p];

        // Shift current to previous
        state->previous = state->current;

        // Read new state
        state->current = read_player_input(p);

        // Calculate edge detection
        state->pressed = state->current & ~state->previous;
        state->released = ~state->current & state->previous;

        // Update hold/release counters for each button
        for (int i = 0; i < NUM_BUTTONS; i++) {
            u16 btn = BUTTON_BITS[i];

            if (state->current & btn) {
                // Button is held - increment counter (cap at max u16)
                if (state->hold_frames[i] < 0xFFFF) {
                    state->hold_frames[i]++;
                }
            } else {
                // Button not held
                if (state->released & btn) {
                    // Just released - store how long it was held
                    state->release_frames[i] = state->hold_frames[i];
                } else {
                    // Not released this frame - clear release duration
                    state->release_frames[i] = 0;
                }
                // Reset hold counter
                state->hold_frames[i] = 0;
            }
        }
    }

    // Update system inputs
    g_system.previous = g_system.current;
    g_system.current = read_system_input();
    g_system.pressed = g_system.current & ~g_system.previous;
    g_system.released = ~g_system.current & g_system.previous;
}

u8 NGInputHeld(u8 player, u16 buttons) {
    if (player > 1) return 0;
    return (g_input[player].current & buttons) == buttons;
}

u8 NGInputPressed(u8 player, u16 buttons) {
    if (player > 1) return 0;
    return (g_input[player].pressed & buttons) == buttons;
}

u8 NGInputReleased(u8 player, u16 buttons) {
    if (player > 1) return 0;
    return (g_input[player].released & buttons) == buttons;
}

u16 NGInputGetRaw(u8 player) {
    if (player > 1) return 0;
    return g_input[player].current;
}

s8 NGInputGetX(u8 player) {
    if (player > 1) return 0;
    u16 state = g_input[player].current;
    s8 x = 0;
    if (state & NG_BTN_LEFT) x -= 1;
    if (state & NG_BTN_RIGHT) x += 1;
    return x;
}

s8 NGInputGetY(u8 player) {
    if (player > 1) return 0;
    u16 state = g_input[player].current;
    s8 y = 0;
    if (state & NG_BTN_UP) y -= 1;
    if (state & NG_BTN_DOWN) y += 1;
    return y;
}

u16 NGInputHeldFrames(u8 player, u16 button) {
    if (player > 1) return 0;
    u8 idx = button_to_index(button);
    return g_input[player].hold_frames[idx];
}

u16 NGInputReleasedFrames(u8 player, u16 button) {
    if (player > 1) return 0;
    u8 idx = button_to_index(button);
    return g_input[player].release_frames[idx];
}

// === System Input Functions ===

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
