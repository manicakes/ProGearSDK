/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file debug.h
 * @brief Development-time diagnostics: frame budget and sprite budget.
 *
 * The NeoGeo gives no feedback when you overrun. A frame that takes too long
 * simply drops; sprites past the hardware's per-scanline limit simply vanish,
 * highest-numbered first, which in practice means the UI. Both failures look
 * like "something is wrong with the graphics" long after the cause.
 *
 * This module measures both, cheaply enough to leave on during development:
 *
 *  - **Frame budget** is read from the LSPC raster counter, so it measures
 *    real elapsed scanlines rather than estimating from instruction counts.
 *    A frame is 264 scanlines; if the work between one VBlank and the next
 *    exceeds that, the frame is late.
 *
 *  - **Sprite budget** is computed from the graphics the SDK is about to
 *    draw, using a difference array, so the cost is proportional to the
 *    number of graphics rather than to scanlines times sprites.
 *
 * Everything here is inert until NGDebugSetEnabled(1), and the HUD is drawn
 * only when you ask for it, so shipping code can leave the calls in place.
 */

#ifndef NG_DEBUG_H
#define NG_DEBUG_H

#include <ng_types.h>

/**
 * @defgroup debug Debug
 * @ingroup sdk
 * @brief Frame-budget and sprite-budget diagnostics.
 * @{
 */

/** Scanlines in one NeoGeo frame, including blanking. */
#define NG_DEBUG_FRAME_LINES 264

/** Sprites the hardware can draw on a single scanline before dropping them. */
#define NG_DEBUG_SPRITES_PER_LINE 96

/** @name Control */
/** @{ */

/**
 * Enable or disable measurement.
 *
 * While disabled the per-frame hooks return immediately, so the cost of
 * leaving instrumentation in a release build is one test per frame.
 *
 * @param enabled 1 to measure, 0 to stop
 */
void NGDebugSetEnabled(u8 enabled);

/**
 * @return 1 if measurement is enabled.
 */
u8 NGDebugIsEnabled(void);
/** @} */

/** @name Frame Budget */
/** @{ */

/**
 * Scanlines consumed by the last completed frame.
 *
 * Measured from the top of the frame to the end of the game's update. Compare
 * against NG_DEBUG_FRAME_LINES: at or above that, the frame overran and the
 * game is no longer running at full rate.
 *
 * @return Scanlines, or 0 if measurement is disabled
 */
u16 NGDebugFrameLines(void);

/**
 * Worst frame seen since the last NGDebugResetPeaks().
 *
 * The peak is what matters - a game that averages comfortably but spikes over
 * budget when six enemies attack at once still stutters at exactly the moment
 * the player cares about.
 *
 * @return Scanlines
 */
u16 NGDebugPeakFrameLines(void);
/** @} */

/** @name Sprite Budget */
/** @{ */

/**
 * Greatest number of sprites on any single scanline in the last frame.
 *
 * Above NG_DEBUG_SPRITES_PER_LINE the hardware drops the excess, starting
 * with the highest-numbered sprites - which is where the UI pool lives, so
 * the visible symptom is usually a menu or HUD with holes in it rather than
 * anything near the actual cause.
 *
 * @return Peak sprite count on one scanline
 */
u16 NGDebugPeakSpritesPerLine(void);

/**
 * Scanline where the sprite peak occurred, useful for working out which
 * layers are stacking up.
 *
 * @return Screen Y of the busiest scanline
 */
u16 NGDebugPeakSpriteLine(void);

/**
 * @return 1 if the last frame exceeded the per-scanline sprite limit.
 */
u8 NGDebugSpritesOverBudget(void);
/** @} */

/** @name Reporting */
/** @{ */

/**
 * Clear the recorded peaks.
 *
 * Call when entering a new scene, or after a deliberate spike, so one bad
 * frame during loading does not mask the rest of the session.
 */
void NGDebugResetPeaks(void);

/**
 * Draw a one-line summary on the fix layer.
 *
 * Shows frame scanlines (current and peak) and peak sprites per scanline,
 * with a marker when either is over budget. Uses the fix layer, so it costs
 * no sprites and cannot itself perturb the sprite budget it is reporting.
 *
 * @param row Fix-layer row to draw on (0-31)
 */
void NGDebugDrawHUD(u8 row);
/** @} */

/** @} */ /* end of debug group */

#endif /* NG_DEBUG_H */
