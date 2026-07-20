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

#ifndef NG_SDK_INTERNAL_H
#define NG_SDK_INTERNAL_H

#include <ng_types.h>
#include "actor.h"
#include "backdrop.h"
#include "terrain.h"

/* ------------------------------------------------------------------------ */
/* Shared utilities                                                         */
/* ------------------------------------------------------------------------ */

/** Set a bit in a 256-bit palette bitmask (32 bytes, indexed by palette ID) */
static inline void _NGPaletteMaskSet(u8 *mask, u8 palette) {
    mask[palette >> 3] |= (u8)(1 << (palette & 7));
}

/* ------------------------------------------------------------------------ */
/* Camera internals                                                         */
/* ------------------------------------------------------------------------ */

/** Get camera X position with shake offset applied (for rendering) */
fixed NGCameraGetRenderX(void);

/** Get camera Y position with shake offset applied (for rendering) */
fixed NGCameraGetRenderY(void);

/* ------------------------------------------------------------------------ */
/* Graphic system internals                                                 */
/* ------------------------------------------------------------------------ */

/** Initialize graphics system (called by scene init) */
void NGGraphicSystemInit(void);

/** Draw all active graphics in layer/z-order (called by scene draw) */
void NGGraphicSystemDraw(void);

/** Reset graphics system, destroying all graphics (called on scene reset) */
void NGGraphicSystemReset(void);

/** Peak sprites on any scanline, and which scanline (for NGDebug) */
void _NGGraphicPeakSpriteLoad(u16 *out_peak, u16 *out_line);

/** Frame-budget sampling hooks, called by the engine each frame */
void _NGDebugFrameStart(void);
void _NGDebugFrameEnd(void);

/** Hide (paused=1) or restore (paused=0) graphics marked NG_PAUSE_HIDE */
void _NGGraphicApplyPause(u8 paused);

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

/** Visual asset backing an actor, or NULL */
const NGVisualAsset *_NGActorGetAsset(NGActorHandle actor);

/** Frame index within the actor's current animation */
u16 _NGActorGetAnimFrame(NGActorHandle actor);

/** Absolute frame index into the asset's frame list */
u16 _NGActorGetAbsoluteFrame(NGActorHandle actor);

/** Horizontal flip state (1 = mirrored) */
u8 _NGActorGetHFlip(NGActorHandle actor);

/* ------------------------------------------------------------------------ */
/* Backdrop internals                                                       */
/* ------------------------------------------------------------------------ */

/** Initialize the backdrop subsystem (called by scene init) */
void _NGBackdropSystemInit(void);

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

#endif /* NG_SDK_INTERNAL_H */
