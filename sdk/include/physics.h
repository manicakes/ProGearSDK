/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file physics.h
 * @brief Simple 2D physics engine.
 *
 * Provides basic physics simulation using fixed-point math:
 * - Rigid bodies with position, velocity, acceleration
 * - Circle and AABB collision shapes
 * - Collision detection and response
 * - Automatic screen bounds handling
 *
 * @section physusage Usage
 * 1. Create a world with NGPhysWorldCreate()
 * 2. Set bounds with NGPhysWorldSetBounds()
 * 3. Create bodies with NGPhysBodyCreateCircle() or NGPhysBodyCreateAABB()
 * 4. Call NGPhysWorldUpdate() each frame
 */

#ifndef _PHYSICS_H_
#define _PHYSICS_H_

#include <types.h>
#include <ngmath.h>

/** @defgroup physconfig Configuration
 *  @{
 */

#define NG_PHYS_MAX_BODIES 32  /**< Maximum bodies per world */

/** @} */

/** @defgroup physshape Collision Shapes
 *  @{
 */

/** Shape type enumeration */
typedef enum {
    NG_SHAPE_CIRCLE,  /**< Circle shape */
    NG_SHAPE_AABB,    /**< Axis-aligned bounding box */
} NGShapeType;

/** Collision shape */
typedef struct {
    NGShapeType type;  /**< Shape type */
    union {
        struct {
            fixed radius;  /**< Circle radius */
        } circle;
        struct {
            fixed half_width;   /**< AABB half-width */
            fixed half_height;  /**< AABB half-height */
        } aabb;
    };
} NGShape;

/** @} */

/** @defgroup physbody Physics Body
 *  @{
 */

/** Physics body */
typedef struct NGBody {
    u8 active;            /**< Body is active */
    u8 flags;             /**< Body flags */

    NGVec2 pos;           /**< Center position */
    NGVec2 vel;           /**< Velocity */
    NGVec2 accel;         /**< Acceleration */

    fixed mass;           /**< Mass (FIX_ONE = 1.0) */
    fixed inv_mass;       /**< Cached 1/mass */
    fixed restitution;    /**< Bounciness (FIX_ONE = perfect) */
    fixed friction;       /**< Friction coefficient */

    NGShape shape;        /**< Collision shape */
    u8 collision_mask;    /**< Layers this body collides with */
    u8 collision_layer;   /**< Layer this body is on */

    void *user_data;      /**< User-defined data */
} NGBody;

/** Body does not move */
#define NG_BODY_STATIC     0x01

/** Body ignores world gravity */
#define NG_BODY_NO_GRAVITY 0x02

/** Body detects collision but doesn't respond */
#define NG_BODY_TRIGGER    0x04

/** Body handle */
typedef NGBody *NGBodyHandle;

/** @} */

/** @defgroup physworld Physics World
 *  @{
 */

/** Physics world */
typedef struct NGPhysWorld {
    u8 active;            /**< World is active */
    NGVec2 gravity;       /**< World gravity */

    fixed bounds_left;    /**< Left boundary */
    fixed bounds_right;   /**< Right boundary */
    fixed bounds_top;     /**< Top boundary */
    fixed bounds_bottom;  /**< Bottom boundary */
    u8 bounds_enabled;    /**< Bounds checking enabled */

    NGBody bodies[NG_PHYS_MAX_BODIES];  /**< Body pool */
} NGPhysWorld;

/** World handle */
typedef NGPhysWorld *NGPhysWorldHandle;

/** @} */

/** @defgroup physcoll Collision Info
 *  @{
 */

/** Collision information */
typedef struct {
    NGBodyHandle body_a;     /**< First body */
    NGBodyHandle body_b;     /**< Second body */
    NGVec2 normal;           /**< Collision normal (A to B) */
    fixed penetration;       /**< Overlap depth */
    NGVec2 contact_point;    /**< Point of contact */
} NGCollision;

/**
 * Collision callback type.
 * @param collision Collision info
 * @param user_data User-provided data
 */
typedef void (*NGCollisionCallback)(NGCollision *collision, void *user_data);

/** @} */

/** @defgroup physworldfn World Management
 *  @{
 */

/**
 * Create a physics world.
 * @return World handle, or NULL if failed
 */
NGPhysWorldHandle NGPhysWorldCreate(void);

/**
 * Destroy a physics world.
 * @param world World handle
 */
void NGPhysWorldDestroy(NGPhysWorldHandle world);

/**
 * Set world gravity.
 * @param world World handle
 * @param gx X gravity (fixed)
 * @param gy Y gravity (fixed)
 */
void NGPhysWorldSetGravity(NGPhysWorldHandle world, fixed gx, fixed gy);

/**
 * Set screen bounds for edge collision.
 * @param world World handle
 * @param left Left edge (fixed)
 * @param right Right edge (fixed)
 * @param top Top edge (fixed)
 * @param bottom Bottom edge (fixed)
 */
void NGPhysWorldSetBounds(NGPhysWorldHandle world,
                          fixed left, fixed right,
                          fixed top, fixed bottom);

/**
 * Disable bounds checking.
 * @param world World handle
 */
void NGPhysWorldDisableBounds(NGPhysWorldHandle world);

/**
 * Reset world to empty state.
 * Destroys all bodies but keeps world settings (gravity, bounds).
 * Call when transitioning between levels/screens.
 * @param world World handle
 */
void NGPhysWorldReset(NGPhysWorldHandle world);

/**
 * Update physics simulation.
 * Call once per frame.
 * @param world World handle
 * @param callback Collision callback (can be NULL)
 * @param callback_data User data for callback
 */
void NGPhysWorldUpdate(NGPhysWorldHandle world,
                       NGCollisionCallback callback,
                       void *callback_data);

/** @} */

/** @defgroup physbodyfn Body Management
 *  @{
 */

/**
 * Create a circle body.
 * @param world World handle
 * @param x Initial X position (fixed)
 * @param y Initial Y position (fixed)
 * @param radius Circle radius (fixed)
 * @return Body handle, or NULL if failed
 */
NGBodyHandle NGPhysBodyCreateCircle(NGPhysWorldHandle world,
                                    fixed x, fixed y,
                                    fixed radius);

/**
 * Create an AABB body.
 * @param world World handle
 * @param x Initial X position (fixed)
 * @param y Initial Y position (fixed)
 * @param half_width Half-width (fixed)
 * @param half_height Half-height (fixed)
 * @return Body handle, or NULL if failed
 */
NGBodyHandle NGPhysBodyCreateAABB(NGPhysWorldHandle world,
                                  fixed x, fixed y,
                                  fixed half_width, fixed half_height);

/**
 * Destroy a body.
 * @param body Body handle
 */
void NGPhysBodyDestroy(NGBodyHandle body);

/** @} */

/** @defgroup physprop Body Properties
 *  @{
 */

/**
 * Set body position.
 * @param body Body handle
 * @param x X position (fixed)
 * @param y Y position (fixed)
 */
void NGPhysBodySetPos(NGBodyHandle body, fixed x, fixed y);

/**
 * Get body position.
 * @param body Body handle
 * @return Position vector
 */
NGVec2 NGPhysBodyGetPos(NGBodyHandle body);

/**
 * Set body velocity.
 * @param body Body handle
 * @param vx X velocity (fixed)
 * @param vy Y velocity (fixed)
 */
void NGPhysBodySetVel(NGBodyHandle body, fixed vx, fixed vy);

/**
 * Get body velocity.
 * @param body Body handle
 * @return Velocity vector
 */
NGVec2 NGPhysBodyGetVel(NGBodyHandle body);

/**
 * Set body acceleration.
 * @param body Body handle
 * @param ax X acceleration (fixed)
 * @param ay Y acceleration (fixed)
 */
void NGPhysBodySetAccel(NGBodyHandle body, fixed ax, fixed ay);

/**
 * Set body mass.
 * @param body Body handle
 * @param mass Mass value (fixed, FIX_ONE = 1.0)
 */
void NGPhysBodySetMass(NGBodyHandle body, fixed mass);

/**
 * Set body restitution (bounciness).
 * @param body Body handle
 * @param restitution Restitution (0 to FIX_ONE)
 */
void NGPhysBodySetRestitution(NGBodyHandle body, fixed restitution);

/**
 * Set body friction.
 * @param body Body handle
 * @param friction Friction coefficient (fixed)
 */
void NGPhysBodySetFriction(NGBodyHandle body, fixed friction);

/**
 * Set body flags.
 * @param body Body handle
 * @param flags Flag bitmask
 */
void NGPhysBodySetFlags(NGBodyHandle body, u8 flags);

/**
 * Set body static state.
 * @param body Body handle
 * @param is_static 1 for static, 0 for dynamic
 */
void NGPhysBodySetStatic(NGBodyHandle body, u8 is_static);

/**
 * Set collision layer and mask.
 * @param body Body handle
 * @param layer Layer this body is on
 * @param mask Layers this body collides with
 */
void NGPhysBodySetLayer(NGBodyHandle body, u8 layer, u8 mask);

/**
 * Set user data pointer.
 * @param body Body handle
 * @param data User data pointer
 */
void NGPhysBodySetUserData(NGBodyHandle body, void *data);

/**
 * Get user data pointer.
 * @param body Body handle
 * @return User data pointer
 */
void *NGPhysBodyGetUserData(NGBodyHandle body);

/** @} */

/** @defgroup physutil Physics Utilities
 *  @{
 */

/**
 * Apply impulse to body.
 * @param body Body handle
 * @param ix X impulse (fixed)
 * @param iy Y impulse (fixed)
 */
void NGPhysBodyApplyImpulse(NGBodyHandle body, fixed ix, fixed iy);

/**
 * Test if two bodies are colliding.
 * @param a First body handle
 * @param b Second body handle
 * @param[out] out Collision info (can be NULL)
 * @return 1 if colliding, 0 otherwise
 */
u8 NGPhysTestCollision(NGBodyHandle a, NGBodyHandle b, NGCollision *out);

/** @} */

#endif // _PHYSICS_H_
