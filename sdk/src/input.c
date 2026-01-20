/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file input.c
 * @brief Public input API - thin wrapper over hw/input.
 */

#include <input.h>
#include <hw/input.h>

u8 InputHeld(u8 player, u16 buttons) {
    return (hw_input_held(player) & buttons) == buttons;
}

u8 InputPressed(u8 player, u16 buttons) {
    return (hw_input_pressed(player) & buttons) == buttons;
}

u8 InputReleased(u8 player, u16 buttons) {
    return (hw_input_released(player) & buttons) == buttons;
}

s8 InputAxisX(u8 player) {
    return hw_input_axis_x(player);
}

s8 InputAxisY(u8 player) {
    return hw_input_axis_y(player);
}

u16 InputRaw(u8 player) {
    return hw_input_raw(player);
}

u16 InputHeldFrames(u8 player, u16 button) {
    return hw_input_held_frames(player, button);
}

u8 InputAny(u8 player) {
    return hw_input_held(player) != 0;
}

u8 SystemHeld(u16 buttons) {
    return (hw_input_system_held() & buttons) == buttons;
}

u8 SystemPressed(u16 buttons) {
    return (hw_input_system_pressed() & buttons) == buttons;
}

void InputInit(void) {
    hw_input_init();
}

void InputUpdate(void) {
    hw_input_update();
}
