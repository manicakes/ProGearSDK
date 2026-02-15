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
 * It handles graphics hardware, input, and audio at the hardware level.
 *
 * This header includes the Core foundation library (types, math, arena) plus
 * all hardware-specific modules.
 *
 * Modules:
 * - @ref color - NeoGeo 16-bit color utilities
 * - @ref palette - Palette RAM management
 * - @ref hardware - Hardware registers and VRAM access
 * - @ref sprite - Sprite Control Block (SCB) operations
 * - @ref fix - Fix layer text rendering
 * - @ref input - Controller input handling
 * - @ref audio - ADPCM audio playback
 */

#ifndef NG_NEOGEO_HAL_H
#define NG_NEOGEO_HAL_H

/* Core foundation (types, math, arena) */
#include <neogeo_core.h>

/* Hardware access */
#include <ng_hardware.h>

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

/* Interrupt handling */
#include <ng_interrupt.h>

/* System features (DIP switches, coins, RTC) */
#include <ng_system.h>

/* Backup SRAM */
#include <ng_sram.h>

/* Memory card */
#include <ng_memcard.h>

/* BIOS calls */
#include <ng_bios.h>

#endif /* NG_NEOGEO_HAL_H */
