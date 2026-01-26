/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_sram.c
 * @brief NeoGeo backup SRAM implementation
 *
 * SRAM access notes:
 * - SRAM is at 0xD00000-0xD0FFFF (64KB address space)
 * - Only odd bytes are accessible (8-bit data bus)
 * - Effective storage is 32KB
 * - Write protection controlled via REG_SRAMLOCK (0x380011)
 */

#include <ng_sram.h>

/* SRAM control register */
#define REG_SRAM_LOCK   (*(vu8 *)0x380011)
#define REG_SRAM_UNLOCK (*(vu8 *)0x3A0001)

/* SRAM base pointer (odd bytes only) */
#define SRAM_BASE ((vu8 *)0xD00001)

/* Track lock state */
static u8 sram_unlocked = 0;

/* ============================================================================
 * SRAM Protection
 * ========================================================================== */

void NGSramUnlock(void) {
    /* Write to unlock register enables SRAM writes */
    REG_SRAM_UNLOCK = 1;
    sram_unlocked = 1;
}

void NGSramLock(void) {
    /* Write to lock register disables SRAM writes */
    REG_SRAM_LOCK = 1;
    sram_unlocked = 0;
}

u8 NGSramIsUnlocked(void) {
    return sram_unlocked;
}

/* ============================================================================
 * Byte Access
 * ========================================================================== */

u8 NGSramReadByte(u16 offset) {
    /* Each logical byte is at offset*2 in SRAM address space (odd bytes) */
    return SRAM_BASE[offset * 2];
}

void NGSramWriteByte(u16 offset, u8 value) {
    if (!sram_unlocked)
        return;
    SRAM_BASE[offset * 2] = value;
}

/* ============================================================================
 * Word/Long Access
 * ========================================================================== */

u16 NGSramReadWord(u16 offset) {
    u16 value;
    value = (u16)NGSramReadByte(offset) << 8;
    value |= NGSramReadByte((u16)(offset + 1));
    return value;
}

void NGSramWriteWord(u16 offset, u16 value) {
    NGSramWriteByte(offset, (u8)(value >> 8));
    NGSramWriteByte((u16)(offset + 1), (u8)(value & 0xFF));
}

u32 NGSramReadLong(u16 offset) {
    u32 value;
    value = (u32)NGSramReadWord(offset) << 16;
    value |= NGSramReadWord((u16)(offset + 2));
    return value;
}

void NGSramWriteLong(u16 offset, u32 value) {
    NGSramWriteWord(offset, (u16)(value >> 16));
    NGSramWriteWord((u16)(offset + 2), (u16)(value & 0xFFFF));
}

/* ============================================================================
 * Block Access
 * ========================================================================== */

void NGSramReadBlock(u16 offset, void *buffer, u16 length) {
    u8 *dst = (u8 *)buffer;
    u16 i;

    for (i = 0; i < length; i++) {
        dst[i] = NGSramReadByte((u16)(offset + i));
    }
}

void NGSramWriteBlock(u16 offset, const void *buffer, u16 length) {
    const u8 *src = (const u8 *)buffer;
    u16 i;

    if (!sram_unlocked)
        return;

    for (i = 0; i < length; i++) {
        NGSramWriteByte((u16)(offset + i), src[i]);
    }
}

/* ============================================================================
 * Checksum Utilities
 * ========================================================================== */

u16 NGSramChecksum(u16 offset, u16 length) {
    u16 checksum = 0;
    u16 i;

    for (i = 0; i < length; i++) {
        u8 byte = NGSramReadByte((u16)(offset + i));
        /* Rotate and XOR for better distribution */
        checksum = (u16)((checksum << 1) | (checksum >> 15));
        checksum ^= byte;
    }

    return checksum;
}

u8 NGSramVerify(u16 offset, u16 length, u16 expected) {
    return NGSramChecksum(offset, length) == expected ? 1 : 0;
}

void NGSramClear(u16 offset, u16 length) {
    u16 i;

    if (!sram_unlocked)
        return;

    for (i = 0; i < length; i++) {
        NGSramWriteByte((u16)(offset + i), 0);
    }
}
