/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_memcard.c
 * @brief NeoGeo memory card implementation
 *
 * Memory card access notes:
 * - Cards are at 0x800000-0x8FFFFF (directly mapped)
 * - Only even bytes are accessible (word-wide bus, upper byte)
 * - Card detection via presence detect line
 * - Write-protect detection via hardware line
 */

#include <ng_memcard.h>
#include <ng_hardware.h>

/* Memory card registers */
#define MEMCARD_BASE    ((vu8 *)0x800000)
#define REG_CARD_STATUS (*(vu8 *)0x380021) /* Card status bits */

/* Status register bits */
#define CARD_PRESENT    0x01 /* Card inserted (active low) */
#define CARD_WRITE_PROT 0x02 /* Write protect (active low) */

/* Card signature for formatted cards */
#define CARD_SIGNATURE_OFFSET 0x0000
#define CARD_SIGNATURE        "NEO-GEO"
#define CARD_SIGNATURE_LEN    7

/* ============================================================================
 * Card Detection
 * ========================================================================== */

u8 NGMemcardIsPresent(void) {
    /* Card present line is active-low */
    return (REG_CARD_STATUS & CARD_PRESENT) ? 0 : 1;
}

u8 NGMemcardIsWriteProtected(void) {
    if (!NGMemcardIsPresent())
        return 1; /* No card = treat as protected */
    /* Write protect line is active-low */
    return (REG_CARD_STATUS & CARD_WRITE_PROT) ? 0 : 1;
}

/* ============================================================================
 * Card Access
 * ========================================================================== */

u8 NGMemcardReadByte(u16 offset) {
    if (!NGMemcardIsPresent())
        return 0xFF;

    /* Each logical byte is at offset*2 (even addresses only) */
    return MEMCARD_BASE[offset * 2];
}

u8 NGMemcardWriteByte(u16 offset, u8 value) {
    if (!NGMemcardIsPresent())
        return 0;
    if (NGMemcardIsWriteProtected())
        return 0;

    MEMCARD_BASE[offset * 2] = value;
    return 1;
}

u16 NGMemcardRead(u16 offset, void *buffer, u16 length) {
    u8 *dst = (u8 *)buffer;
    u16 i;

    if (!NGMemcardIsPresent())
        return 0;

    for (i = 0; i < length; i++) {
        dst[i] = NGMemcardReadByte((u16)(offset + i));
    }

    return length;
}

u16 NGMemcardWrite(u16 offset, const void *buffer, u16 length) {
    const u8 *src = (const u8 *)buffer;
    u16 i;

    if (!NGMemcardIsPresent())
        return 0;
    if (NGMemcardIsWriteProtected())
        return 0;

    for (i = 0; i < length; i++) {
        if (!NGMemcardWriteByte((u16)(offset + i), src[i]))
            return i;
    }

    return length;
}

/* ============================================================================
 * Card Format / Initialization
 * ========================================================================== */

u8 NGMemcardIsFormatted(void) {
    u8 sig[CARD_SIGNATURE_LEN];
    u16 i;

    if (!NGMemcardIsPresent())
        return 0;

    /* Read signature */
    NGMemcardRead(CARD_SIGNATURE_OFFSET, sig, CARD_SIGNATURE_LEN);

    /* Compare with expected signature */
    for (i = 0; i < CARD_SIGNATURE_LEN; i++) {
        if (sig[i] != (u8)CARD_SIGNATURE[i])
            return 0;
    }

    return 1;
}

u8 NGMemcardFormat(void) {
    u16 i;

    if (!NGMemcardIsPresent())
        return 0;
    if (NGMemcardIsWriteProtected())
        return 0;

    /* Write signature */
    for (i = 0; i < CARD_SIGNATURE_LEN; i++) {
        if (!NGMemcardWriteByte((u16)(CARD_SIGNATURE_OFFSET + i), (u8)CARD_SIGNATURE[i]))
            return 0;
    }

    /* Clear rest of card header (first 256 bytes) */
    for (i = CARD_SIGNATURE_LEN; i < 256; i++) {
        NGMemcardWriteByte(i, 0x00);
    }

    return 1;
}

u16 NGMemcardGetSize(void) {
    if (!NGMemcardIsPresent())
        return 0;

    /* Standard NeoGeo memory cards are 2KB (usable)
     * Some third-party cards may be larger */
    return 0x800; /* 2KB default */
}
