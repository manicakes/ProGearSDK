/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file neogeo_core.h
 * @brief Master header for NeoGeo Core Foundation Library.
 *
 * Include this single header to access all core functionality:
 * - Fixed-width integer types
 * - Fixed-point math and trigonometry
 * - Arena memory allocator
 *
 * This library contains foundational utilities with no hardware dependencies.
 * It can be used by both the HAL and SDK layers.
 */

/**
 * @defgroup core Core - Foundation Library
 * @brief Foundational types, math, and memory utilities.
 *
 * The Core library provides platform-independent foundations used throughout
 * the SDK. These modules have no hardware dependencies.
 *
 * Modules:
 * - @ref types - Fixed-width integer types (u8, u16, u32, etc.)
 * - @ref math - Fixed-point math, vectors, and trigonometry
 * - @ref arena - Bump-pointer arena memory allocator
 */

#ifndef _NEOGEO_CORE_H_
#define _NEOGEO_CORE_H_

/* Foundation types */
#include <ng_types.h>

/* Fixed-point math */
#include <ng_math.h>

/* Memory management */
#include <ng_arena.h>

/* String/memory functions (for compiler-generated calls) */
#include <ng_string.h>

#endif /* _NEOGEO_CORE_H_ */
