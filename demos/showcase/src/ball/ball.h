/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ball.h - Bouncing ball entity

#ifndef BALL_H
#define BALL_H

#include <types.h>
#include <arena.h>

// Opaque handle to ball system
typedef struct BallSystem *BallSystemHandle;

// Create ball system using arena allocation
// arena: Arena to allocate from (typically ng_arena_state)
// max_balls: Maximum number of balls to support
BallSystemHandle BallSystemCreate(NGArena *arena, u8 max_balls);

// Destroy ball system and release physics world
void BallSystemDestroy(BallSystemHandle sys);

// Update all balls (call once per frame)
void BallSystemUpdate(BallSystemHandle sys);

// Spawn a new ball at random position with random velocity
// Returns 1 on success, 0 if pool is full
u8 BallSpawn(BallSystemHandle sys);

// Destroy the most recently spawned ball
// Returns 1 if a ball was destroyed, 0 if no balls exist
u8 BallDestroyLast(BallSystemHandle sys);

// Get current ball count
u8 BallCount(BallSystemHandle sys);

#endif // BALL_H
