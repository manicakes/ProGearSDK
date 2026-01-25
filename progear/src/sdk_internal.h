/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sdk_internal.h
 * @brief Internal declarations shared between SDK modules.
 *
 * This header is NOT part of the public API. It contains declarations for
 * functions that need to be called across module boundaries within the SDK
 * implementation, but should not be exposed to users.
 *
 * All functions here use the _NG prefix to indicate internal use.
 */

#ifndef _SDK_INTERNAL_H_
#define _SDK_INTERNAL_H_

#include <ng_types.h>
#include "actor.h"
#include "backdrop.h"
#include "terrain.h"

/* ------------------------------------------------------------------------ */
/* Scene internals                                                          */
/* ------------------------------------------------------------------------ */

/** Mark the render queue as needing re-sort (called when Z or visibility changes) */
void _NGSceneMarkRenderQueueDirty(void);

/* ------------------------------------------------------------------------ */
/* Actor internals                                                          */
/* ------------------------------------------------------------------------ */

/** Initialize the actor subsystem (called by scene init) */
void _NGActorSystemInit(void);

/** Update all actors (animation, etc.) */
void _NGActorSystemUpdate(void);

/** Sync actor state to graphics hardware */
void _NGActorSyncGraphics(void);

/** Collect palette indices used by actors into a bitmask */
void _NGActorCollectPalettes(u8 *palette_mask);

/** Check if an actor is currently in the scene */
u8 _NGActorIsInScene(NGActorHandle handle);

/** Get an actor's Z depth */
u8 _NGActorGetZ(NGActorHandle handle);

/** Check if an actor is in screen-space mode */
u8 _NGActorIsScreenSpace(NGActorHandle handle);

/* ------------------------------------------------------------------------ */
/* Backdrop internals                                                       */
/* ------------------------------------------------------------------------ */

/** Initialize the backdrop subsystem (called by scene init) */
void _NGBackdropSystemInit(void);

/** Update all backdrops */
void _NGBackdropSystemUpdate(void);

/** Sync backdrop state to graphics hardware */
void _NGBackdropSyncGraphics(void);

/** Collect palette indices used by backdrops into a bitmask */
void _NGBackdropCollectPalettes(u8 *palette_mask);

/* ------------------------------------------------------------------------ */
/* Terrain internals                                                        */
/* ------------------------------------------------------------------------ */

/** Initialize the terrain subsystem (called by scene init) */
void _NGTerrainSystemInit(void);

/** Sync terrain state to graphics hardware */
void _NGTerrainSyncGraphics(void);

/** Collect palette indices used by terrain into a bitmask */
void _NGTerrainCollectPalettes(u8 *palette_mask);

#endif /* _SDK_INTERNAL_H_ */
