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

    /* The 68000 can only do word/long access on even addresses, so the fast
     * long-word path is available only when src and dest share parity. */
    if ((((u32)d ^ (u32)s) & 1) == 0) {
        if (((u32)d & 1) && n) { /* step to an even boundary */
            *d++ = *s++;
            n--;
        }
        u32 *dl = (u32 *)d;
        const u32 *sl = (const u32 *)s;
        for (u32 longs = n >> 2; longs; longs--) {
            *dl++ = *sl++;
        }
        d = (u8 *)dl;
        s = (const u8 *)sl;
        n &= 3;
    }

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
    u8 byte = (u8)c;

    /* For larger fills, write 32 bits at a time (the 68000 needs even
     * alignment for long access). */
    if (n >= 8) {
        if ((u32)p & 1) { /* step to an even boundary */
            *p++ = byte;
            n--;
        }
        u32 word = byte;
        word |= word << 8;
        word |= word << 16; /* replicate the byte across all four lanes */
        u32 *pl = (u32 *)p;
        for (u32 longs = n >> 2; longs; longs--) {
            *pl++ = word;
        }
        p = (u8 *)pl;
        n &= 3;
    }

    while (n--) {
        *p++ = byte;
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
