/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ball_demo.h - Bouncing balls demo

#ifndef BALL_DEMO_H
#define BALL_DEMO_H

#include <types.h>

// Initialize the ball demo (creates background, balls, etc.)
void BallDemoInit(void);

// Update the ball demo (call once per frame)
// Returns menu action: 0 = none, 1 = switch to other demo
u8 BallDemoUpdate(void);

// Cleanup the ball demo (remove all actors from scene)
void BallDemoCleanup(void);

#endif // BALL_DEMO_H
