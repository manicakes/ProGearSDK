/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_string.h
 * @brief Minimal string/memory functions for embedded environment
 *
 * These functions are provided because GCC may generate implicit calls
 * for struct copies and array initialization.
 */

#ifndef NG_STRING_H
#define NG_STRING_H

#include <ng_types.h>

/**
 * @defgroup string String/Memory Functions
 * @ingroup core
 * @brief Minimal libc-style memory functions.
 * @{
 */

/**
 * Copy memory from source to destination
 *
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return dest
 */
void *memcpy(void *dest, const void *src, u32 n);

/**
 * Fill memory with a constant byte
 *
 * @param s Buffer to fill
 * @param c Byte value to fill with
 * @param n Number of bytes to fill
 * @return s
 */
void *memset(void *s, int c, u32 n);

/**
 * Copy memory (handles overlapping regions)
 *
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return dest
 */
void *memmove(void *dest, const void *src, u32 n);

/**
 * Compare two null-terminated strings for equality.
 *
 * @param a First string
 * @param b Second string
 * @return 1 if equal, 0 if not
 */
static inline u8 ng_str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}

/** @} */ /* end of string group */

#endif /* NG_STRING_H */
