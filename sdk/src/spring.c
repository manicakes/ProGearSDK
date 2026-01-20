/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <spring.h>

#define SETTLE_VELOCITY_THRESHOLD FIX_FROM_FLOAT(0.1)
#define SETTLE_POSITION_THRESHOLD FIX_FROM_FLOAT(0.5)

void SpringInit(Spring *spring, fixed initial) {
    spring->value = initial;
    spring->velocity = 0;
    spring->target = initial;
    spring->stiffness = SPRING_SNAPPY_STIFFNESS;
    spring->damping = SPRING_SNAPPY_DAMPING;
}

void SpringInitEx(Spring *spring, fixed initial, fixed stiffness, fixed damping) {
    spring->value = initial;
    spring->velocity = 0;
    spring->target = initial;
    spring->stiffness = stiffness;
    spring->damping = damping;
}

void SpringSetTarget(Spring *spring, fixed target) {
    spring->target = target;
}

void SpringSnap(Spring *spring, fixed value) {
    spring->value = value;
    spring->target = value;
    spring->velocity = 0;
}

void SpringImpulse(Spring *spring, fixed impulse) {
    spring->velocity += impulse;
}

void SpringUpdate(Spring *spring) {
    /* F = -k * displacement - d * velocity */
    fixed displacement = spring->value - spring->target;
    fixed spring_force = -FIX_MUL(spring->stiffness, displacement);
    fixed damping_force = -FIX_MUL(spring->damping, spring->velocity);
    fixed acceleration = spring_force + damping_force;

    spring->velocity += acceleration;
    spring->value += spring->velocity;
}

u8 SpringSettled(Spring *spring) {
    fixed displacement = spring->value - spring->target;
    if (displacement < 0)
        displacement = -displacement;

    fixed vel = spring->velocity;
    if (vel < 0)
        vel = -vel;

    return (displacement < SETTLE_POSITION_THRESHOLD) && (vel < SETTLE_VELOCITY_THRESHOLD);
}

void Spring2DInit(Spring2D *spring, fixed x, fixed y) {
    SpringInit(&spring->x, x);
    SpringInit(&spring->y, y);
}

void Spring2DInitEx(Spring2D *spring, fixed x, fixed y, fixed stiffness, fixed damping) {
    SpringInitEx(&spring->x, x, stiffness, damping);
    SpringInitEx(&spring->y, y, stiffness, damping);
}

void Spring2DSetTarget(Spring2D *spring, fixed x, fixed y) {
    spring->x.target = x;
    spring->y.target = y;
}

void Spring2DSnap(Spring2D *spring, fixed x, fixed y) {
    SpringSnap(&spring->x, x);
    SpringSnap(&spring->y, y);
}

void Spring2DUpdate(Spring2D *spring) {
    SpringUpdate(&spring->x);
    SpringUpdate(&spring->y);
}

u8 Spring2DSettled(Spring2D *spring) {
    return SpringSettled(&spring->x) && SpringSettled(&spring->y);
}
