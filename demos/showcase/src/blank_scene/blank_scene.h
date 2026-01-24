/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// blank_scene.h - Blank scene demo

#ifndef BLANK_SCENE_H
#define BLANK_SCENE_H

#include <ng_types.h>

// Initialize the blank scene demo
void BlankSceneInit(void);

// Update the blank scene demo (call once per frame)
// Returns: 0 = no switch, 1 = ball demo, 2 = scroll demo
u8 BlankSceneUpdate(void);

// Cleanup the blank scene demo
void BlankSceneCleanup(void);

#endif // BLANK_SCENE_H
