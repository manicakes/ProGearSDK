/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <state.h>
#include <ng_arena.h>

typedef struct {
    const char *name;
    NGStateInitFn init;
    NGStateUpdateFn update;
    NGStateCleanupFn cleanup;
} NGStateEntry;

static NGStateEntry g_states[NG_STATE_MAX];
static u8 g_current = NG_STATE_ID_NONE;

/* Registration order, so a menu lists scenes the way the game declared them
 * rather than in id order. */
static u8 g_order[NG_STATE_MAX];
static u8 g_count;

void NGStateRegister(u8 id, const char *name, NGStateInitFn init, NGStateUpdateFn update,
                     NGStateCleanupFn cleanup) {
    if (id == NG_STATE_ID_NONE || id >= NG_STATE_MAX) {
        return;
    }

    if (!g_states[id].update && g_count < NG_STATE_MAX) {
        g_order[g_count++] = id; /* first registration takes a listing slot */
    }

    g_states[id].name = name;
    g_states[id].init = init;
    g_states[id].update = update;
    g_states[id].cleanup = cleanup;
}

const char *NGStateGetName(u8 id) {
    if (id == NG_STATE_ID_NONE || id >= NG_STATE_MAX) {
        return 0;
    }
    return g_states[id].name;
}

u8 NGStateCount(void) {
    return g_count;
}

u8 NGStateIdAt(u8 index) {
    return (index < g_count) ? g_order[index] : NG_STATE_ID_NONE;
}

void NGStateStart(u8 id) {
    g_current = id;
    g_states[id].init();
}

void NGStateUpdate(void) {
    if (g_current == NG_STATE_ID_NONE) {
        return;
    }

    u8 next = g_states[g_current].update();
    if (next == NG_STATE_ID_NONE || next == g_current) {
        return;
    }

    if (g_states[g_current].cleanup) {
        g_states[g_current].cleanup();
    }

    NGArenaReset(&ng_arena_state);

    g_current = next;
    g_states[g_current].init();
}

u8 NGStateGetCurrent(void) {
    return g_current;
}
