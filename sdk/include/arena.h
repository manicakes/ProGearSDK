/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// arena.h - Arena memory allocator
//
// Arenas provide fast, zero-fragmentation memory allocation with bulk free.
// Allocations bump a pointer forward; freeing resets the pointer.
//
// Three standard arenas are provided:
//   ng_arena_persistent - Lives entire game (player data, global state)
//   ng_arena_state      - Cleared on level/screen changes
//   ng_arena_frame      - Cleared every frame (temp strings, scratch)

#ifndef ARENA_H
#define ARENA_H

#include <types.h>

// Arena structure
typedef struct NGArena {
    u8 *base;       // Start of memory region
    u8 *current;    // Current allocation pointer (bump pointer)
    u8 *end;        // End of memory region
} NGArena;

// Mark for save/restore (temporary allocations)
typedef u8 *NGArenaMark;

// === Core Operations ===

// Initialize arena with external buffer
void NGArenaInit(NGArena *arena, void *buffer, u32 size);

// Allocate size bytes (4-byte aligned), returns NULL if out of memory
void *NGArenaAlloc(NGArena *arena, u32 size);

// Reset arena to empty (instant bulk free)
void NGArenaReset(NGArena *arena);

// === Temporary Allocations ===

// Save current position (for temporary allocations)
NGArenaMark NGArenaSave(NGArena *arena);

// Restore to saved position (frees everything allocated since save)
void NGArenaRestore(NGArena *arena, NGArenaMark mark);

// === Queries ===

// Get bytes currently used
u32 NGArenaUsed(NGArena *arena);

// Get bytes remaining
u32 NGArenaRemaining(NGArena *arena);

// === Convenience Macros ===

// Allocate a single struct
#define NG_ARENA_ALLOC(arena, type) \
    ((type *)NGArenaAlloc((arena), sizeof(type)))

// Allocate an array of structs
#define NG_ARENA_ALLOC_ARRAY(arena, type, count) \
    ((type *)NGArenaAlloc((arena), sizeof(type) * (count)))

// === Standard Arenas ===

// Default arena sizes (can be overridden before including arena.h)
#ifndef NG_ARENA_PERSISTENT_SIZE
#define NG_ARENA_PERSISTENT_SIZE  8192   // 8 KB
#endif

#ifndef NG_ARENA_STATE_SIZE
#define NG_ARENA_STATE_SIZE      24576   // 24 KB
#endif

#ifndef NG_ARENA_FRAME_SIZE
#define NG_ARENA_FRAME_SIZE       4096   // 4 KB
#endif

// Standard arenas (initialized by NGArenaSystemInit)
extern NGArena ng_arena_persistent;
extern NGArena ng_arena_state;
extern NGArena ng_arena_frame;

// Initialize standard arenas (call once at startup)
void NGArenaSystemInit(void);

#endif // ARENA_H
