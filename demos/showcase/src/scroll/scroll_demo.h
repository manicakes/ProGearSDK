/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// scroll_demo.h - Parallax scrolling demo

#ifndef SCROLL_DEMO_H
#define SCROLL_DEMO_H

#include <types.h>

// Initialize the scroll demo (creates parallax layers)
void ScrollDemoInit(void);

// Update the scroll demo (call once per frame)
// Returns menu action: 0 = none, 1 = switch to other demo
u8 ScrollDemoUpdate(void);

// Cleanup the scroll demo (remove all layers from scene)
void ScrollDemoCleanup(void);

#endif // SCROLL_DEMO_H
