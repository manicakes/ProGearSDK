/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <arena.h>

static u8 persistent_buffer[ARENA_PERSISTENT_SIZE];
static u8 state_buffer[ARENA_STATE_SIZE];
static u8 frame_buffer[ARENA_FRAME_SIZE];

Arena arena_persistent;
Arena arena_state;
Arena arena_frame;

void ArenaInit(Arena *arena, void *buffer, u32 size) {
    arena->base = (u8 *)buffer;
    arena->current = arena->base;
    arena->end = arena->base + size;
}

void *ArenaAlloc(Arena *arena, u32 size) {
    u8 *aligned = (u8 *)(((u32)arena->current + 3) & ~3); /* 4-byte alignment for m68k */
    u8 *next = aligned + size;

    if (next > arena->end) {
        return 0;
    }

    arena->current = next;
    return aligned;
}

void ArenaReset(Arena *arena) {
    arena->current = arena->base;
}

ArenaMark ArenaSave(Arena *arena) {
    return arena->current;
}

void ArenaRestore(Arena *arena, ArenaMark mark) {
    arena->current = mark;
}

u32 ArenaUsed(Arena *arena) {
    return (u32)(arena->current - arena->base);
}

u32 ArenaRemaining(Arena *arena) {
    return (u32)(arena->end - arena->current);
}

void ArenaSystemInit(void) {
    ArenaInit(&arena_persistent, persistent_buffer, ARENA_PERSISTENT_SIZE);
    ArenaInit(&arena_state, state_buffer, ARENA_STATE_SIZE);
    ArenaInit(&arena_frame, frame_buffer, ARENA_FRAME_SIZE);
}
