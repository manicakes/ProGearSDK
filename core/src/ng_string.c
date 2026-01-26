/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_string.c
 * @brief Minimal string/memory functions for embedded environment
 *
 * GCC may generate implicit calls to these functions for struct copies,
 * array initialization, etc. We provide minimal implementations.
 */

#include <ng_types.h>

/**
 * Copy memory from source to destination
 *
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return dest
 */
void *memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

/**
 * Fill memory with a constant byte
 *
 * @param s Buffer to fill
 * @param c Byte value to fill with
 * @param n Number of bytes to fill
 * @return s
 */
void *memset(void *s, int c, u32 n) {
    u8 *p = (u8 *)s;

    while (n--) {
        *p++ = (u8)c;
    }

    return s;
}

/**
 * Copy memory (may overlap)
 *
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return dest
 */
void *memmove(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    if (d < s) {
        /* Copy forward */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Copy backward */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}
