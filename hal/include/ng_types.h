/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_types.h
 * @brief Basic type definitions for NeoGeo HAL.
 *
 * Provides fixed-width integer types and volatile variants for hardware access.
 */

#ifndef _NG_TYPES_H_
#define _NG_TYPES_H_

#include <stdint.h>

/** @defgroup types Basic Types
 *  @brief Fixed-width integer types.
 *  @{
 */

typedef uint8_t u8;   /**< Unsigned 8-bit integer */
typedef uint16_t u16; /**< Unsigned 16-bit integer */
typedef uint32_t u32; /**< Unsigned 32-bit integer */
typedef int8_t s8;    /**< Signed 8-bit integer */
typedef int16_t s16;  /**< Signed 16-bit integer */
typedef int32_t s32;  /**< Signed 32-bit integer */

/** @} */

/** @defgroup voltypes Volatile Types
 *  @brief For hardware register access.
 *  @{
 */

typedef volatile u8 vu8;   /**< Volatile unsigned 8-bit */
typedef volatile u16 vu16; /**< Volatile unsigned 16-bit */
typedef volatile u32 vu32; /**< Volatile unsigned 32-bit */

/** @} */

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif // _NG_TYPES_H_
