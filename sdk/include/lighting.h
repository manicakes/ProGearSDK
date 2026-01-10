/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file lighting.h
 * @brief Global lighting and visual effects system.
 *
 * The lighting system provides stackable visual effects through palette
 * manipulation. Effects are composited using a priority-based layer stack,
 * with all palette updates batched into a single VRAM write per frame.
 *
 * ## Layer System
 *
 * Effects are organized into layers with priorities. Lower priority layers
 * are applied first, higher priority layers on top:
 *
 * - **Ambient** (priority ~50): Persistent world state (day/night, weather)
 * - **Overlay** (priority ~100): UI effects (menu dimming)
 * - **Transient** (priority ~200): Momentary effects (flashes, pulses)
 *
 * ## Composition Rules
 *
 * - **Tints**: Additive (clamped to valid range)
 * - **Brightness**: Multiplicative
 * - **Saturation**: Multiplicative
 *
 * ## Example
 *
 * ```c
 * // Apply night mode (persists until removed)
 * NGLightingLayerHandle night = NGLightingPush(NG_LIGHTING_PRIORITY_AMBIENT);
 * NGLightingSetTint(night, -10, -10, 20);    // Blue tint
 * NGLightingSetBrightness(night, FIX(0.7));  // Dim
 *
 * // Flash effect (auto-expires)
 * NGLightingFlash(80, 80, 60, 6);  // Warm white flash, 6 frames
 *
 * // Menu dimming (remove when menu closes)
 * NGLightingLayerHandle menu_dim = NGLightingPush(NG_LIGHTING_PRIORITY_OVERLAY);
 * NGLightingSetBrightness(menu_dim, FIX(0.5));
 * // ... later ...
 * NGLightingPop(menu_dim);  // Reverts to night mode only
 * ```
 *
 * @note The system maintains a backup of original palette data. Palettes
 *       are automatically restored when all layers are removed.
 */

#ifndef LIGHTING_H
#define LIGHTING_H

#include <types.h>
#include <ngmath.h>
#include <color.h>

/** @defgroup lightingconst Lighting Constants
 *  @{
 */

/** Maximum number of concurrent lighting layers */
#define NG_LIGHTING_MAX_LAYERS 8

/** Invalid layer handle (returned on error) */
#define NG_LIGHTING_INVALID_HANDLE 0xFF

/** @} */

/** @defgroup lightingpriority Priority Levels
 *  @brief Suggested priority values for different effect types.
 *  @{
 */

/** Ambient effects: day/night, weather, global mood (applied first) */
#define NG_LIGHTING_PRIORITY_AMBIENT 50

/** Overlay effects: menu dimming, pause effects */
#define NG_LIGHTING_PRIORITY_OVERLAY 100

/** Transient effects: flashes, pulses (applied last) */
#define NG_LIGHTING_PRIORITY_TRANSIENT 200

/** @} */

/** @defgroup lightingpreset Lighting Presets
 *  @{
 */

/** Pre-defined lighting presets */
typedef enum {
    NG_LIGHTING_PRESET_DAY,        /**< Normal daylight (no modification) */
    NG_LIGHTING_PRESET_NIGHT,      /**< Nighttime: blue tint, dimmed */
    NG_LIGHTING_PRESET_SUNSET,     /**< Sunset: warm orange tint */
    NG_LIGHTING_PRESET_DAWN,       /**< Dawn: cool pink/purple tint */
    NG_LIGHTING_PRESET_SANDSTORM,  /**< Sandstorm: tan/brown tint, desaturated */
    NG_LIGHTING_PRESET_FOG,        /**< Fog: gray tint, desaturated */
    NG_LIGHTING_PRESET_UNDERWATER, /**< Underwater: blue-green tint */
    NG_LIGHTING_PRESET_SEPIA,      /**< Sepia: warm brown, desaturated */
    NG_LIGHTING_PRESET_MENU_DIM,   /**< Menu dimming: darkened */
} NGLightingPreset;

/** @} */

/** @defgroup lightingblend Blend Modes
 *  @{
 */

/** How a layer's effects are applied */
typedef enum {
    /** Normal blending: tint additive, brightness multiplicative */
    NG_LIGHTING_BLEND_NORMAL,
    /** Additive: brightness adds instead of multiplies (for flashes) */
    NG_LIGHTING_BLEND_ADDITIVE,
} NGLightingBlendMode;

/** @} */

/** Handle to a lighting layer */
typedef u8 NGLightingLayerHandle;

/** @defgroup lightinginit Initialization
 *  @{
 */

/**
 * Initialize the lighting system.
 * Called automatically by NGEngineInit().
 */
void NGLightingInit(void);

/**
 * Reset the lighting system, removing all layers.
 * Restores all palettes to their original state.
 */
void NGLightingReset(void);

/** @} */

/** @defgroup lightinglayer Layer Management
 *  @{
 */

/**
 * Push a new lighting layer onto the stack.
 *
 * The layer starts with neutral settings (no tint, 1.0 brightness/saturation).
 * Configure it using NGLightingSet* functions after creation.
 *
 * @param priority Layer priority (lower = applied first). Use
 *                 NG_LIGHTING_PRIORITY_* constants as guidelines.
 * @return Layer handle, or NG_LIGHTING_INVALID_HANDLE if no slots available
 *
 * @note Layers with the same priority are applied in creation order.
 */
NGLightingLayerHandle NGLightingPush(u8 priority);

/**
 * Remove a lighting layer from the stack.
 *
 * The visual effect reverts to whatever layers remain active.
 *
 * @param handle Layer handle from NGLightingPush()
 */
void NGLightingPop(NGLightingLayerHandle handle);

/**
 * Check if a layer handle is still valid and active.
 *
 * @param handle Layer handle to check
 * @return 1 if active, 0 if inactive or invalid
 */
u8 NGLightingLayerActive(NGLightingLayerHandle handle);

/**
 * Get the number of currently active layers.
 *
 * @return Count of active layers (0 to NG_LIGHTING_MAX_LAYERS)
 */
u8 NGLightingGetLayerCount(void);

/** @} */

/** @defgroup lightingconfig Layer Configuration
 *  @{
 */

/**
 * Set color tint for a layer.
 *
 * Tint values are added to each color channel. Positive values shift toward
 * that color, negative values shift away.
 *
 * @param handle Layer handle
 * @param r Red shift (-31 to +31)
 * @param g Green shift (-31 to +31)
 * @param b Blue shift (-31 to +31)
 *
 * @note Values are clamped to the valid range.
 */
void NGLightingSetTint(NGLightingLayerHandle handle, s8 r, s8 g, s8 b);

/**
 * Set brightness multiplier for a layer.
 *
 * @param handle Layer handle
 * @param brightness Brightness scale (FIX(0.0)=black, FIX(1.0)=normal,
 *                   FIX(2.0)=overbright)
 */
void NGLightingSetBrightness(NGLightingLayerHandle handle, fixed brightness);

/**
 * Set saturation multiplier for a layer.
 *
 * @param handle Layer handle
 * @param saturation Saturation scale (FIX(0.0)=grayscale, FIX(1.0)=normal)
 */
void NGLightingSetSaturation(NGLightingLayerHandle handle, fixed saturation);

/**
 * Set automatic expiration for a layer.
 *
 * @param handle Layer handle
 * @param frames Duration in frames (0 = permanent, never expires)
 */
void NGLightingSetDuration(NGLightingLayerHandle handle, u16 frames);

/**
 * Set blend mode for a layer.
 *
 * @param handle Layer handle
 * @param mode Blend mode (NG_LIGHTING_BLEND_NORMAL or NG_LIGHTING_BLEND_ADDITIVE)
 */
void NGLightingSetBlendMode(NGLightingLayerHandle handle, NGLightingBlendMode mode);

/** @} */

/** @defgroup lightingconvenience Convenience Functions
 *  @{
 */

/**
 * Create a flash effect.
 *
 * Creates a high-priority additive layer that auto-expires. Perfect for
 * explosions, lightning strikes, damage feedback, etc.
 *
 * @param r Red tint (-31 to +31, typically positive for flashes)
 * @param g Green tint (-31 to +31)
 * @param b Blue tint (-31 to +31)
 * @param duration Flash duration in frames
 * @return Layer handle (can be ignored for fire-and-forget usage)
 */
NGLightingLayerHandle NGLightingFlash(s8 r, s8 g, s8 b, u16 duration);

/**
 * Apply a preset lighting configuration.
 *
 * Creates a layer at AMBIENT priority with the preset's settings.
 *
 * @param preset Preset to apply
 * @return Layer handle for later removal
 */
NGLightingLayerHandle NGLightingApplyPreset(NGLightingPreset preset);

/**
 * Animate a layer's brightness over time.
 *
 * The layer will smoothly transition from its current brightness to
 * the target over the specified duration.
 *
 * @param handle Layer handle
 * @param target Target brightness (fixed-point)
 * @param frames Animation duration in frames
 */
void NGLightingFadeBrightness(NGLightingLayerHandle handle, fixed target, u16 frames);

/**
 * Animate a layer's tint over time.
 *
 * @param handle Layer handle
 * @param r Target red tint
 * @param g Target green tint
 * @param b Target blue tint
 * @param frames Animation duration in frames
 */
void NGLightingFadeTint(NGLightingLayerHandle handle, s8 r, s8 g, s8 b, u16 frames);

/** @} */

/** @defgroup lightingupdate Update
 *  @{
 */

/**
 * Update the lighting system.
 *
 * Called automatically by NGEngineFrameEnd(). Handles:
 * - Ticking layer durations and removing expired layers
 * - Processing fade animations
 * - Resolving the layer stack into final palette values
 * - Writing changed palettes to VRAM (single batched write)
 *
 * @note Only writes to VRAM if palettes have actually changed.
 */
void NGLightingUpdate(void);

/**
 * Force a full palette recalculation and VRAM write.
 *
 * Normally not needed, but useful after directly modifying palettes.
 */
void NGLightingInvalidate(void);

/** @} */

/** @defgroup lightingquery State Queries
 *  @{
 */

/**
 * Check if any lighting effects are currently active.
 *
 * @return 1 if at least one layer is active, 0 otherwise
 */
u8 NGLightingIsActive(void);

/**
 * Check if lighting is currently animating (fading).
 *
 * @return 1 if any layer has an active fade animation, 0 otherwise
 */
u8 NGLightingIsAnimating(void);

/** @} */

#endif // LIGHTING_H
