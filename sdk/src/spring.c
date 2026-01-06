/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <spring.h>

#define SETTLE_VELOCITY_THRESHOLD  FIX_FROM_FLOAT(0.1)
#define SETTLE_POSITION_THRESHOLD  FIX_FROM_FLOAT(0.5)

void NGSpringInit(NGSpring *spring, fixed initial) {
    spring->value = initial;
    spring->velocity = 0;
    spring->target = initial;
    spring->stiffness = NG_SPRING_SNAPPY_STIFFNESS;
    spring->damping = NG_SPRING_SNAPPY_DAMPING;
}

void NGSpringInitEx(NGSpring *spring, fixed initial, fixed stiffness, fixed damping) {
    spring->value = initial;
    spring->velocity = 0;
    spring->target = initial;
    spring->stiffness = stiffness;
    spring->damping = damping;
}

void NGSpringSetTarget(NGSpring *spring, fixed target) {
    spring->target = target;
}

void NGSpringSnap(NGSpring *spring, fixed value) {
    spring->value = value;
    spring->target = value;
    spring->velocity = 0;
}

void NGSpringImpulse(NGSpring *spring, fixed impulse) {
    spring->velocity += impulse;
}

void NGSpringUpdate(NGSpring *spring) {
    // F = -k * displacement - d * velocity
    fixed displacement = spring->value - spring->target;
    fixed spring_force = -FIX_MUL(spring->stiffness, displacement);
    fixed damping_force = -FIX_MUL(spring->damping, spring->velocity);
    fixed acceleration = spring_force + damping_force;

    spring->velocity += acceleration;
    spring->value += spring->velocity;
}

u8 NGSpringSettled(NGSpring *spring) {
    fixed displacement = spring->value - spring->target;
    if (displacement < 0) displacement = -displacement;

    fixed vel = spring->velocity;
    if (vel < 0) vel = -vel;

    return (displacement < SETTLE_POSITION_THRESHOLD) &&
           (vel < SETTLE_VELOCITY_THRESHOLD);
}

void NGSpring2DInit(NGSpring2D *spring, fixed x, fixed y) {
    NGSpringInit(&spring->x, x);
    NGSpringInit(&spring->y, y);
}

void NGSpring2DInitEx(NGSpring2D *spring, fixed x, fixed y, fixed stiffness, fixed damping) {
    NGSpringInitEx(&spring->x, x, stiffness, damping);
    NGSpringInitEx(&spring->y, y, stiffness, damping);
}

void NGSpring2DSetTarget(NGSpring2D *spring, fixed x, fixed y) {
    spring->x.target = x;
    spring->y.target = y;
}

void NGSpring2DSnap(NGSpring2D *spring, fixed x, fixed y) {
    NGSpringSnap(&spring->x, x);
    NGSpringSnap(&spring->y, y);
}

void NGSpring2DUpdate(NGSpring2D *spring) {
    NGSpringUpdate(&spring->x);
    NGSpringUpdate(&spring->y);
}

u8 NGSpring2DSettled(NGSpring2D *spring) {
    return NGSpringSettled(&spring->x) && NGSpringSettled(&spring->y);
}
