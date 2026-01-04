/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// physics.c - Simple 2D physics engine implementation

#include <physics.h>

#define MAX_WORLDS 1

// === Axis-Aligned Normal Optimization ===
// AABB collisions always produce axis-aligned normals (±1,0) or (0,±1)
// This avoids expensive FIX_MUL when multiplying by these normals

static inline fixed mul_by_normal_component(fixed value, fixed normal_comp) {
    // Fast path for axis-aligned normals (very common with AABB)
    if (normal_comp == FIX_ONE) return value;
    if (normal_comp == -FIX_ONE) return -value;
    if (normal_comp == 0) return 0;
    // General case (circle collisions, etc.)
    return FIX_MUL(value, normal_comp);
}

// Apply scalar to 2D normal vector, returns result in out_x/out_y
static inline void scale_normal(fixed scalar, NGVec2 *normal, fixed *out_x, fixed *out_y) {
    *out_x = mul_by_normal_component(scalar, normal->x);
    *out_y = mul_by_normal_component(scalar, normal->y);
}

// Dot product with potentially axis-aligned normal
static inline fixed dot_with_normal(fixed vx, fixed vy, NGVec2 *normal) {
    return mul_by_normal_component(vx, normal->x) +
           mul_by_normal_component(vy, normal->y);
}

static NGPhysWorld world_pool[MAX_WORLDS];

// === World Management ===

NGPhysWorldHandle NGPhysWorldCreate(void) {
    for (int i = 0; i < MAX_WORLDS; i++) {
        if (!world_pool[i].active) {
            NGPhysWorld *world = &world_pool[i];
            world->active = 1;
            world->gravity.x = 0;
            world->gravity.y = 0;
            world->bounds_enabled = 0;

            // Clear body pool
            for (int j = 0; j < NG_PHYS_MAX_BODIES; j++) {
                world->bodies[j].active = 0;
            }

            return world;
        }
    }
    return 0;
}

void NGPhysWorldDestroy(NGPhysWorldHandle world) {
    if (world) {
        world->active = 0;
    }
}

void NGPhysWorldSetGravity(NGPhysWorldHandle world, fixed gx, fixed gy) {
    if (!world) return;
    world->gravity.x = gx;
    world->gravity.y = gy;
}

void NGPhysWorldSetBounds(NGPhysWorldHandle world,
                          fixed left, fixed right,
                          fixed top, fixed bottom) {
    if (!world) return;
    world->bounds_left = left;
    world->bounds_right = right;
    world->bounds_top = top;
    world->bounds_bottom = bottom;
    world->bounds_enabled = 1;
}

void NGPhysWorldDisableBounds(NGPhysWorldHandle world) {
    if (!world) return;
    world->bounds_enabled = 0;
}

void NGPhysWorldReset(NGPhysWorldHandle world) {
    if (!world) return;

    // Destroy all bodies but keep world settings
    for (int i = 0; i < NG_PHYS_MAX_BODIES; i++) {
        world->bodies[i].active = 0;
    }
}

// === Collision Detection ===

static u8 test_circle_circle(NGBody *a, NGBody *b, NGCollision *out) {
    NGVec2 delta = NGVec2Sub(b->pos, a->pos);
    fixed dist_sq = NGVec2LengthSq(delta);
    fixed radii = a->shape.circle.radius + b->shape.circle.radius;
    fixed radii_sq = FIX_MUL(radii, radii);

    if (dist_sq >= radii_sq) return 0;  // No collision

    fixed dist = NGSqrtFix(dist_sq);
    if (dist == 0) {
        // Bodies at same position - push apart arbitrarily
        out->normal = (NGVec2){ FIX_ONE, 0 };
        out->penetration = radii;
    } else {
        out->normal = (NGVec2){ FIX_DIV(delta.x, dist), FIX_DIV(delta.y, dist) };
        out->penetration = radii - dist;
    }

    out->body_a = a;
    out->body_b = b;
    out->contact_point = (NGVec2){
        a->pos.x + FIX_MUL(out->normal.x, a->shape.circle.radius),
        a->pos.y + FIX_MUL(out->normal.y, a->shape.circle.radius)
    };

    return 1;
}

static u8 test_aabb_aabb(NGBody *a, NGBody *b, NGCollision *out) {
    fixed dx = b->pos.x - a->pos.x;
    fixed dy = b->pos.y - a->pos.y;

    fixed overlap_x = a->shape.aabb.half_width + b->shape.aabb.half_width - FIX_ABS(dx);
    if (overlap_x <= 0) return 0;

    fixed overlap_y = a->shape.aabb.half_height + b->shape.aabb.half_height - FIX_ABS(dy);
    if (overlap_y <= 0) return 0;

    out->body_a = a;
    out->body_b = b;

    // Use smallest overlap axis
    if (overlap_x < overlap_y) {
        out->penetration = overlap_x;
        if (dx > 0) {
            out->normal = (NGVec2){ FIX_ONE, 0 };
        } else {
            out->normal = (NGVec2){ -FIX_ONE, 0 };
        }
    } else {
        out->penetration = overlap_y;
        if (dy > 0) {
            out->normal = (NGVec2){ 0, FIX_ONE };
        } else {
            out->normal = (NGVec2){ 0, -FIX_ONE };
        }
    }

    out->contact_point = (NGVec2){
        (a->pos.x + b->pos.x) / 2,
        (a->pos.y + b->pos.y) / 2
    };

    return 1;
}

u8 NGPhysTestCollision(NGBodyHandle a, NGBodyHandle b, NGCollision *out) {
    if (!a || !b) return 0;
    if (!a->active || !b->active) return 0;

    // Check collision layer/mask
    if (!(a->collision_mask & b->collision_layer) &&
        !(b->collision_mask & a->collision_layer)) {
        return 0;
    }

    // Dispatch based on shape types
    if (a->shape.type == NG_SHAPE_CIRCLE && b->shape.type == NG_SHAPE_CIRCLE) {
        return test_circle_circle(a, b, out);
    }
    if (a->shape.type == NG_SHAPE_AABB && b->shape.type == NG_SHAPE_AABB) {
        return test_aabb_aabb(a, b, out);
    }

    // TODO: Circle-AABB collision
    return 0;
}

// === Collision Response ===

static void resolve_collision(NGCollision *col) {
    NGBody *a = col->body_a;
    NGBody *b = col->body_b;

    // Skip if both static or triggers
    u8 a_movable = !(a->flags & (NG_BODY_STATIC | NG_BODY_TRIGGER));
    u8 b_movable = !(b->flags & (NG_BODY_STATIC | NG_BODY_TRIGGER));

    if (!a_movable && !b_movable) return;
    if ((a->flags & NG_BODY_TRIGGER) || (b->flags & NG_BODY_TRIGGER)) return;

    // Fast path: equal mass, both movable (any restitution)
    // Avoids all FIX_DIV calls - very common case (enemies, projectiles, etc.)
    if (a_movable && b_movable && a->mass == b->mass) {
        // Positional correction: each body moves half (no division needed)
        fixed slop = FIX(1) / 16;
        fixed correction = NG_MAX(col->penetration - slop, 0);
        fixed half_corr = correction >> 1;  // Fast divide by 2

        fixed corr_x, corr_y;
        scale_normal(half_corr, &col->normal, &corr_x, &corr_y);
        a->pos.x -= corr_x;
        a->pos.y -= corr_y;
        b->pos.x += corr_x;
        b->pos.y += corr_y;

        // Relative velocity along normal (optimized for axis-aligned)
        fixed rel_vx = b->vel.x - a->vel.x;
        fixed rel_vy = b->vel.y - a->vel.y;
        fixed vel_along_normal = dot_with_normal(rel_vx, rel_vy, &col->normal);

        if (vel_along_normal >= 0) return;  // Separating

        // For equal mass: j = -(1+e) * v_rel / (inv_m + inv_m) = -(1+e) * v_rel / 2
        // Each body gets same impulse magnitude (just opposite direction)
        fixed e = NG_MIN(a->restitution, b->restitution);
        fixed j = FIX_MUL(-(FIX_ONE + e), vel_along_normal) >> 1;  // Divide by 2

        // Scale by inv_mass and apply to normal
        fixed scaled_j = FIX_MUL(j, a->inv_mass);
        fixed impulse_x, impulse_y;
        scale_normal(scaled_j, &col->normal, &impulse_x, &impulse_y);

        a->vel.x -= impulse_x;
        a->vel.y -= impulse_y;
        b->vel.x += impulse_x;
        b->vel.y += impulse_y;
        return;
    }

    // General case (different masses - requires division)
    fixed total_mass = a->mass + b->mass;
    if (total_mass == 0) total_mass = FIX_ONE;

    fixed a_ratio = a_movable ? FIX_DIV(b->mass, total_mass) : 0;
    fixed b_ratio = b_movable ? FIX_DIV(a->mass, total_mass) : 0;

    if (!a_movable) b_ratio = FIX_ONE;
    if (!b_movable) a_ratio = FIX_ONE;

    // Positional correction (optimized for axis-aligned normals)
    fixed slop = FIX(1) / 16;
    fixed correction = NG_MAX(col->penetration - slop, 0);

    if (a_movable) {
        fixed a_corr = FIX_MUL(correction, a_ratio);
        fixed cx, cy;
        scale_normal(a_corr, &col->normal, &cx, &cy);
        a->pos.x -= cx;
        a->pos.y -= cy;
    }
    if (b_movable) {
        fixed b_corr = FIX_MUL(correction, b_ratio);
        fixed cx, cy;
        scale_normal(b_corr, &col->normal, &cx, &cy);
        b->pos.x += cx;
        b->pos.y += cy;
    }

    // Relative velocity along normal (optimized for axis-aligned)
    fixed rel_vx = b->vel.x - a->vel.x;
    fixed rel_vy = b->vel.y - a->vel.y;
    fixed vel_along_normal = dot_with_normal(rel_vx, rel_vy, &col->normal);

    if (vel_along_normal > 0) return;

    fixed e = NG_MIN(a->restitution, b->restitution);
    fixed j = FIX_MUL(-(FIX_ONE + e), vel_along_normal);

    // Use cached inv_mass (no division needed here)
    fixed inv_mass_a = a_movable ? a->inv_mass : 0;
    fixed inv_mass_b = b_movable ? b->inv_mass : 0;
    fixed inv_mass_sum = inv_mass_a + inv_mass_b;

    if (inv_mass_sum > 0) {
        j = FIX_DIV(j, inv_mass_sum);
    }

    // Apply impulse (optimized for axis-aligned normals)
    if (a_movable) {
        fixed imp_a = FIX_MUL(inv_mass_a, j);
        fixed ix, iy;
        scale_normal(imp_a, &col->normal, &ix, &iy);
        a->vel.x -= ix;
        a->vel.y -= iy;
    }
    if (b_movable) {
        fixed imp_b = FIX_MUL(inv_mass_b, j);
        fixed ix, iy;
        scale_normal(imp_b, &col->normal, &ix, &iy);
        b->vel.x += ix;
        b->vel.y += iy;
    }
}

// === Bounds Collision ===

static void handle_bounds(NGPhysWorld *world, NGBody *body) {
    if (!world->bounds_enabled) return;
    if (body->flags & NG_BODY_STATIC) return;

    fixed left, right, top, bottom;

    if (body->shape.type == NG_SHAPE_CIRCLE) {
        left = body->pos.x - body->shape.circle.radius;
        right = body->pos.x + body->shape.circle.radius;
        top = body->pos.y - body->shape.circle.radius;
        bottom = body->pos.y + body->shape.circle.radius;
    } else {
        left = body->pos.x - body->shape.aabb.half_width;
        right = body->pos.x + body->shape.aabb.half_width;
        top = body->pos.y - body->shape.aabb.half_height;
        bottom = body->pos.y + body->shape.aabb.half_height;
    }

    // Left edge
    if (left < world->bounds_left) {
        body->pos.x += world->bounds_left - left;
        if (body->vel.x < 0) {
            // Fast path: perfect bounce (restitution = 1.0) is just negation
            if (body->restitution == FIX_ONE) {
                body->vel.x = -body->vel.x;
            } else {
                body->vel.x = FIX_MUL(-body->vel.x, body->restitution);
            }
        }
    }

    // Right edge
    if (right > world->bounds_right) {
        body->pos.x -= right - world->bounds_right;
        if (body->vel.x > 0) {
            if (body->restitution == FIX_ONE) {
                body->vel.x = -body->vel.x;
            } else {
                body->vel.x = FIX_MUL(-body->vel.x, body->restitution);
            }
        }
    }

    // Top edge
    if (top < world->bounds_top) {
        body->pos.y += world->bounds_top - top;
        if (body->vel.y < 0) {
            if (body->restitution == FIX_ONE) {
                body->vel.y = -body->vel.y;
            } else {
                body->vel.y = FIX_MUL(-body->vel.y, body->restitution);
            }
        }
    }

    // Bottom edge
    if (bottom > world->bounds_bottom) {
        body->pos.y -= bottom - world->bounds_bottom;
        if (body->vel.y > 0) {
            if (body->restitution == FIX_ONE) {
                body->vel.y = -body->vel.y;
            } else {
                body->vel.y = FIX_MUL(-body->vel.y, body->restitution);
            }
        }
    }
}

// === World Update ===

void NGPhysWorldUpdate(NGPhysWorldHandle world,
                       NGCollisionCallback callback,
                       void *callback_data) {
    if (!world) return;

    // Integration: apply acceleration and velocity
    for (int i = 0; i < NG_PHYS_MAX_BODIES; i++) {
        NGBody *body = &world->bodies[i];
        if (!body->active) continue;
        if (body->flags & NG_BODY_STATIC) continue;

        // Apply gravity if enabled
        if (!(body->flags & NG_BODY_NO_GRAVITY)) {
            body->vel.x += world->gravity.x;
            body->vel.y += world->gravity.y;
        }

        // Apply body acceleration
        body->vel.x += body->accel.x;
        body->vel.y += body->accel.y;

        // Update position
        body->pos.x += body->vel.x;
        body->pos.y += body->vel.y;
    }

    // Collision detection and response
    // Skip entirely if no callback and we can detect no collisions are possible
    // (This is an optimization - early-out when masks are all zero)
    u8 any_can_collide = 0;
    for (int i = 0; i < NG_PHYS_MAX_BODIES && !any_can_collide; i++) {
        if (world->bodies[i].active && world->bodies[i].collision_mask) {
            any_can_collide = 1;
        }
    }

    if (any_can_collide || callback) {
        for (int i = 0; i < NG_PHYS_MAX_BODIES; i++) {
            if (!world->bodies[i].active) continue;

            for (int j = i + 1; j < NG_PHYS_MAX_BODIES; j++) {
                if (!world->bodies[j].active) continue;

                NGCollision col;
                if (NGPhysTestCollision(&world->bodies[i], &world->bodies[j], &col)) {
                    resolve_collision(&col);

                    if (callback) {
                        callback(&col, callback_data);
                    }
                }
            }
        }
    }

    // Handle bounds
    for (int i = 0; i < NG_PHYS_MAX_BODIES; i++) {
        if (!world->bodies[i].active) continue;
        handle_bounds(world, &world->bodies[i]);
    }
}

// === Body Management ===

static NGBody *alloc_body(NGPhysWorldHandle world) {
    if (!world) return 0;

    for (int i = 0; i < NG_PHYS_MAX_BODIES; i++) {
        if (!world->bodies[i].active) {
            NGBody *body = &world->bodies[i];
            body->active = 1;
            body->flags = 0;
            body->pos = (NGVec2){ 0, 0 };
            body->vel = (NGVec2){ 0, 0 };
            body->accel = (NGVec2){ 0, 0 };
            body->mass = FIX_ONE;
            body->inv_mass = FIX_ONE;     // 1/1 = 1
            body->restitution = FIX_ONE;  // Perfect bounce by default
            body->friction = 0;
            body->collision_layer = 0x01;
            body->collision_mask = 0xFF;
            body->user_data = 0;
            return body;
        }
    }
    return 0;
}

NGBodyHandle NGPhysBodyCreateCircle(NGPhysWorldHandle world,
                                    fixed x, fixed y,
                                    fixed radius) {
    NGBody *body = alloc_body(world);
    if (!body) return 0;

    body->pos.x = x;
    body->pos.y = y;
    body->shape.type = NG_SHAPE_CIRCLE;
    body->shape.circle.radius = radius;

    return body;
}

NGBodyHandle NGPhysBodyCreateAABB(NGPhysWorldHandle world,
                                  fixed x, fixed y,
                                  fixed half_width, fixed half_height) {
    NGBody *body = alloc_body(world);
    if (!body) return 0;

    body->pos.x = x;
    body->pos.y = y;
    body->shape.type = NG_SHAPE_AABB;
    body->shape.aabb.half_width = half_width;
    body->shape.aabb.half_height = half_height;

    return body;
}

void NGPhysBodyDestroy(NGBodyHandle body) {
    if (body) {
        body->active = 0;
    }
}

// === Body Properties ===

void NGPhysBodySetPos(NGBodyHandle body, fixed x, fixed y) {
    if (!body) return;
    body->pos.x = x;
    body->pos.y = y;
}

NGVec2 NGPhysBodyGetPos(NGBodyHandle body) {
    if (!body) return (NGVec2){ 0, 0 };
    return body->pos;
}

void NGPhysBodySetVel(NGBodyHandle body, fixed vx, fixed vy) {
    if (!body) return;
    body->vel.x = vx;
    body->vel.y = vy;
}

NGVec2 NGPhysBodyGetVel(NGBodyHandle body) {
    if (!body) return (NGVec2){ 0, 0 };
    return body->vel;
}

void NGPhysBodySetAccel(NGBodyHandle body, fixed ax, fixed ay) {
    if (!body) return;
    body->accel.x = ax;
    body->accel.y = ay;
}

void NGPhysBodySetMass(NGBodyHandle body, fixed mass) {
    if (!body) return;
    body->mass = mass;
    // Cache inverse mass (computed once, used many times in collision response)
    body->inv_mass = (mass > 0) ? FIX_DIV(FIX_ONE, mass) : 0;
}

void NGPhysBodySetRestitution(NGBodyHandle body, fixed restitution) {
    if (!body) return;
    body->restitution = restitution;
}

void NGPhysBodySetFriction(NGBodyHandle body, fixed friction) {
    if (!body) return;
    body->friction = friction;
}

void NGPhysBodySetFlags(NGBodyHandle body, u8 flags) {
    if (!body) return;
    body->flags = flags;
}

void NGPhysBodySetStatic(NGBodyHandle body, u8 is_static) {
    if (!body) return;
    if (is_static) {
        body->flags |= NG_BODY_STATIC;
    } else {
        body->flags &= ~NG_BODY_STATIC;
    }
}

void NGPhysBodySetLayer(NGBodyHandle body, u8 layer, u8 mask) {
    if (!body) return;
    body->collision_layer = layer;
    body->collision_mask = mask;
}

void NGPhysBodySetUserData(NGBodyHandle body, void *data) {
    if (!body) return;
    body->user_data = data;
}

void *NGPhysBodyGetUserData(NGBodyHandle body) {
    if (!body) return 0;
    return body->user_data;
}

void NGPhysBodyApplyImpulse(NGBodyHandle body, fixed ix, fixed iy) {
    if (!body) return;
    if (body->flags & NG_BODY_STATIC) return;

    // Use cached inv_mass (no division needed)
    body->vel.x += FIX_MUL(ix, body->inv_mass);
    body->vel.y += FIX_MUL(iy, body->inv_mass);
}
