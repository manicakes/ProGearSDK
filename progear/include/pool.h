/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file pool.h
 * @brief Fixed-capacity pools of game entities, each optionally with an actor.
 *
 * Every game ends up writing the same thing: an array of entities, a flag
 * saying which are alive, a linear search for a free slot, and a skip-the-dead
 * test at the top of each loop. Bullets, enemies, pickups, hit sparks,
 * particles - the same shape each time, and each rewrite is another chance to
 * leave a slot leaked or iterate a dead one.
 *
 * A pool owns that bookkeeping. It can also own one actor per slot, created
 * once when the pool is built and reused thereafter, so spawning costs a
 * visibility change rather than an actor and graphic allocation - which on
 * this hardware also avoids reshuffling sprite indices and forcing tile
 * rewrites on everything downstream.
 *
 * Entity types begin with their actor handle so the pool can bind the two:
 *
 * @code
 * typedef struct {
 *     NGActorHandle actor;   // must be first; the pool fills this in
 *     fixed x, y;
 *     u8 life;
 * } Bullet;
 *
 * NGPoolConfig cfg = {
 *     .arena = &ng_arena_state, .stride = sizeof(Bullet), .capacity = 8,
 *     .asset = &NGVisualAsset_bullet, .z = 11, .anchor = NG_ANCHOR_CENTER,
 * };
 * NGPoolInit(&bullets, &cfg);
 *
 * Bullet *b = NGPoolSpawn(&bullets);
 * if (b) {
 *     b->x = hero_x;
 *     b->life = 70;
 * }
 *
 * NG_POOL_FOREACH(Bullet, b, &bullets) {
 *     b->x += step;
 *     NGActorSetPos(b->actor, b->x, b->y);
 *     if (--b->life == 0) {
 *         NGPoolRetire(&bullets, b);   // safe during iteration
 *     }
 * }
 * @endcode
 */

#ifndef NG_POOL_H
#define NG_POOL_H

#include <ng_types.h>
#include <ng_arena.h>
#include <actor.h>
#include <visual.h>

/**
 * @defgroup pool Entity Pools
 * @ingroup sdk
 * @brief Fixed-capacity entity storage with optional per-slot actors.
 * @{
 */

/** Largest pool the liveness bitmap addresses. */
#define NG_POOL_MAX_CAPACITY 64

/**
 * Pool state. Treat as opaque; the fields exist so a pool can live in a
 * game's own state struct rather than needing a separate allocation.
 */
typedef struct {
    u8 *data;              /**< capacity * stride bytes of entity storage */
    NGActorHandle *actors; /**< One actor per slot, or NULL */
    u32 alive[(NG_POOL_MAX_CAPACITY + 31) / 32];
    u16 stride;
    u8 capacity;
    u8 count;
} NGPool;

/**
 * How to build a pool.
 *
 * Leave @c asset NULL for a pool of plain data - a particle system that draws
 * itself some other way, or a list of spawn points. With an asset set, the
 * pool creates one actor per slot up front and hands it to you in the entity's
 * first field.
 */
typedef struct {
    NGArena *arena;             /**< Where storage comes from */
    u16 stride;                 /**< sizeof(YourEntity) */
    u8 capacity;                /**< Slots, up to NG_POOL_MAX_CAPACITY */
    const NGVisualAsset *asset; /**< Actor asset, or NULL for data only */
    u8 z;                       /**< Z-order for created actors */
    NGAnchor anchor;            /**< Anchor for created actors */
} NGPoolConfig;

/** @name Lifecycle */
/** @{ */

/**
 * Build a pool.
 *
 * Allocates storage from the arena and, when an asset is given, creates one
 * actor per slot and adds it to the scene hidden. Spawning then only has to
 * make an actor visible, so entity churn never disturbs sprite allocation.
 *
 * @param pool Pool to initialise
 * @param cfg Configuration
 * @return 1 on success, 0 if the arena or actor pool is exhausted
 */
u8 NGPoolInit(NGPool *pool, const NGPoolConfig *cfg);

/**
 * Take a free slot.
 *
 * The entity's memory is zeroed apart from its actor handle, so a spawn starts
 * from a known state without the caller clearing fields it does not set.
 *
 * @param pool Pool
 * @return Pointer to the entity, or NULL if the pool is full
 */
void *NGPoolSpawn(NGPool *pool);

/**
 * Return an entity to the pool and hide its actor.
 *
 * Safe to call while iterating with NG_POOL_FOREACH, including on the entity
 * currently being visited.
 *
 * @param pool Pool
 * @param entity Entity previously returned by NGPoolSpawn()
 */
void NGPoolRetire(NGPool *pool, void *entity);

/**
 * Retire every live entity.
 *
 * @param pool Pool
 */
void NGPoolRetireAll(NGPool *pool);
/** @} */

/** @name Queries */
/** @{ */

/**
 * @param pool Pool
 * @return Number of live entities
 */
u8 NGPoolCount(const NGPool *pool);

/**
 * @param pool Pool
 * @return Total slots
 */
u8 NGPoolCapacity(const NGPool *pool);

/**
 * @param pool Pool
 * @return 1 if no slot is free
 */
u8 NGPoolIsFull(const NGPool *pool);
/** @} */

/** @name Iteration */
/** @{ */

/**
 * First live entity, or NULL.
 * Prefer NG_POOL_FOREACH; this exists for hand-written loops.
 */
void *NGPoolFirst(const NGPool *pool);

/**
 * Live entity after @p entity, or NULL.
 */
void *NGPoolNext(const NGPool *pool, const void *entity);

/**
 * Walk every live entity.
 *
 * The successor is captured before the body runs, so retiring the current
 * entity - or any other - mid-loop is safe.
 *
 * @param type Entity struct type
 * @param var Name to bind each entity to
 * @param pool Pool pointer
 */
#define NG_POOL_FOREACH(type, var, pool)                                             \
    for (type *var = (type *)NGPoolFirst(pool), *_ng_pool_next = (type *)0;          \
         (var != (type *)0) && (_ng_pool_next = (type *)NGPoolNext((pool), var), 1); \
         var = _ng_pool_next)
/** @} */

/** @} */ /* end of pool group */

#endif /* NG_POOL_H */
