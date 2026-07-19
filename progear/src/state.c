/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <state.h>
#include <ng_arena.h>

typedef struct {
    NGStateInitFn init;
    NGStateUpdateFn update;
    NGStateCleanupFn cleanup;
} NGStateEntry;

static NGStateEntry g_states[NG_STATE_MAX];
static u8 g_current = NG_STATE_ID_NONE;

void NGStateRegister(u8 id, NGStateInitFn init, NGStateUpdateFn update,
                      NGStateCleanupFn cleanup) {
    if (id == NG_STATE_ID_NONE || id >= NG_STATE_MAX) {
        return;
    }

    g_states[id].init = init;
    g_states[id].update = update;
    g_states[id].cleanup = cleanup;
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
