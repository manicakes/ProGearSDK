/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file neogeo_hal.h
 * @brief Master header for NeoGeo Hardware Abstraction Layer.
 *
 * Include this single header to access all HAL functionality:
 * - Types and hardware registers
 * - Fixed-point math
 * - Color and palette management
 * - Sprite hardware access
 * - Fix layer (text) rendering
 * - Input handling
 * - Audio playback
 * - Arena memory allocator
 */

#ifndef _NEOGEO_HAL_H_
#define _NEOGEO_HAL_H_

/* Foundation types */
#include <ng_types.h>

/* Hardware access */
#include <ng_hardware.h>

/* Fixed-point math */
#include <ng_math.h>

/* Color and palette */
#include <ng_color.h>
#include <ng_palette.h>

/* Sprite hardware */
#include <ng_sprite.h>

/* Fix layer (text) */
#include <ng_fix.h>

/* Input handling */
#include <ng_input.h>

/* Audio system */
#include <ng_audio.h>

/* Memory management */
#include <ng_arena.h>

#endif /* _NEOGEO_HAL_H_ */
