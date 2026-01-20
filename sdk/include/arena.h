/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file arena.h
 * @brief Arena memory allocator.
 *
 * Arenas provide fast, zero-fragmentation memory allocation with bulk free.
 * Allocations bump a pointer forward; freeing resets the pointer.
 *
 * Three standard arenas are provided:
 *   arena_persistent - Lives entire game (player data, global state)
 *   arena_state      - Cleared on level/screen changes
 *   arena_frame      - Cleared every frame (temp strings, scratch)
 */

#ifndef ARENA_H
#define ARENA_H

#include <types.h>

/**
 * Arena allocator structure.
 * Uses bump-pointer allocation for zero-fragmentation memory management.
 */
typedef struct Arena {
    u8 *base;    /**< Start of memory region */
    u8 *current; /**< Current allocation pointer (bump pointer) */
    u8 *end;     /**< End of memory region */
} Arena;

/** Mark for save/restore (temporary allocations) */
typedef u8 *ArenaMark;

/** @defgroup arenacore Core Operations
 *  @{
 */

/**
 * Initialize arena with external buffer.
 * @param arena Arena to initialize
 * @param buffer Memory buffer to use
 * @param size Size of buffer in bytes
 */
void ArenaInit(Arena *arena, void *buffer, u32 size);

/**
 * Allocate memory from arena.
 * @param arena Arena to allocate from
 * @param size Bytes to allocate (will be 4-byte aligned)
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *ArenaAlloc(Arena *arena, u32 size);

/**
 * Reset arena to empty (instant bulk free).
 * @param arena Arena to reset
 */
void ArenaReset(Arena *arena);

/** @} */

/** @defgroup arenatemp Temporary Allocations
 *  @{
 */

/**
 * Save current position for later restore.
 * @param arena Arena to save
 * @return Mark that can be passed to ArenaRestore()
 */
ArenaMark ArenaSave(Arena *arena);

/**
 * Restore arena to saved position.
 * Frees everything allocated since the mark was saved.
 * @param arena Arena to restore
 * @param mark Mark from ArenaSave()
 */
void ArenaRestore(Arena *arena, ArenaMark mark);

/** @} */

/** @defgroup arenaquery Queries
 *  @{
 */

/**
 * Get bytes currently used.
 * @param arena Arena to query
 * @return Bytes used
 */
u32 ArenaUsed(Arena *arena);

/**
 * Get bytes remaining.
 * @param arena Arena to query
 * @return Bytes available
 */
u32 ArenaRemaining(Arena *arena);

/** @} */

/** @defgroup arenamacro Convenience Macros
 *  @{
 */

/**
 * Allocate a single struct.
 * @param arena Arena to allocate from
 * @param type Type to allocate
 * @return Typed pointer to allocated struct
 */
#define ARENA_ALLOC(arena, type) ((type *)ArenaAlloc((arena), sizeof(type)))

/**
 * Allocate an array of structs.
 * @param arena Arena to allocate from
 * @param type Type of array elements
 * @param count Number of elements
 * @return Typed pointer to allocated array
 */
#define ARENA_ALLOC_ARRAY(arena, type, count) ((type *)ArenaAlloc((arena), sizeof(type) * (count)))

/** @} */

/** @defgroup arenastd Standard Arenas
 *  @brief Pre-configured arenas for common use cases.
 *  @{
 */

/** Default size for persistent arena */
#ifndef ARENA_PERSISTENT_SIZE
#define ARENA_PERSISTENT_SIZE 8192 /**< 8 KB */
#endif

/** Default size for state arena */
#ifndef ARENA_STATE_SIZE
#define ARENA_STATE_SIZE 24576 /**< 24 KB */
#endif

/** Default size for frame arena */
#ifndef ARENA_FRAME_SIZE
#define ARENA_FRAME_SIZE 4096 /**< 4 KB */
#endif

/** Persistent arena - lives entire game (player data, global state) */
extern Arena arena_persistent;

/** State arena - cleared on level/screen changes */
extern Arena arena_state;

/** Frame arena - cleared every frame (temp strings, scratch) */
extern Arena arena_frame;

/**
 * Initialize standard arenas.
 * Call once at startup (automatically called by EngineInit).
 */
void ArenaSystemInit(void);

/** @} */

#endif /* ARENA_H */
