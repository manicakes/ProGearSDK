/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_sram.h
 * @brief NeoGeo backup RAM (SRAM) save/load functionality
 *
 * The NeoGeo has 64KB of backup SRAM at 0xD00000-0xD0FFFF.
 * Only odd bytes are usable (8-bit data bus), giving 32KB effective storage.
 *
 * SRAM is write-protected by default. Use NGSramUnlock() before writing
 * and NGSramLock() after to prevent accidental corruption.
 *
 * Usage example:
 * @code
 * // Save high score
 * NGSramUnlock();
 * NGSramWriteWord(0x0000, high_score);
 * NGSramLock();
 *
 * // Load high score
 * high_score = NGSramReadWord(0x0000);
 * @endcode
 */

#ifndef NG_SRAM_H
#define NG_SRAM_H

#include <ng_types.h>

/**
 * @defgroup sram Backup SRAM
 * @ingroup hal
 * @brief Persistent save data storage.
 * @{
 */

/* SRAM address range */
#define NG_SRAM_BASE 0xD00000 /**< SRAM base address */
#define NG_SRAM_SIZE 0x10000  /**< Total SRAM size (64KB address space) */
#define NG_SRAM_EFFECTIVE_SIZE 0x8000 /**< Usable bytes (32KB, odd bytes only) */

/* ============================================================================
 * SRAM Protection
 * ========================================================================== */

/**
 * Unlock SRAM for writing
 * Must be called before any write operations.
 * Call NGSramLock() when done to re-enable write protection.
 */
void NGSramUnlock(void);

/**
 * Lock SRAM (enable write protection)
 * Prevents accidental writes to save data.
 */
void NGSramLock(void);

/**
 * Check if SRAM is currently unlocked
 *
 * @return 1 if unlocked, 0 if locked
 */
u8 NGSramIsUnlocked(void);

/* ============================================================================
 * Byte Access
 * ========================================================================== */

/**
 * Read a byte from SRAM
 *
 * @param offset Offset into SRAM (0 to 0x7FFF)
 * @return Byte value at that offset
 */
u8 NGSramReadByte(u16 offset);

/**
 * Write a byte to SRAM
 * SRAM must be unlocked first.
 *
 * @param offset Offset into SRAM (0 to 0x7FFF)
 * @param value Byte value to write
 */
void NGSramWriteByte(u16 offset, u8 value);

/* ============================================================================
 * Word/Long Access
 * ========================================================================== */

/**
 * Read a 16-bit word from SRAM
 *
 * @param offset Offset into SRAM (0 to 0x7FFE, must be even)
 * @return 16-bit value
 */
u16 NGSramReadWord(u16 offset);

/**
 * Write a 16-bit word to SRAM
 * SRAM must be unlocked first.
 *
 * @param offset Offset into SRAM (0 to 0x7FFE, must be even)
 * @param value 16-bit value to write
 */
void NGSramWriteWord(u16 offset, u16 value);

/**
 * Read a 32-bit long from SRAM
 *
 * @param offset Offset into SRAM (0 to 0x7FFC, must be even)
 * @return 32-bit value
 */
u32 NGSramReadLong(u16 offset);

/**
 * Write a 32-bit long to SRAM
 * SRAM must be unlocked first.
 *
 * @param offset Offset into SRAM (0 to 0x7FFC, must be even)
 * @param value 32-bit value to write
 */
void NGSramWriteLong(u16 offset, u32 value);

/* ============================================================================
 * Block Access
 * ========================================================================== */

/**
 * Read a block of data from SRAM
 *
 * @param offset Offset into SRAM
 * @param buffer Destination buffer
 * @param length Number of bytes to read
 */
void NGSramReadBlock(u16 offset, void *buffer, u16 length);

/**
 * Write a block of data to SRAM
 * SRAM must be unlocked first.
 *
 * @param offset Offset into SRAM
 * @param buffer Source buffer
 * @param length Number of bytes to write
 */
void NGSramWriteBlock(u16 offset, const void *buffer, u16 length);

/* ============================================================================
 * Checksum Utilities
 * ========================================================================== */

/**
 * Calculate checksum of SRAM region
 * Uses simple XOR-based checksum for quick validation.
 *
 * @param offset Start offset
 * @param length Number of bytes to checksum
 * @return 16-bit checksum
 */
u16 NGSramChecksum(u16 offset, u16 length);

/**
 * Verify SRAM region against expected checksum
 *
 * @param offset Start offset
 * @param length Number of bytes to verify
 * @param expected Expected checksum value
 * @return 1 if checksum matches, 0 if corrupted
 */
u8 NGSramVerify(u16 offset, u16 length, u16 expected);

/**
 * Clear SRAM region to zero
 * SRAM must be unlocked first.
 *
 * @param offset Start offset
 * @param length Number of bytes to clear
 */
void NGSramClear(u16 offset, u16 length);

/** @} */ /* end of sram group */

#endif /* NG_SRAM_H */
