/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_arena.h
 * @brief Arena memory allocator
 *
 * Arenas provide fast, zero-fragmentation memory allocation with bulk free.
 * Allocations bump a pointer forward; freeing resets the pointer.
 *
 * Three standard arenas are provided:
 *   ng_arena_persistent - Lives entire game (player data, global state)
 *   ng_arena_state      - Cleared on level/screen changes
 *   ng_arena_frame      - Cleared every frame (temp strings, scratch)
 */

#ifndef _NG_ARENA_H_
#define _NG_ARENA_H_

#include <ng_types.h>

/**
 * @defgroup arena Arena Memory Allocator
 * @ingroup hal
 * @brief Fast bump-pointer memory allocation with bulk free.
 * @{
 */

/**
 * Arena allocator structure.
 * Uses bump-pointer allocation for zero-fragmentation memory management.
 */
typedef struct NGArena {
    u8 *base;    /**< Start of memory region */
    u8 *current; /**< Current allocation pointer (bump pointer) */
    u8 *end;     /**< End of memory region */
} NGArena;

/** Mark for save/restore (temporary allocations) */
typedef u8 *NGArenaMark;

/** @name Core Operations */
/** @{ */

/**
 * Initialize arena with external buffer.
 * @param arena Arena to initialize
 * @param buffer Memory buffer to use
 * @param size Size of buffer in bytes
 */
void NGArenaInit(NGArena *arena, void *buffer, u32 size);

/**
 * Allocate memory from arena.
 * @param arena Arena to allocate from
 * @param size Bytes to allocate (will be 4-byte aligned)
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *NGArenaAlloc(NGArena *arena, u32 size);

/**
 * Reset arena to empty (instant bulk free).
 * @param arena Arena to reset
 */
void NGArenaReset(NGArena *arena);
/** @} */

/** @name Temporary Allocations */
/** @{ */

/**
 * Save current position for later restore.
 * @param arena Arena to save
 * @return Mark that can be passed to NGArenaRestore()
 */
NGArenaMark NGArenaSave(NGArena *arena);

/**
 * Restore arena to saved position.
 * Frees everything allocated since the mark was saved.
 * @param arena Arena to restore
 * @param mark Mark from NGArenaSave()
 */
void NGArenaRestore(NGArena *arena, NGArenaMark mark);
/** @} */

/** @name Queries */
/** @{ */

/**
 * Get bytes currently used.
 * @param arena Arena to query
 * @return Bytes used
 */
u32 NGArenaUsed(NGArena *arena);

/**
 * Get bytes remaining.
 * @param arena Arena to query
 * @return Bytes available
 */
u32 NGArenaRemaining(NGArena *arena);
/** @} */

/** @name Convenience Macros */
/** @{ */

/**
 * Allocate a single struct.
 * @param arena Arena to allocate from
 * @param type Type to allocate
 * @return Typed pointer to allocated struct
 */
#define NG_ARENA_ALLOC(arena, type) ((type *)NGArenaAlloc((arena), sizeof(type)))

/**
 * Allocate an array of structs.
 * @param arena Arena to allocate from
 * @param type Type of array elements
 * @param count Number of elements
 * @return Typed pointer to allocated array
 */
#define NG_ARENA_ALLOC_ARRAY(arena, type, count) \
    ((type *)NGArenaAlloc((arena), sizeof(type) * (count)))
/** @} */

/** @name Standard Arenas */
/** @{ */

/** Default size for persistent arena (can be overridden before including ng_arena.h) */
#ifndef NG_ARENA_PERSISTENT_SIZE
#define NG_ARENA_PERSISTENT_SIZE 8192 /**< 8 KB */
#endif

/** Default size for state arena (can be overridden before including ng_arena.h) */
#ifndef NG_ARENA_STATE_SIZE
#define NG_ARENA_STATE_SIZE 24576 /**< 24 KB */
#endif

/** Default size for frame arena (can be overridden before including ng_arena.h) */
#ifndef NG_ARENA_FRAME_SIZE
#define NG_ARENA_FRAME_SIZE 4096 /**< 4 KB */
#endif

/** Persistent arena - lives entire game (player data, global state) */
extern NGArena ng_arena_persistent;

/** State arena - cleared on level/screen changes */
extern NGArena ng_arena_state;

/** Frame arena - cleared every frame (temp strings, scratch) */
extern NGArena ng_arena_frame;

/**
 * Initialize standard arenas.
 * Call once at startup (automatically called by NGEngineInit).
 */
void NGArenaSystemInit(void);
/** @} */

/** @} */ /* end of arena group */

#endif // _NG_ARENA_H_
