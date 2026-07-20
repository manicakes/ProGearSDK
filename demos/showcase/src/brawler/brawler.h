/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef BRAWLER_DEMO_H
#define BRAWLER_DEMO_H

#include <ng_types.h>

/* Entity pools and depth sorting: a floor plane where walking up and down
 * changes who draws in front. */
void BrawlerInit(void);
u8 BrawlerUpdate(void);
void BrawlerCleanup(void);

#endif // BRAWLER_DEMO_H
