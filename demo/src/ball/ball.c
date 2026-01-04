/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ball.c - Bouncing ball entity using physics engine and arena allocation

#include "ball.h"
#include <actor.h>
#include <audio.h>
#include <camera.h>
#include <palette.h>
#include <physics.h>
#include <ngres_generated_assets.h>

#define BALL_HALF_SIZE FIX(16)  // 16 pixels (32x32 sprite)

// Palette indices for each ball color (from generated assets)
static const u8 ball_palettes[] = {
    NGPAL_BALL_DEFAULT,
    NGPAL_BALL_RED,
    NGPAL_BALL_GREEN,
    NGPAL_BALL_BLUE,
    NGPAL_BALL_YELLOW,
    NGPAL_BALL_CYAN,
    NGPAL_BALL_MAGENTA,
    NGPAL_BALL_WHITE,
};

#define NUM_PALETTES (sizeof(ball_palettes) / sizeof(ball_palettes[0]))

typedef struct Ball {
    NGBodyHandle body;
    NGActorHandle actor;
    u8 active;
} Ball;

typedef struct BallSystem {
    NGPhysWorldHandle physics;
    Ball *balls;           // Allocated from arena
    u8 max_balls;
    u8 ball_count;
    u16 random_seed;
} BallSystem;

// Simple pseudo-random number generator
static u16 random(BallSystemHandle sys) {
    sys->random_seed = sys->random_seed * 25173 + 13849;
    return sys->random_seed;
}

static fixed random_range_fix(BallSystemHandle sys, fixed min, fixed max) {
    u16 r = random(sys);
    fixed range = max - min;
    return min + FIX_MUL(range, FIX(r) / 65535);
}

static s16 random_range(BallSystemHandle sys, s16 min, s16 max) {
    return min + (random(sys) % (max - min + 1));
}

// Collision callback - shake camera and play sound when balls collide
static void on_ball_collision(NGCollision *collision, void *user_data) {
    (void)user_data;

    // Get ball from first body and play hit sound (positional audio)
    Ball *ball = (Ball *)NGPhysBodyGetUserData(collision->body_a);
    if (ball && ball->actor) {
        // Play sound effect with stereo panning based on ball position
        NGActorPlaySfx(ball->actor, NGSFX_BALL_HIT);
    }

    // Boundary collisions don't trigger this callback, only ball-ball collisions
    NGCameraShake(2, 4);
}

BallSystemHandle BallSystemCreate(NGArena *arena, u8 max_balls) {
    // Allocate system struct from arena
    BallSystem *sys = NG_ARENA_ALLOC(arena, BallSystem);
    if (!sys) return 0;

    // Allocate ball array from arena
    sys->balls = NG_ARENA_ALLOC_ARRAY(arena, Ball, max_balls);
    if (!sys->balls) return 0;

    sys->max_balls = max_balls;
    sys->ball_count = 0;
    sys->random_seed = 12345;

    // Initialize ball slots
    for (u8 i = 0; i < max_balls; i++) {
        sys->balls[i].active = 0;
    }

    // Create physics world with bounds from asset dimensions
    // Brick is centered within sky
    sys->physics = NGPhysWorldCreate();
    const u8 offset = 16;
    NGPhysWorldSetBounds(sys->physics,
                         FIX(offset),
                         FIX(NGVisualAsset_brick.width_pixels - offset),
                         FIX(offset),
                         FIX(NGVisualAsset_brick.height_pixels - offset));
    NGPhysWorldSetGravity(sys->physics, 0, FIX(1));

    return sys;
}

void BallSystemDestroy(BallSystemHandle sys) {
    if (!sys) return;

    // Destroy all active balls
    for (u8 i = 0; i < sys->max_balls; i++) {
        if (sys->balls[i].active) {
            NGActorRemoveFromScene(sys->balls[i].actor);
            NGActorDestroy(sys->balls[i].actor);
            NGPhysBodyDestroy(sys->balls[i].body);
            sys->balls[i].active = 0;
        }
    }

    // Release the physics world slot
    NGPhysWorldDestroy(sys->physics);
}

void BallSystemUpdate(BallSystemHandle sys) {
    if (!sys) return;

    // Update physics with collision callback for ball-ball collisions
    NGPhysWorldUpdate(sys->physics, on_ball_collision, 0);

    // Sync actor positions to physics bodies
    // Physics uses center position, actor uses top-left
    for (u8 i = 0; i < sys->max_balls; i++) {
        Ball *ball = &sys->balls[i];
        if (!ball->active) continue;

        NGVec2 pos = NGPhysBodyGetPos(ball->body);
        fixed x = pos.x - FIX(16);  // offset to top-left (32x32 sprite)
        fixed y = pos.y - FIX(16);
        NGActorSetPos(ball->actor, x, y);
    }
}

u8 BallSpawn(BallSystemHandle sys) {
    if (!sys) return 0;

    // Find free slot
    for (u8 i = 0; i < sys->max_balls; i++) {
        if (!sys->balls[i].active) {
            Ball *ball = &sys->balls[i];

            // Random position within brick bounds (away from edges)
            fixed x = random_range_fix(sys, FIX(85), FIX(NGVisualAsset_brick.width_pixels - 85));
            fixed y = random_range_fix(sys, FIX(85), FIX(NGVisualAsset_brick.height_pixels - 85));

            // Random velocity
            fixed vx = FIX(random_range(sys, -3, 3));
            fixed vy = FIX(random_range(sys, -3, 3));
            if (vx == 0) vx = FIX(1);
            if (vy == 0) vy = FIX(1);

            // Create physics body (AABB for simple box collision)
            ball->body = NGPhysBodyCreateAABB(sys->physics, x, y, BALL_HALF_SIZE, BALL_HALF_SIZE);
            NGPhysBodySetVel(ball->body, vx, vy);
            NGPhysBodySetRestitution(ball->body, FIX_FROM_FLOAT(1));  // Perfect bounce
            NGPhysBodySetUserData(ball->body, ball);

            // Create actor with unique palette per ball (cycling through available colors)
            ball->actor = NGActorCreate(&NGVisualAsset_ball, 0, 0);
            NGActorAddToScene(ball->actor, x - FIX(16), y - FIX(16), 100);  // Z=100, in front of backgrounds
            NGActorSetPalette(ball->actor, ball_palettes[i % NUM_PALETTES]);
            NGActorSetAnimByName(ball->actor, "spin");
            ball->active = 1;
            sys->ball_count++;

            return 1;
        }
    }
    return 0;  // Pool full
}

u8 BallDestroyLast(BallSystemHandle sys) {
    if (!sys) return 0;

    // Find last active ball (highest index)
    for (s8 i = sys->max_balls - 1; i >= 0; i--) {
        if (sys->balls[i].active) {
            NGActorDestroy(sys->balls[i].actor);
            NGPhysBodyDestroy(sys->balls[i].body);
            sys->balls[i].active = 0;
            sys->ball_count--;
            return 1;
        }
    }
    return 0;  // No balls to destroy
}

u8 BallCount(BallSystemHandle sys) {
    if (!sys) return 0;
    return sys->ball_count;
}
