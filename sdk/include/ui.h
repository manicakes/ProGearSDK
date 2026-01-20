/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ui.h
 * @brief UI system for menus and dialogs.
 *
 * The UI system provides sprite-based, animated interface elements.
 * UI elements are positioned in VIEWPORT coordinates (screen-relative),
 * not world coordinates. They appear at a high Z-index, always on top.
 *
 * The system uses spring physics for smooth, modern-feeling animations:
 * - Menus slide in/out with spring motion
 * - Selection cursor animates between items with overshoot
 * - Background dimming via palette fade (no extra sprites!)
 * - All animations are snappy (~200-300ms)
 */

#ifndef UI_H
#define UI_H

#include <types.h>
#include <arena.h>
#include <spring.h>
#include <visual.h>

/** @defgroup uiconst UI Constants
 *  @{
 */

#define MENU_MAX_ITEMS 12  /**< Maximum items per menu */
#define MENU_Z_INDEX   250 /**< High Z to render on top of game */

/** @} */

/** @defgroup menuhandle Menu Handle
 *  @{
 */

/** Menu handle type */
typedef struct MenuData *MenuHandle;

/** @} */

/** @defgroup menucreate Menu Creation
 *  @{
 */

/**
 * Create a menu using arena allocation.
 * @param arena Arena to allocate from (typically arena_state)
 * @param panel_asset Visual asset for menu background panel
 * @param cursor_asset Visual asset for selection cursor
 * @param dim_amount Background dimming intensity (0=none, 1-31=darken)
 *                   Uses palette manipulation instead of sprites,
 *                   so it doesn't consume sprite resources.
 * @return Menu handle, or NULL if allocation failed
 */
MenuHandle MenuCreate(Arena *arena, const VisualAsset *panel_asset, const VisualAsset *cursor_asset,
                      u8 dim_amount);

/** @} */

/** @defgroup menuconfig Menu Configuration
 *  @{
 */

/**
 * Set menu title (displayed at top of menu).
 * @param menu Menu handle
 * @param title Title string (NULL for no title)
 */
void MenuSetTitle(MenuHandle menu, const char *title);

/**
 * Set menu position in viewport coordinates.
 * Position is the top-left corner of the panel.
 * @param menu Menu handle
 * @param viewport_x X position relative to screen left edge
 * @param viewport_y Y position relative to screen top edge
 */
void MenuSetPosition(MenuHandle menu, s16 viewport_x, s16 viewport_y);

/**
 * Set text palette for menu items.
 * @param menu Menu handle
 * @param normal_pal Palette index for normal items (fix layer palette, 0-15)
 * @param selected_pal Palette index for selected item (fix layer palette, 0-15)
 */
void MenuSetTextPalette(MenuHandle menu, u8 normal_pal, u8 selected_pal);

/**
 * Add an item to the menu.
 * @param menu Menu handle
 * @param label Item text
 * @return Item index (0-based), or 0xFF if menu is full
 */
u8 MenuAddItem(MenuHandle menu, const char *label);

/**
 * Add a non-selectable separator to the menu.
 * The cursor will skip over separators when navigating.
 * @param menu Menu handle
 * @param label Separator text (e.g., "--------")
 * @return Item index (0-based), or 0xFF if menu is full
 */
u8 MenuAddSeparator(MenuHandle menu, const char *label);

/**
 * Change a menu item's text.
 * @param menu Menu handle
 * @param index Item index (0-based)
 * @param label New item text
 */
void MenuSetItemText(MenuHandle menu, u8 index, const char *label);

/**
 * Set sound effects for menu interactions.
 * @param menu Menu handle
 * @param move_sfx SFX index to play when cursor moves (0xFF = none)
 * @param select_sfx SFX index to play when item is selected (0xFF = none)
 */
void MenuSetSounds(MenuHandle menu, u8 move_sfx, u8 select_sfx);

/** @} */

/** @defgroup menulife Menu Lifecycle
 *  @{
 */

/**
 * Show the menu with entrance animation.
 * Menu slides in from off-screen.
 * @param menu Menu handle
 */
void MenuShow(MenuHandle menu);

/**
 * Hide the menu with exit animation.
 * Menu slides out to off-screen.
 * @param menu Menu handle
 */
void MenuHide(MenuHandle menu);

/**
 * Update menu state (call once per frame).
 * Handles input navigation and spring animations.
 * @param menu Menu handle
 */
void MenuUpdate(MenuHandle menu);

/**
 * Check if menu needs a fix layer VRAM update.
 * Fix layer tiles persist once written, so we only need to write
 * when text first appears or when content changes (selection, blink).
 * @param menu Menu handle
 * @return 1 if VRAM update needed, 0 otherwise
 */
u8 MenuNeedsDraw(MenuHandle menu);

/**
 * Draw menu to screen (call during vblank when MenuNeedsDraw returns true).
 * Updates fix layer text. Only writes to VRAM when content has changed.
 * @param menu Menu handle
 */
void MenuDraw(MenuHandle menu);

/** @} */

/** @defgroup menustate Menu State Queries
 *  @{
 */

/**
 * Check if menu is visible (shown and not fully hidden).
 * @param menu Menu handle
 * @return 1 if visible, 0 if hidden
 */
u8 MenuIsVisible(MenuHandle menu);

/**
 * Check if menu is currently animating (entrance/exit).
 * @param menu Menu handle
 * @return 1 if animating, 0 if settled
 */
u8 MenuIsAnimating(MenuHandle menu);

/**
 * Get currently selected item index.
 * @param menu Menu handle
 * @return Selected item index (0-based)
 */
u8 MenuGetSelection(MenuHandle menu);

/**
 * Set selected item index.
 * @param menu Menu handle
 * @param index Item index to select
 */
void MenuSetSelection(MenuHandle menu, u8 index);

/**
 * Check if user confirmed selection (pressed A).
 * Clears the flag after reading.
 * @param menu Menu handle
 * @return 1 if confirmed this frame, 0 otherwise
 */
u8 MenuConfirmed(MenuHandle menu);

/**
 * Check if user cancelled (pressed B).
 * Clears the flag after reading.
 * @param menu Menu handle
 * @return 1 if cancelled this frame, 0 otherwise
 */
u8 MenuCancelled(MenuHandle menu);

/** @} */

/** @defgroup menuclean Cleanup
 *  @{
 */

/**
 * Destroy menu and free resources.
 * Removes sprites from scene and clears fix layer text.
 * @param menu Menu handle
 */
void MenuDestroy(MenuHandle menu);

/** @} */

#endif /* UI_H */
