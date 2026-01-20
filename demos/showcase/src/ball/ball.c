/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "ball.h"
#include <actor.h>
#include <audio.h>
#include <camera.h>
#include <palette.h>
#include <physics.h>
#include <progear_assets.h>

#define BALL_HALF_SIZE FIX(16) // 16 pixels (32x32 sprite)

static const u8 ball_palettes[] = {
    PAL_BALL_DEFAULT, PAL_BALL_RED,  PAL_BALL_GREEN,   PAL_BALL_BLUE,
    PAL_BALL_YELLOW,  PAL_BALL_CYAN, PAL_BALL_MAGENTA, PAL_BALL_WHITE,
};

#define NUM_PALETTES (sizeof(ball_palettes) / sizeof(ball_palettes[0]))

typedef struct Ball {
    BodyHandle body;
    Actor actor;
    u8 active;
} Ball;

typedef struct BallSystem {
    PhysWorldHandle physics;
    Ball *balls; // Allocated from arena
    u8 max_balls;
    u8 ball_count;
    u16 random_seed;
} BallSystem;

// Simple pseudo-random number generator
static u16 random(BallSystemHandle sys) {
    sys->random_seed = (u16)(sys->random_seed * 25173 + 13849);
    return sys->random_seed;
}

static fixed random_range_fix(BallSystemHandle sys, fixed min, fixed max) {
    u16 r = random(sys);
    fixed range = max - min;
    return min + FIX_MUL(range, FIX(r) / 65535);
}

static s16 random_range(BallSystemHandle sys, s16 min, s16 max) {
    return (s16)(min + (random(sys) % (max - min + 1)));
}

// Collision callback - shake camera and play sound when balls collide
static void on_ball_collision(Collision *collision, void *user_data) {
    (void)user_data;

    Ball *ball = (Ball *)PhysBodyGetUserData(collision->body_a);
    if (ball && ball->actor) {
        AudioPlaySfxAt(ball->actor, SFX_BALL_HIT);
    }

    // Boundary collisions don't trigger this callback, only ball-ball collisions
    CameraShake(2, 4);
}

BallSystemHandle BallSystemCreate(Arena *arena, u8 max_balls) {
    BallSystem *sys = ARENA_ALLOC(arena, BallSystem);
    if (!sys)
        return 0;

    sys->balls = ARENA_ALLOC_ARRAY(arena, Ball, max_balls);
    if (!sys->balls)
        return 0;

    sys->max_balls = max_balls;
    sys->ball_count = 0;
    sys->random_seed = 12345;

    for (u8 i = 0; i < max_balls; i++) {
        sys->balls[i].active = 0;
    }

    sys->physics = PhysWorldCreate();
    const u8 offset = 16;
    PhysWorldSetBounds(sys->physics, FIX(offset), FIX(VisualAsset_brick.width_pixels - offset),
                       FIX(offset), FIX(VisualAsset_brick.height_pixels - offset));
    PhysWorldSetGravity(sys->physics, 0, FIX(1));

    return sys;
}

void BallSystemDestroy(BallSystemHandle sys) {
    if (!sys)
        return;

    for (u8 i = 0; i < sys->max_balls; i++) {
        if (sys->balls[i].active) {
            ActorRemoveFromScene(sys->balls[i].actor);
            ActorDestroy(sys->balls[i].actor);
            PhysBodyDestroy(sys->balls[i].body);
            sys->balls[i].active = 0;
        }
    }

    PhysWorldDestroy(sys->physics);
}

void BallSystemUpdate(BallSystemHandle sys) {
    if (!sys)
        return;

    PhysWorldUpdate(sys->physics, on_ball_collision, 0);

    // Physics uses center position, actor uses top-left
    for (u8 i = 0; i < sys->max_balls; i++) {
        Ball *ball = &sys->balls[i];
        if (!ball->active)
            continue;

        Vec2 pos = PhysBodyGetPos(ball->body);
        fixed x = pos.x - BALL_HALF_SIZE;
        fixed y = pos.y - BALL_HALF_SIZE;
        ActorSetPos(ball->actor, x, y);
    }
}

u8 BallSpawn(BallSystemHandle sys) {
    if (!sys)
        return 0;

    for (u8 i = 0; i < sys->max_balls; i++) {
        if (!sys->balls[i].active) {
            Ball *ball = &sys->balls[i];

            fixed x = random_range_fix(sys, FIX(85), FIX(VisualAsset_brick.width_pixels - 85));
            fixed y = random_range_fix(sys, FIX(85), FIX(VisualAsset_brick.height_pixels - 85));

            fixed vx = FIX(random_range(sys, -3, 3));
            fixed vy = FIX(random_range(sys, -3, 3));
            if (vx == 0)
                vx = FIX(1);
            if (vy == 0)
                vy = FIX(1);

            ball->body = PhysBodyCreateAABB(sys->physics, x, y, BALL_HALF_SIZE, BALL_HALF_SIZE);
            PhysBodySetVel(ball->body, vx, vy);
            PhysBodySetRestitution(ball->body, FIX_FROM_FLOAT(1));
            PhysBodySetUserData(ball->body, ball);

            ball->actor = ActorCreate(&VisualAsset_ball);
            ActorAddToScene(ball->actor, x - BALL_HALF_SIZE, y - BALL_HALF_SIZE, 100);
            ActorSetPalette(ball->actor, ball_palettes[i % NUM_PALETTES]);
            ActorPlayAnim(ball->actor, "spin");
            ball->active = 1;
            sys->ball_count++;

            return 1;
        }
    }
    return 0; // Pool full
}

u8 BallDestroyLast(BallSystemHandle sys) {
    if (!sys)
        return 0;

    for (s8 i = (s8)(sys->max_balls - 1); i >= 0; i--) {
        if (sys->balls[i].active) {
            ActorDestroy(sys->balls[i].actor);
            PhysBodyDestroy(sys->balls[i].body);
            sys->balls[i].active = 0;
            sys->ball_count--;
            return 1;
        }
    }
    return 0; // No balls to destroy
}

u8 BallCount(BallSystemHandle sys) {
    if (!sys)
        return 0;
    return sys->ball_count;
}

void BallSystemSetGravity(BallSystemHandle sys, fixed gravity_y) {
    if (!sys)
        return;
    PhysWorldSetGravity(sys->physics, 0, gravity_y);
}
