/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <pool.h>
#include <ng_string.h>

#include "sdk_internal.h"

/* Liveness is a bitmap rather than a per-slot flag: iteration can then skip a
 * whole word of dead slots at a time, which matters for pools that are mostly
 * empty most of the time - the usual case for bullets and effects. */
static u8 slot_alive(const NGPool *pool, u8 index) {
    return (pool->alive[index >> 5] >> (index & 31)) & 1u;
}

static void slot_set(NGPool *pool, u8 index) {
    pool->alive[index >> 5] |= (u32)1u << (index & 31);
}

static void slot_clear(NGPool *pool, u8 index) {
    pool->alive[index >> 5] &= ~((u32)1u << (index & 31));
}

static void *slot_data(const NGPool *pool, u8 index) {
    return pool->data + (u32)index * pool->stride;
}

static u8 slot_index(const NGPool *pool, const void *entity) {
    u32 offset = (u32)((const u8 *)entity - pool->data);
    return (u8)(offset / pool->stride);
}

u8 NGPoolInit(NGPool *pool, const NGPoolConfig *cfg) {
    if (!pool || !cfg || !cfg->arena || cfg->stride == 0 || cfg->capacity == 0)
        return 0;
    if (cfg->capacity > NG_POOL_MAX_CAPACITY)
        return 0;
    /* The actor handle is written into the entity's first field, so a stride
     * smaller than that would corrupt whatever follows. */
    if (cfg->asset && cfg->stride < sizeof(NGActorHandle))
        return 0;

    pool->stride = cfg->stride;
    pool->capacity = cfg->capacity;
    pool->count = 0;
    pool->actors = 0;
    for (u8 i = 0; i < (NG_POOL_MAX_CAPACITY + 31) / 32; i++) {
        pool->alive[i] = 0;
    }

    pool->data = (u8 *)NGArenaAlloc(cfg->arena, (u32)cfg->stride * cfg->capacity);
    if (!pool->data)
        return 0;

    if (!cfg->asset)
        return 1;

    pool->actors = (NGActorHandle *)NGArenaAlloc(cfg->arena, sizeof(NGActorHandle) * cfg->capacity);
    if (!pool->actors)
        return 0;

    /* Create every actor up front and leave it in the scene, hidden. Spawning
     * is then a visibility change: no actor allocation, and no shifting of
     * sprite indices that would force neighbouring graphics to redraw. */
    for (u8 i = 0; i < cfg->capacity; i++) {
        NGActorHandle a = NGActorCreate(cfg->asset, 0, 0);
        pool->actors[i] = a;
        if (a == NG_ACTOR_INVALID) {
            pool->capacity = i; /* keep what we got; the rest stay unusable */
            return i > 0;
        }
        NGActorSetAnchor(a, cfg->anchor);
        NGActorAddToScene(a, 0, 0, cfg->z);
        NGActorSetVisible(a, 0);
    }
    return 1;
}

void *NGPoolSpawn(NGPool *pool) {
    if (!pool || pool->count >= pool->capacity)
        return 0;

    for (u8 i = 0; i < pool->capacity; i++) {
        if (slot_alive(pool, i))
            continue;

        void *entity = slot_data(pool, i);

        /* Clear to a known state, then restore the actor binding so callers
         * never have to remember to reattach it. */
        memset(entity, 0, pool->stride);
        if (pool->actors) {
            *(NGActorHandle *)entity = pool->actors[i];
            NGActorSetVisible(pool->actors[i], 1);
        }

        slot_set(pool, i);
        pool->count++;
        return entity;
    }
    return 0;
}

void NGPoolRetire(NGPool *pool, void *entity) {
    if (!pool || !entity)
        return;

    u8 i = slot_index(pool, entity);
    if (i >= pool->capacity || !slot_alive(pool, i))
        return;

    if (pool->actors && pool->actors[i] != NG_ACTOR_INVALID) {
        NGActorSetVisible(pool->actors[i], 0);
    }
    slot_clear(pool, i);
    pool->count--;
}

void NGPoolRetireAll(NGPool *pool) {
    if (!pool)
        return;
    for (u8 i = 0; i < pool->capacity; i++) {
        if (!slot_alive(pool, i))
            continue;
        if (pool->actors && pool->actors[i] != NG_ACTOR_INVALID) {
            NGActorSetVisible(pool->actors[i], 0);
        }
        slot_clear(pool, i);
    }
    pool->count = 0;
}

u8 NGPoolCount(const NGPool *pool) {
    return pool ? pool->count : 0;
}

u8 NGPoolCapacity(const NGPool *pool) {
    return pool ? pool->capacity : 0;
}

u8 NGPoolIsFull(const NGPool *pool) {
    return (pool && pool->count >= pool->capacity) ? 1 : 0;
}

void *NGPoolFirst(const NGPool *pool) {
    if (!pool)
        return 0;
    for (u8 i = 0; i < pool->capacity; i++) {
        if (slot_alive(pool, i))
            return slot_data(pool, i);
    }
    return 0;
}

void *NGPoolNext(const NGPool *pool, const void *entity) {
    if (!pool || !entity)
        return 0;
    for (u8 i = (u8)(slot_index(pool, entity) + 1); i < pool->capacity; i++) {
        if (slot_alive(pool, i))
            return slot_data(pool, i);
    }
    return 0;
}
