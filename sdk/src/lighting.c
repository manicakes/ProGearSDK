/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <lighting.h>
#include <palette.h>
#include <color.h>

/** Maximum palettes to back up for lighting effects.
 *  This is a sparse list - only palettes actually in use are stored.
 *  Cost: ~33 bytes per entry (1 byte index + 32 bytes for 16 colors).
 */
#define LIGHTING_MAX_BACKUP_PALETTES 32

/* External functions to collect palettes from scene objects */
extern void _NGActorCollectPalettes(u8 *palette_mask);
extern void _NGBackdropCollectPalettes(u8 *palette_mask);
extern void _NGTerrainCollectPalettes(u8 *palette_mask);

/** Backup entry for a single palette */
typedef struct {
    u8 palette_index;
    NGColor colors[NG_PAL_SIZE];
} PaletteBackup;

/** Internal layer state */
typedef struct {
    u8 active;
    u8 priority;
    s8 tint_r;
    s8 tint_g;
    s8 tint_b;
    fixed brightness;
    fixed saturation;
    u16 duration;
    NGLightingBlendMode blend_mode;

    /* Fade animation state */
    u8 fade_active;
    u8 fade_tint_active;
    u16 fade_frames_remaining;
    fixed fade_brightness_current;
    fixed fade_brightness_target;
    fixed fade_brightness_step;
    s8 fade_tint_target_r;
    s8 fade_tint_target_g;
    s8 fade_tint_target_b;
    s16 fade_tint_current_r; /* 8.8 fixed point accumulator */
    s16 fade_tint_current_g;
    s16 fade_tint_current_b;
    s16 fade_tint_step_r; /* 8.8 fixed point step per frame */
    s16 fade_tint_step_g;
    s16 fade_tint_step_b;
} LightingLayer;

/** Global lighting system state */
static struct {
    LightingLayer layers[NG_LIGHTING_MAX_LAYERS];
    u8 dirty;
    u8 initialized;
    u8 backup_valid;

    /* Sparse backup of only palettes in use */
    PaletteBackup backup[LIGHTING_MAX_BACKUP_PALETTES];
    u8 backup_count; /* Number of palettes actually backed up */

    /* Pre-computed combined transform for all layers */
    s16 combined_tint_r;
    s16 combined_tint_g;
    s16 combined_tint_b;
    fixed combined_brightness;
    fixed combined_saturation;

    /* Additive tint (from BLEND_ADDITIVE layers) applied after brightness */
    s16 additive_tint_r;
    s16 additive_tint_g;
    s16 additive_tint_b;

    /* Pre-baked preset state */
    u8 prebaked_handle;       /* Handle for active preset (0xFF = none) */
    u8 prebaked_preset_id;    /* Current preset ID */
    u8 prebaked_fading;       /* Is a fade animation in progress? */
    u8 prebaked_fade_out;     /* Fading out (pop) vs fading in (push)? */
    u8 prebaked_current_step; /* Current fade step */
    u8 prebaked_max_steps;    /* Total steps in preset */
    u16 prebaked_frames_remaining;
    u16 prebaked_frames_per_step;
    u16 prebaked_frame_counter;
} g_lighting;

/* Forward declarations */
static void backup_palettes(void);
static void restore_palettes(void);
static void resolve_palettes(void);
static void apply_prebaked_step(u8 preset_id, u8 step);
static void recalc_combined_transform(void);
static s16 clamp_tint(s16 val);

/* Weak default - games with lighting_presets in assets.yaml provide a strong definition */
__attribute__((weak)) void NGLightingInitPresets(void) {}

void NGLightingInit(void) {
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        g_lighting.layers[i].active = 0;
    }
    g_lighting.dirty = 0;
    g_lighting.initialized = 1;
    g_lighting.backup_valid = 0;
    g_lighting.backup_count = 0;

    /* Initialize pre-baked preset state */
    g_lighting.prebaked_handle = NG_LIGHTING_INVALID_HANDLE;
    g_lighting.prebaked_fading = 0;

    /* Register pre-baked presets if available (provided by progear_assets.h) */
    NGLightingInitPresets();
}

void NGLightingReset(void) {
    /* Remove all layers */
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        g_lighting.layers[i].active = 0;
    }

    /* Restore original palettes if we have a backup */
    if (g_lighting.backup_valid) {
        restore_palettes();
        g_lighting.backup_valid = 0;
    }

    g_lighting.dirty = 0;
    g_lighting.backup_count = 0;
}

NGLightingLayerHandle NGLightingPush(u8 priority) {
    /* Find first available slot */
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        if (!g_lighting.layers[i].active) {
            LightingLayer *layer = &g_lighting.layers[i];
            layer->active = 1;
            layer->priority = priority;
            layer->tint_r = 0;
            layer->tint_g = 0;
            layer->tint_b = 0;
            layer->brightness = FIX_ONE;
            layer->saturation = FIX_ONE;
            layer->duration = 0;
            layer->blend_mode = NG_LIGHTING_BLEND_NORMAL;
            layer->fade_active = 0;
            layer->fade_tint_active = 0;

            /* Back up palettes on first layer activation */
            if (!g_lighting.backup_valid) {
                backup_palettes();
                g_lighting.backup_valid = 1;
            }

            g_lighting.dirty = 1;
            return i;
        }
    }
    return NG_LIGHTING_INVALID_HANDLE;
}

void NGLightingPop(NGLightingLayerHandle handle) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    g_lighting.layers[handle].active = 0;
    g_lighting.dirty = 1;

    /* Check if all layers are now inactive */
    u8 any_active = 0;
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        if (g_lighting.layers[i].active) {
            any_active = 1;
            break;
        }
    }

    /* If no layers active, restore to appropriate state */
    if (!any_active && g_lighting.backup_valid) {
        /* If a pre-baked preset is active, re-apply it */
        if (g_lighting.prebaked_handle != NG_LIGHTING_INVALID_HANDLE) {
            apply_prebaked_step(g_lighting.prebaked_preset_id, g_lighting.prebaked_current_step);
        } else {
            restore_palettes();
            g_lighting.backup_valid = 0;
        }
        g_lighting.dirty = 0;
    }
}

u8 NGLightingLayerActive(NGLightingLayerHandle handle) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return 0;
    return g_lighting.layers[handle].active;
}

u8 NGLightingGetLayerCount(void) {
    u8 count = 0;
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        if (g_lighting.layers[i].active)
            count++;
    }
    return count;
}

void NGLightingSetTint(NGLightingLayerHandle handle, s8 r, s8 g, s8 b) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    LightingLayer *layer = &g_lighting.layers[handle];
    if (layer->tint_r != r || layer->tint_g != g || layer->tint_b != b) {
        layer->tint_r = r;
        layer->tint_g = g;
        layer->tint_b = b;
        g_lighting.dirty = 1;
    }
}

void NGLightingSetBrightness(NGLightingLayerHandle handle, fixed brightness) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    LightingLayer *layer = &g_lighting.layers[handle];
    if (layer->brightness != brightness) {
        layer->brightness = brightness;
        g_lighting.dirty = 1;
    }
}

void NGLightingSetSaturation(NGLightingLayerHandle handle, fixed saturation) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    LightingLayer *layer = &g_lighting.layers[handle];
    if (layer->saturation != saturation) {
        layer->saturation = saturation;
        g_lighting.dirty = 1;
    }
}

void NGLightingSetDuration(NGLightingLayerHandle handle, u16 frames) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    g_lighting.layers[handle].duration = frames;
}

void NGLightingSetBlendMode(NGLightingLayerHandle handle, NGLightingBlendMode mode) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;

    LightingLayer *layer = &g_lighting.layers[handle];
    if (layer->blend_mode != mode) {
        layer->blend_mode = mode;
        g_lighting.dirty = 1;
    }
}

NGLightingLayerHandle NGLightingFlash(s8 r, s8 g, s8 b, u16 duration) {
    NGLightingLayerHandle handle = NGLightingPush(NG_LIGHTING_PRIORITY_TRANSIENT);
    if (handle == NG_LIGHTING_INVALID_HANDLE)
        return handle;

    NGLightingSetTint(handle, r, g, b);
    NGLightingSetBrightness(handle, FIX_FROM_FLOAT(1.3));
    NGLightingSetDuration(handle, duration);
    NGLightingSetBlendMode(handle, NG_LIGHTING_BLEND_ADDITIVE);

    return handle;
}

NGLightingLayerHandle NGLightingApplyPreset(NGLightingPreset preset) {
    NGLightingLayerHandle handle = NGLightingPush(NG_LIGHTING_PRIORITY_AMBIENT);
    if (handle == NG_LIGHTING_INVALID_HANDLE)
        return handle;

    switch (preset) {
        case NG_LIGHTING_PRESET_DAY:
            /* No modification needed - neutral */
            break;

        case NG_LIGHTING_PRESET_NIGHT:
            NGLightingSetTint(handle, -8, -5, 12);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.65));
            break;

        case NG_LIGHTING_PRESET_SUNSET:
            NGLightingSetTint(handle, 12, 4, -6);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.9));
            break;

        case NG_LIGHTING_PRESET_DAWN:
            NGLightingSetTint(handle, 6, -2, 8);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.85));
            break;

        case NG_LIGHTING_PRESET_SANDSTORM:
            NGLightingSetTint(handle, 10, 6, -4);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.85));
            NGLightingSetSaturation(handle, FIX_FROM_FLOAT(0.6));
            break;

        case NG_LIGHTING_PRESET_FOG:
            NGLightingSetTint(handle, 4, 4, 4);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.9));
            NGLightingSetSaturation(handle, FIX_FROM_FLOAT(0.4));
            break;

        case NG_LIGHTING_PRESET_UNDERWATER:
            NGLightingSetTint(handle, -6, 4, 10);
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.8));
            NGLightingSetSaturation(handle, FIX_FROM_FLOAT(0.85));
            break;

        case NG_LIGHTING_PRESET_SEPIA:
            NGLightingSetTint(handle, 8, 4, -4);
            NGLightingSetSaturation(handle, FIX_FROM_FLOAT(0.3));
            break;

        case NG_LIGHTING_PRESET_MENU_DIM:
            NGLightingSetBrightness(handle, FIX_FROM_FLOAT(0.5));
            break;
    }

    return handle;
}

void NGLightingFadeBrightness(NGLightingLayerHandle handle, fixed target, u16 frames) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;
    if (frames == 0)
        frames = 1;

    LightingLayer *layer = &g_lighting.layers[handle];
    layer->fade_active = 1;
    layer->fade_frames_remaining = frames;
    layer->fade_brightness_current = layer->brightness;
    layer->fade_brightness_target = target;
    /* Pre-compute step: (target - start) / frames, using single division upfront */
    layer->fade_brightness_step = (target - layer->brightness) / (s32)frames;
}

void NGLightingFadeTint(NGLightingLayerHandle handle, s8 r, s8 g, s8 b, u16 frames) {
    if (handle >= NG_LIGHTING_MAX_LAYERS)
        return;
    if (!g_lighting.layers[handle].active)
        return;
    if (frames == 0)
        frames = 1;

    LightingLayer *layer = &g_lighting.layers[handle];
    layer->fade_tint_active = 1;
    layer->fade_frames_remaining = frames;
    layer->fade_tint_target_r = r;
    layer->fade_tint_target_g = g;
    layer->fade_tint_target_b = b;
    /* Use 8.8 fixed point for sub-integer precision without FIX_MUL */
    layer->fade_tint_current_r = (s16)layer->tint_r << 8;
    layer->fade_tint_current_g = (s16)layer->tint_g << 8;
    layer->fade_tint_current_b = (s16)layer->tint_b << 8;
    layer->fade_tint_step_r = (s16)(((s16)r - layer->tint_r) << 8) / (s16)frames;
    layer->fade_tint_step_g = (s16)(((s16)g - layer->tint_g) << 8) / (s16)frames;
    layer->fade_tint_step_b = (s16)(((s16)b - layer->tint_b) << 8) / (s16)frames;
}

void NGLightingUpdate(void) {
    if (!g_lighting.initialized)
        return;

    u8 any_expired = 0;

    /* Update layer durations and fade animations */
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        LightingLayer *layer = &g_lighting.layers[i];
        if (!layer->active)
            continue;

        /* Process duration countdown */
        if (layer->duration > 0) {
            layer->duration--;
            if (layer->duration == 0) {
                layer->active = 0;
                any_expired = 1;
            }
        }

        /* Process fade animations - use pre-computed steps, no division */
        if (layer->active && (layer->fade_active || layer->fade_tint_active)) {
            if (layer->fade_frames_remaining > 0) {
                layer->fade_frames_remaining--;

                if (layer->fade_active) {
                    /* Incremental step - no division needed */
                    layer->fade_brightness_current += layer->fade_brightness_step;
                    layer->brightness = layer->fade_brightness_current;
                }

                if (layer->fade_tint_active) {
                    layer->fade_tint_current_r += layer->fade_tint_step_r;
                    layer->fade_tint_current_g += layer->fade_tint_step_g;
                    layer->fade_tint_current_b += layer->fade_tint_step_b;
                    layer->tint_r = (s8)(layer->fade_tint_current_r >> 8);
                    layer->tint_g = (s8)(layer->fade_tint_current_g >> 8);
                    layer->tint_b = (s8)(layer->fade_tint_current_b >> 8);
                }

                g_lighting.dirty = 1;

                /* Check if animation complete - snap to target */
                if (layer->fade_frames_remaining == 0) {
                    if (layer->fade_active) {
                        layer->brightness = layer->fade_brightness_target;
                        layer->fade_active = 0;
                    }
                    if (layer->fade_tint_active) {
                        layer->tint_r = layer->fade_tint_target_r;
                        layer->tint_g = layer->fade_tint_target_g;
                        layer->tint_b = layer->fade_tint_target_b;
                        layer->fade_tint_active = 0;
                    }
                }
            }
        }
    }

    if (any_expired) {
        g_lighting.dirty = 1;

        /* Check if all layers are now inactive */
        u8 any_active = 0;
        for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
            if (g_lighting.layers[i].active) {
                any_active = 1;
                break;
            }
        }

        if (!any_active && g_lighting.backup_valid) {
            /* If a pre-baked preset is active, re-apply it instead of
             * restoring to original palettes */
            if (g_lighting.prebaked_handle != NG_LIGHTING_INVALID_HANDLE) {
                apply_prebaked_step(g_lighting.prebaked_preset_id,
                                    g_lighting.prebaked_current_step);
            } else {
                restore_palettes();
                g_lighting.backup_valid = 0;
            }
            g_lighting.dirty = 0;
            return;
        }
    }

    /* Only resolve if something changed */
    if (g_lighting.dirty) {
        recalc_combined_transform();
        resolve_palettes();
        g_lighting.dirty = 0;
    }
}

void NGLightingInvalidate(void) {
    g_lighting.dirty = 1;
}

u8 NGLightingIsActive(void) {
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        if (g_lighting.layers[i].active)
            return 1;
    }
    return 0;
}

u8 NGLightingIsAnimating(void) {
    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        LightingLayer *layer = &g_lighting.layers[i];
        if (layer->active && (layer->fade_active || layer->fade_tint_active)) {
            return 1;
        }
    }
    return 0;
}

/* === Internal functions === */

static void backup_palettes(void) {
    /* Collect palettes from all scene objects into a bitmask */
    u8 palette_mask[32]; /* 256 bits = 256 palettes */
    for (u8 i = 0; i < 32; i++) {
        palette_mask[i] = 0;
    }

    /* Query actors, backdrops, and terrain for their palettes */
    _NGActorCollectPalettes(palette_mask);
    _NGBackdropCollectPalettes(palette_mask);
    _NGTerrainCollectPalettes(palette_mask);

    /* Convert bitmask to sparse list and backup each palette */
    g_lighting.backup_count = 0;
    for (u16 pal = 1; pal < NG_PAL_COUNT; pal++) { /* Skip palette 0 (fix layer) */
        if (palette_mask[pal >> 3] & (1 << (pal & 7))) {
            if (g_lighting.backup_count < LIGHTING_MAX_BACKUP_PALETTES) {
                PaletteBackup *entry = &g_lighting.backup[g_lighting.backup_count];
                entry->palette_index = (u8)pal;
                NGPalBackup((u8)pal, entry->colors);
                g_lighting.backup_count++;
            }
        }
    }
}

static void restore_palettes(void) {
    for (u8 i = 0; i < g_lighting.backup_count; i++) {
        PaletteBackup *entry = &g_lighting.backup[i];
        NGPalRestore(entry->palette_index, entry->colors);
    }
}

static s16 clamp_tint(s16 val) {
    if (val > 31)
        return 31;
    if (val < -31)
        return -31;
    return val;
}

static void recalc_combined_transform(void) {
    /* Sort layers by priority (simple insertion sort, max 8 layers) */
    u8 sorted[NG_LIGHTING_MAX_LAYERS];
    u8 sorted_count = 0;

    for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
        if (g_lighting.layers[i].active) {
            /* Insert in priority order */
            u8 j = sorted_count;
            while (j > 0 &&
                   g_lighting.layers[sorted[j - 1]].priority > g_lighting.layers[i].priority) {
                sorted[j] = sorted[j - 1];
                j--;
            }
            sorted[j] = i;
            sorted_count++;
        }
    }

    /* Compute combined transform */
    s16 tint_r = 0;
    s16 tint_g = 0;
    s16 tint_b = 0;
    fixed brightness = FIX_ONE;
    fixed saturation = FIX_ONE;
    s16 add_r = 0;
    s16 add_g = 0;
    s16 add_b = 0;

    for (u8 i = 0; i < sorted_count; i++) {
        LightingLayer *layer = &g_lighting.layers[sorted[i]];

        if (layer->blend_mode == NG_LIGHTING_BLEND_ADDITIVE) {
            /* Additive layers contribute to additive tint (applied after brightness) */
            add_r += layer->tint_r;
            add_g += layer->tint_g;
            add_b += layer->tint_b;
            /* Brightness is still multiplicative */
            brightness = FIX_MUL(brightness, layer->brightness);
        } else {
            /* Normal layers: tints additive, brightness/saturation multiplicative */
            tint_r += layer->tint_r;
            tint_g += layer->tint_g;
            tint_b += layer->tint_b;
            brightness = FIX_MUL(brightness, layer->brightness);
            saturation = FIX_MUL(saturation, layer->saturation);
        }
    }

    g_lighting.combined_tint_r = clamp_tint(tint_r);
    g_lighting.combined_tint_g = clamp_tint(tint_g);
    g_lighting.combined_tint_b = clamp_tint(tint_b);
    g_lighting.combined_brightness = brightness;
    g_lighting.combined_saturation = saturation;
    g_lighting.additive_tint_r = clamp_tint(add_r);
    g_lighting.additive_tint_g = clamp_tint(add_g);
    g_lighting.additive_tint_b = clamp_tint(add_b);
}

/**
 * Apply additive effects (flash) to current palettes.
 * Used when a pre-baked preset is active - applies flash on top of pre-baked colors.
 */
static void apply_additive_to_current_palettes(s16 add_r, s16 add_g, s16 add_b, u16 bright_scale) {
    for (u8 i = 0; i < g_lighting.backup_count; i++) {
        PaletteBackup *entry = &g_lighting.backup[i];
        volatile u16 *pal = NGPalGetPtr(entry->palette_index);

        for (u8 c = 1; c < NG_PAL_SIZE; c++) {
            u16 original = pal[c];

            /* Extract RGB from current palette (already has pre-baked colors) */
            u16 r = (original >> 8) & 0x0F;
            u16 g = (original >> 4) & 0x0F;
            u16 b = original & 0x0F;
            u16 d = (original >> 12) & 0x01;

            /* Expand to 5-bit range */
            r = (r + r) | d;
            g = (g + g) | d;
            b = (b + b) | d;

            /* Apply brightness */
            if (bright_scale != 256) {
                r = (u16)((r * bright_scale) >> 8);
                g = (u16)((g * bright_scale) >> 8);
                b = (u16)((b * bright_scale) >> 8);
            }

            /* Apply additive tint */
            s16 sr = (s16)r + add_r;
            s16 sg = (s16)g + add_g;
            s16 sb = (s16)b + add_b;

            /* Clamp to 0-31 */
            if (sr < 0) sr = 0;
            if (sr > 31) sr = 31;
            if (sg < 0) sg = 0;
            if (sg > 31) sg = 31;
            if (sb < 0) sb = 0;
            if (sb > 31) sb = 31;

            r = (u16)sr;
            g = (u16)sg;
            b = (u16)sb;

            /* Pack back to NeoGeo format */
            d = r & 1;
            r >>= 1;
            g >>= 1;
            b >>= 1;

            pal[c] = (u16)((d << 15) | (r << 8) | (g << 4) | b);
        }
    }
}

/**
 * Apply combined lighting transform to all backed-up palettes.
 *
 * Optimizations applied per NeoGeo dev wiki:
 * - Uses 16-bit MULU for brightness/saturation (68000 native instruction)
 * - Pre-computes all transform parameters outside inner loop
 * - Uses shift-based fixed point instead of division in loop
 * - Skips neutral transforms entirely
 * - Uses ADD for multiply-by-2 (4 cycles faster than LSL #1)
 */
static void resolve_palettes(void) {
    if (g_lighting.backup_count == 0)
        return;

    /* Check for additive effects (like flash) */
    const s16 add_r = g_lighting.additive_tint_r;
    const s16 add_g = g_lighting.additive_tint_g;
    const s16 add_b = g_lighting.additive_tint_b;
    const u16 add_bright = (u16)(g_lighting.combined_brightness >> 8);
    const u8 has_additive = (add_r != 0 || add_g != 0 || add_b != 0 || add_bright != 256);

    /* When a pre-baked preset is active, only apply additive effects (flash).
     * Pre-baked presets have correct colors computed at build time.
     * The backup contains original colors, so applying full transforms here
     * would overwrite the pre-baked colors with wrong values.
     * But additive effects can be applied on top of the current colors. */
    if (g_lighting.prebaked_handle != NG_LIGHTING_INVALID_HANDLE) {
        if (has_additive) {
            apply_additive_to_current_palettes(add_r, add_g, add_b, add_bright);
        }
        return;
    }

    /* Pre-compute combined tint (normal + additive) - moved outside all loops */
    const s16 total_tint_r = g_lighting.combined_tint_r + add_r;
    const s16 total_tint_g = g_lighting.combined_tint_g + add_g;
    const s16 total_tint_b = g_lighting.combined_tint_b + add_b;

    /* Convert fixed-point to 8-bit integer scale for FAST 16-bit multiply.
     * Scale of 256 = 1.0, so brightness 0.5 -> 128, brightness 1.3 -> 333.
     * This uses the 68000's native MULU instruction instead of 64-bit emulation. */
    const u16 bright_scale = (u16)(g_lighting.combined_brightness >> 8);
    const u16 sat_scale = (u16)(g_lighting.combined_saturation >> 8);

    /* Check if transform is neutral (no-op) - just restore if so */
    if (bright_scale == 256 && sat_scale == 256 && total_tint_r == 0 && total_tint_g == 0 &&
        total_tint_b == 0) {
        restore_palettes();
        return;
    }

    /* Determine which operations we need - checked once, not per-color */
    const u8 need_saturation = (sat_scale != 256);
    const u8 need_brightness = (bright_scale != 256);

    /* Process only the palettes that are actually in use (sparse iteration) */
    for (u8 i = 0; i < g_lighting.backup_count; i++) {
        PaletteBackup *entry = &g_lighting.backup[i];
        volatile u16 *dest = NGPalGetPtr(entry->palette_index);
        const NGColor *src = entry->colors;

        /* Process colors 1-15 (skip color 0 which is reference/transparent).
         * Each color is processed with minimal branching in the inner loop. */
        for (u8 c = 1; c < NG_PAL_SIZE; c++) {
            NGColor original = src[c];

            /* Extract RGB components using bit operations.
             * NeoGeo color format: D15=dark, D14-D12=unused, D11-D8=R, D7-D4=G, D3-D0=B */
            u16 r = (original >> 8) & 0x0F;  /* D11-D8 */
            u16 g = (original >> 4) & 0x0F;  /* D7-D4 */
            u16 b = original & 0x0F;         /* D3-D0 */
            u16 d = (original >> 12) & 0x01; /* Dark bit D15 */

            /* Expand to 5-bit (0-31 range).
             * Uses ADD for multiply-by-2: faster than LSL #1 per wiki.
             * r = (r << 1) | d  ->  r = r + r, then OR with dark bit */
            r = (r + r) | d;
            g = (g + g) | d;
            b = (b + b) | d;

            /* Apply saturation (desaturate toward gray) - integer math only.
             * Luminance coefficients: R*0.299 + G*0.587 + B*0.114
             * Scaled to integers: R*77 + G*150 + B*29 (sum = 256 for >> 8) */
            if (need_saturation) {
                u16 lum = (u16)((r * 77 + g * 150 + b * 29) >> 8);

                /* Interpolate: result = lum + ((orig - lum) * sat_scale) >> 8
                 * Uses 68000 MULS for signed multiply */
                r = (u16)(lum + ((((s16)r - (s16)lum) * (s16)sat_scale) >> 8));
                g = (u16)(lum + ((((s16)g - (s16)lum) * (s16)sat_scale) >> 8));
                b = (u16)(lum + ((((s16)b - (s16)lum) * (s16)sat_scale) >> 8));
            }

            /* Apply brightness - fast 16-bit MULU.
             * The 68000 MULU takes 38-70 cycles but is faster than
             * software multiplication or repeated shifts for larger values. */
            if (need_brightness) {
                r = (u16)((r * bright_scale) >> 8);
                g = (u16)((g * bright_scale) >> 8);
                b = (u16)((b * bright_scale) >> 8);
            }

            /* Apply tint - simple addition */
            s16 tr = (s16)r + total_tint_r;
            s16 tg = (s16)g + total_tint_g;
            s16 tb = (s16)b + total_tint_b;

            /* Clamp to valid range (0-31) using branchless-friendly comparisons.
             * The 68000 branch penalty is low, so explicit branches are fine here. */
            if (tr < 0)
                tr = 0;
            else if (tr > 31)
                tr = 31;
            if (tg < 0)
                tg = 0;
            else if (tg > 31)
                tg = 31;
            if (tb < 0)
                tb = 0;
            else if (tb > 31)
                tb = 31;

            /* Write directly to palette RAM */
            dest[c] = NG_RGB5((u8)tr, (u8)tg, (u8)tb);
        }
    }
}

/* === Pre-baked preset functions === */

/* Function pointers for pre-baked preset operations.
 * These are set by NGLightingRegisterPrebaked(), which is called
 * automatically by generated code in progear_assets.h. */
static NGLightingApplyStepFn g_prebaked_apply_fn = 0;
static NGLightingGetInfoFn g_prebaked_info_fn = 0;

void NGLightingRegisterPrebaked(NGLightingApplyStepFn apply_fn, NGLightingGetInfoFn info_fn) {
    g_prebaked_apply_fn = apply_fn;
    g_prebaked_info_fn = info_fn;
}

static void apply_prebaked_step(u8 preset_id, u8 step) {
    if (g_prebaked_apply_fn) {
        g_prebaked_apply_fn(preset_id, step);
    }
}

static const void *get_prebaked_info(u8 preset_id) {
    if (g_prebaked_info_fn) {
        return g_prebaked_info_fn(preset_id);
    }
    return 0;
}

NGLightingLayerHandle NGLightingPushPreset(u8 preset_id, u16 fade_frames) {
    if (g_lighting.prebaked_handle != NG_LIGHTING_INVALID_HANDLE) {
        return NG_LIGHTING_INVALID_HANDLE;
    }

    const void *info_ptr = get_prebaked_info(preset_id);
    if (!info_ptr) {
        return NG_LIGHTING_INVALID_HANDLE;
    }

    /* Extract fade_steps from NGLightingPresetInfo (byte after name pointer) */
    const u8 *info_bytes = (const u8 *)info_ptr;
    u8 fade_steps = info_bytes[sizeof(void *)];
    if (fade_steps == 0) {
        fade_steps = 1;
    }

    if (!g_lighting.backup_valid) {
        backup_palettes();
        g_lighting.backup_valid = 1;
    }

    g_lighting.prebaked_handle = NG_LIGHTING_MAX_LAYERS;
    g_lighting.prebaked_preset_id = preset_id;
    g_lighting.prebaked_max_steps = fade_steps;

    if (fade_frames == 0) {
        g_lighting.prebaked_fading = 0;
        g_lighting.prebaked_current_step = fade_steps;
        apply_prebaked_step(preset_id, fade_steps);
    } else {
        g_lighting.prebaked_fading = 1;
        g_lighting.prebaked_fade_out = 0;
        g_lighting.prebaked_current_step = 0;
        g_lighting.prebaked_frames_remaining = fade_frames;
        g_lighting.prebaked_frames_per_step =
            (fade_frames >= fade_steps) ? fade_frames / (u16)fade_steps : 1;
        g_lighting.prebaked_frame_counter = 0;
        apply_prebaked_step(preset_id, 0);
    }

    return g_lighting.prebaked_handle;
}

void NGLightingPopPreset(NGLightingLayerHandle handle, u16 fade_frames) {
    /* Validate handle */
    if (handle != g_lighting.prebaked_handle) {
        return; /* Not the active preset */
    }

    if (g_lighting.prebaked_handle == NG_LIGHTING_INVALID_HANDLE) {
        return; /* No preset active */
    }

    if (fade_frames == 0) {
        /* Instant restore */
        restore_palettes();
        g_lighting.prebaked_handle = NG_LIGHTING_INVALID_HANDLE;
        g_lighting.prebaked_fading = 0;
        g_lighting.backup_valid = 0;
    } else {
        /* Start fade-out animation */
        g_lighting.prebaked_fading = 1;
        g_lighting.prebaked_fade_out = 1;
        g_lighting.prebaked_frames_remaining = fade_frames;

        /* Calculate frames per step based on current position */
        u8 steps_to_fade = g_lighting.prebaked_current_step;
        if (steps_to_fade == 0) {
            steps_to_fade = 1;
        }

        if (fade_frames >= steps_to_fade) {
            g_lighting.prebaked_frames_per_step = fade_frames / (u16)steps_to_fade;
        } else {
            g_lighting.prebaked_frames_per_step = 1;
        }
        g_lighting.prebaked_frame_counter = 0;
    }
}

u8 NGLightingUpdatePrebakedFade(void) {
    if (!g_lighting.prebaked_fading) {
        return 0;
    }

    /* Increment frame counter */
    g_lighting.prebaked_frame_counter++;

    /* Check if it's time to advance to next step */
    if (g_lighting.prebaked_frame_counter >= g_lighting.prebaked_frames_per_step) {
        g_lighting.prebaked_frame_counter = 0;

        if (g_lighting.prebaked_fade_out) {
            /* Fading out - decrement step */
            if (g_lighting.prebaked_current_step > 0) {
                g_lighting.prebaked_current_step--;
                apply_prebaked_step(g_lighting.prebaked_preset_id,
                                    g_lighting.prebaked_current_step);
            }
        } else {
            /* Fading in - increment step */
            if (g_lighting.prebaked_current_step < g_lighting.prebaked_max_steps) {
                g_lighting.prebaked_current_step++;
                apply_prebaked_step(g_lighting.prebaked_preset_id,
                                    g_lighting.prebaked_current_step);
            }
        }
    }

    /* Decrement frames remaining */
    if (g_lighting.prebaked_frames_remaining > 0) {
        g_lighting.prebaked_frames_remaining--;
    }

    /* Check if fade is complete */
    if (g_lighting.prebaked_frames_remaining == 0) {
        g_lighting.prebaked_fading = 0;

        if (g_lighting.prebaked_fade_out) {
            /* Fade-out complete - restore original palettes */
            restore_palettes();
            g_lighting.prebaked_handle = NG_LIGHTING_INVALID_HANDLE;
            g_lighting.backup_valid = 0;
        } else {
            /* Fade-in complete - snap to final step */
            apply_prebaked_step(g_lighting.prebaked_preset_id, g_lighting.prebaked_max_steps);
        }
        return 0;
    }

    return 1;
}

u8 NGLightingIsPrebakedFading(void) {
    return g_lighting.prebaked_fading;
}

u8 NGLightingGetActivePreset(void) {
    if (g_lighting.prebaked_handle != NG_LIGHTING_INVALID_HANDLE) {
        return g_lighting.prebaked_preset_id;
    }
    return 0xFF;
}
