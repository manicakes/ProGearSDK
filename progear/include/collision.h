/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file collision.h
 * @brief Shared collision and tile property constants.
 *
 * These flags are used by both the scene and terrain systems for
 * tile-based collision detection and resolution.
 */

#ifndef NG_COLLISION_H
#define NG_COLLISION_H

/**
 * @defgroup collision Collision Constants
 * @ingroup sdk
 * @brief Tile collision flags and resolution direction flags.
 * @{
 */

/** @name Collision Direction Flags
 * Returned by NGSceneResolveCollision() and NGTerrainResolveAABB()
 * to indicate which sides of an AABB collided with terrain.
 */
/** @{ */

#define NG_COLL_NONE   0x00 /**< No collision */
#define NG_COLL_LEFT   0x01 /**< Hit terrain on left */
#define NG_COLL_RIGHT  0x02 /**< Hit terrain on right */
#define NG_COLL_TOP    0x04 /**< Hit terrain above */
#define NG_COLL_BOTTOM 0x08 /**< Hit terrain below (landed) */
/** @} */

/** @name Tile Property Flags
 * Stored per-tile in collision data. Queried via NGSceneGetCollisionAt()
 * and NGTerrainGetCollision().
 */
/** @{ */

#define NG_TILE_SOLID    0x01 /**< Blocks movement (walls/floors) */
#define NG_TILE_PLATFORM 0x02 /**< One-way platform (solid from above) */
#define NG_TILE_SLOPE_L  0x04 /**< Left-to-right slope */
#define NG_TILE_SLOPE_R  0x08 /**< Right-to-left slope */
#define NG_TILE_HAZARD   0x10 /**< Damages player on contact */
#define NG_TILE_TRIGGER  0x20 /**< Triggers callback on contact */
#define NG_TILE_LADDER   0x40 /**< Climbable tile */
/** @} */

/** @} */ /* end of collision group */

#endif /* NG_COLLISION_H */
