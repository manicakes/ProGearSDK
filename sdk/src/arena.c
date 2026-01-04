/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// arena.c - Arena memory allocator implementation

#include <arena.h>

// === Standard Arena Backing Storage ===

static u8 persistent_buffer[NG_ARENA_PERSISTENT_SIZE];
static u8 state_buffer[NG_ARENA_STATE_SIZE];
static u8 frame_buffer[NG_ARENA_FRAME_SIZE];

// === Standard Arenas ===

NGArena ng_arena_persistent;
NGArena ng_arena_state;
NGArena ng_arena_frame;

// === Core Operations ===

void NGArenaInit(NGArena *arena, void *buffer, u32 size) {
    arena->base = (u8 *)buffer;
    arena->current = arena->base;
    arena->end = arena->base + size;
}

void *NGArenaAlloc(NGArena *arena, u32 size) {
    // 4-byte alignment for m68k
    u8 *aligned = (u8 *)(((u32)arena->current + 3) & ~3);
    u8 *next = aligned + size;

    if (next > arena->end) {
        return 0;  // Out of memory
    }

    arena->current = next;
    return aligned;
}

void NGArenaReset(NGArena *arena) {
    arena->current = arena->base;
}

// === Temporary Allocations ===

NGArenaMark NGArenaSave(NGArena *arena) {
    return arena->current;
}

void NGArenaRestore(NGArena *arena, NGArenaMark mark) {
    arena->current = mark;
}

// === Queries ===

u32 NGArenaUsed(NGArena *arena) {
    return (u32)(arena->current - arena->base);
}

u32 NGArenaRemaining(NGArena *arena) {
    return (u32)(arena->end - arena->current);
}

// === System Init ===

void NGArenaSystemInit(void) {
    NGArenaInit(&ng_arena_persistent, persistent_buffer, NG_ARENA_PERSISTENT_SIZE);
    NGArenaInit(&ng_arena_state, state_buffer, NG_ARENA_STATE_SIZE);
    NGArenaInit(&ng_arena_frame, frame_buffer, NG_ARENA_FRAME_SIZE);
}
