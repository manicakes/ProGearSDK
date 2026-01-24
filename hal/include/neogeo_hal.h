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

/**
 * @defgroup hal HAL - Hardware Abstraction Layer
 * @brief Low-level NeoGeo hardware access and utilities.
 *
 * The HAL provides direct access to NeoGeo hardware with portable abstractions.
 * It handles types, math, graphics hardware, input, and audio at the hardware level.
 *
 * Modules:
 * - @ref types - Fixed-width integer types
 * - @ref math - Fixed-point math and trigonometry
 * - @ref color - NeoGeo 16-bit color utilities
 * - @ref palette - Palette RAM management
 * - @ref hardware - Hardware registers and VRAM access
 * - @ref sprite - Sprite Control Block (SCB) operations
 * - @ref fix - Fix layer text rendering
 * - @ref input - Controller input handling
 * - @ref audio - ADPCM audio playback
 * - @ref arena - Arena memory allocator
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
