/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "ball.h"
#include <actor.h>
#include <ng_audio.h>
#include <camera.h>
#include <ng_palette.h>
#include <physics.h>
#include <progear_assets.h>

#define BALL_HALF_SIZE FIX(16) // 16 pixels (32x32 sprite)

static const u8 ball_palettes[] = {
    NGPAL_BALL_DEFAULT, NGPAL_BALL_RED,  NGPAL_BALL_GREEN,   NGPAL_BALL_BLUE,
    NGPAL_BALL_YELLOW,  NGPAL_BALL_CYAN, NGPAL_BALL_MAGENTA, NGPAL_BALL_WHITE,
};

#define NUM_PALETTES (sizeof(ball_palettes) / sizeof(ball_palettes[0]))

typedef struct Ball {
    NGBodyHandle body;
    NGActorHandle actor;
    u8 active;
} Ball;

typedef struct BallSystem {
    NGPhysWorldHandle physics;
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
static void on_ball_collision(NGCollision *collision, void *user_data) {
    (void)user_data;

    Ball *ball = (Ball *)NGPhysBodyGetUserData(collision->body_a);
    if (ball && ball->actor) {
        NGActorPlaySfx(ball->actor, NGSFX_BALL_HIT);
    }

    // Boundary collisions don't trigger this callback, only ball-ball collisions
    NGCameraShake(2, 4);
}

BallSystemHandle BallSystemCreate(NGArena *arena, u8 max_balls) {
    BallSystem *sys = NG_ARENA_ALLOC(arena, BallSystem);
    if (!sys)
        return 0;

    sys->balls = NG_ARENA_ALLOC_ARRAY(arena, Ball, max_balls);
    if (!sys->balls)
        return 0;

    sys->max_balls = max_balls;
    sys->ball_count = 0;
    sys->random_seed = 12345;

    for (u8 i = 0; i < max_balls; i++) {
        sys->balls[i].active = 0;
    }

    sys->physics = NGPhysWorldCreate();
    const u8 offset = 16;
    NGPhysWorldSetBounds(sys->physics, FIX(offset), FIX(NGVisualAsset_brick.width_pixels - offset),
                         FIX(offset), FIX(NGVisualAsset_brick.height_pixels - offset));
    NGPhysWorldSetGravity(sys->physics, 0, FIX(1));

    return sys;
}

void BallSystemDestroy(BallSystemHandle sys) {
    if (!sys)
        return;

    for (u8 i = 0; i < sys->max_balls; i++) {
        if (sys->balls[i].active) {
            NGActorRemoveFromScene(sys->balls[i].actor);
            NGActorDestroy(sys->balls[i].actor);
            NGPhysBodyDestroy(sys->balls[i].body);
            sys->balls[i].active = 0;
        }
    }

    NGPhysWorldDestroy(sys->physics);
}

void BallSystemUpdate(BallSystemHandle sys) {
    if (!sys)
        return;

    NGPhysWorldUpdate(sys->physics, on_ball_collision, 0);

    // Physics uses center position, actor uses top-left
    for (u8 i = 0; i < sys->max_balls; i++) {
        Ball *ball = &sys->balls[i];
        if (!ball->active)
            continue;

        NGVec2 pos = NGPhysBodyGetPos(ball->body);
        fixed x = pos.x - BALL_HALF_SIZE;
        fixed y = pos.y - BALL_HALF_SIZE;
        NGActorSetPos(ball->actor, x, y);
    }
}

u8 BallSpawn(BallSystemHandle sys) {
    if (!sys)
        return 0;

    for (u8 i = 0; i < sys->max_balls; i++) {
        if (!sys->balls[i].active) {
            Ball *ball = &sys->balls[i];

            fixed x = random_range_fix(sys, FIX(85), FIX(NGVisualAsset_brick.width_pixels - 85));
            fixed y = random_range_fix(sys, FIX(85), FIX(NGVisualAsset_brick.height_pixels - 85));

            fixed vx = FIX(random_range(sys, -3, 3));
            fixed vy = FIX(random_range(sys, -3, 3));
            if (vx == 0)
                vx = FIX(1);
            if (vy == 0)
                vy = FIX(1);

            ball->body = NGPhysBodyCreateAABB(sys->physics, x, y, BALL_HALF_SIZE, BALL_HALF_SIZE);
            NGPhysBodySetVel(ball->body, vx, vy);
            NGPhysBodySetRestitution(ball->body, FIX(1));
            NGPhysBodySetUserData(ball->body, ball);

            ball->actor = NGActorCreate(&NGVisualAsset_ball, 0, 0);
            NGActorAddToScene(ball->actor, x - BALL_HALF_SIZE, y - BALL_HALF_SIZE, 100);
            NGActorSetPalette(ball->actor, ball_palettes[i % NUM_PALETTES]);
            NGActorSetAnimByName(ball->actor, "spin");
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
            NGActorDestroy(sys->balls[i].actor);
            NGPhysBodyDestroy(sys->balls[i].body);
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
    NGPhysWorldSetGravity(sys->physics, 0, gravity_y);
}
