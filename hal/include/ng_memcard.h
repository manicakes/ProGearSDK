/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_memcard.h
 * @brief NeoGeo memory card support
 *
 * The NeoGeo memory card provides portable save data storage.
 * Cards are accessed through a serial protocol at 0x800000-0x8FFFFF.
 *
 * Memory cards have limited space (typically 2KB-8KB usable).
 * Each game should claim a small slot for its save data.
 *
 * Note: Memory card access is slow - avoid during gameplay.
 *
 * Usage example:
 * @code
 * if (NGMemcardIsPresent()) {
 *     NGMemcardRead(0x0000, save_data, sizeof(save_data));
 * }
 * @endcode
 */

#ifndef _NG_MEMCARD_H_
#define _NG_MEMCARD_H_

#include <ng_types.h>

/**
 * @defgroup memcard Memory Card
 * @ingroup hal
 * @brief Portable save data via memory card.
 * @{
 */

/* Memory card constants */
#define NG_MEMCARD_BASE     0x800000 /**< Memory card base address */
#define NG_MEMCARD_MAX_SIZE 0x2000   /**< Maximum card size (8KB) */

/* ============================================================================
 * Card Detection
 * ========================================================================== */

/**
 * Check if a memory card is present
 *
 * @return 1 if card present, 0 if no card
 */
u8 NGMemcardIsPresent(void);

/**
 * Check if memory card is write-protected
 *
 * @return 1 if write-protected, 0 if writable
 */
u8 NGMemcardIsWriteProtected(void);

/* ============================================================================
 * Card Access
 * ========================================================================== */

/**
 * Read data from memory card
 *
 * @param offset Offset into card (0 to card size)
 * @param buffer Destination buffer
 * @param length Number of bytes to read
 * @return Number of bytes actually read, 0 on error
 */
u16 NGMemcardRead(u16 offset, void *buffer, u16 length);

/**
 * Write data to memory card
 *
 * @param offset Offset into card
 * @param buffer Source buffer
 * @param length Number of bytes to write
 * @return Number of bytes actually written, 0 on error
 */
u16 NGMemcardWrite(u16 offset, const void *buffer, u16 length);

/**
 * Read a single byte from memory card
 *
 * @param offset Offset into card
 * @return Byte value, or 0xFF on error
 */
u8 NGMemcardReadByte(u16 offset);

/**
 * Write a single byte to memory card
 *
 * @param offset Offset into card
 * @param value Byte value to write
 * @return 1 on success, 0 on error
 */
u8 NGMemcardWriteByte(u16 offset, u8 value);

/* ============================================================================
 * Card Format / Initialization
 * ========================================================================== */

/**
 * Check if memory card is formatted
 * Looks for NeoGeo card signature.
 *
 * @return 1 if formatted, 0 if unformatted or corrupt
 */
u8 NGMemcardIsFormatted(void);

/**
 * Format memory card
 * WARNING: This erases all data on the card!
 *
 * @return 1 on success, 0 on error
 */
u8 NGMemcardFormat(void);

/**
 * Get memory card size in bytes
 *
 * @return Card size, or 0 if no card/error
 */
u16 NGMemcardGetSize(void);

/** @} */ /* end of memcard group */

#endif /* _NG_MEMCARD_H_ */
