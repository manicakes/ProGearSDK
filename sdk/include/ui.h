/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ui.h - UI system for menus and dialogs
//
// The UI system provides sprite-based, animated interface elements.
// UI elements are positioned in VIEWPORT coordinates (screen-relative),
// not world coordinates. They appear at a high Z-index, always on top.
//
// The system uses spring physics for smooth, modern-feeling animations:
// - Menus slide in/out with spring motion
// - Selection cursor animates between items with overshoot
// - Background dimming via palette fade (no extra sprites!)
// - All animations are snappy (~200-300ms)

#ifndef UI_H
#define UI_H

#include <types.h>
#include <arena.h>
#include <spring.h>
#include <visual.h>

// === Configuration ===

#define NG_MENU_MAX_ITEMS    12   // Maximum items per menu
#define NG_MENU_Z_INDEX    250    // High Z to render on top of game

// === Menu Handle ===

typedef struct NGMenu *NGMenuHandle;

// === Menu Creation ===

/**
 * Create a menu using arena allocation.
 * @param arena Arena to allocate from (typically ng_arena_state)
 * @param panel_asset Visual asset for menu background panel
 * @param cursor_asset Visual asset for selection cursor
 * @param dim_amount Background dimming intensity (0=none, 1-31=darken)
 *                   Uses palette manipulation instead of sprites,
 *                   so it doesn't consume sprite resources.
 * @return Menu handle, or NULL if allocation failed
 */
NGMenuHandle NGMenuCreate(NGArena *arena,
                          const NGVisualAsset *panel_asset,
                          const NGVisualAsset *cursor_asset,
                          u8 dim_amount);

// === Menu Configuration ===

/**
 * Set menu title (displayed at top of menu).
 * @param menu Menu handle
 * @param title Title string (NULL for no title)
 */
void NGMenuSetTitle(NGMenuHandle menu, const char *title);

/**
 * Set menu position in viewport coordinates.
 * Position is the top-left corner of the panel.
 * @param menu Menu handle
 * @param viewport_x X position relative to screen left edge
 * @param viewport_y Y position relative to screen top edge
 */
void NGMenuSetPosition(NGMenuHandle menu, s16 viewport_x, s16 viewport_y);

/**
 * Set text palette for menu items.
 * @param menu Menu handle
 * @param normal_pal Palette index for normal items (fix layer palette, 0-15)
 * @param selected_pal Palette index for selected item (fix layer palette, 0-15)
 */
void NGMenuSetTextPalette(NGMenuHandle menu, u8 normal_pal, u8 selected_pal);

/**
 * Add an item to the menu.
 * @param menu Menu handle
 * @param label Item text
 * @return Item index (0-based), or 0xFF if menu is full
 */
u8 NGMenuAddItem(NGMenuHandle menu, const char *label);

/**
 * Add a non-selectable separator to the menu.
 * The cursor will skip over separators when navigating.
 * @param menu Menu handle
 * @param label Separator text (e.g., "--------")
 * @return Item index (0-based), or 0xFF if menu is full
 */
u8 NGMenuAddSeparator(NGMenuHandle menu, const char *label);

/**
 * Change a menu item's text.
 * @param menu Menu handle
 * @param index Item index (0-based)
 * @param label New item text
 */
void NGMenuSetItemText(NGMenuHandle menu, u8 index, const char *label);

/**
 * Set sound effects for menu interactions.
 * @param menu Menu handle
 * @param move_sfx SFX index to play when cursor moves (0xFF = none)
 * @param select_sfx SFX index to play when item is selected (0xFF = none)
 */
void NGMenuSetSounds(NGMenuHandle menu, u8 move_sfx, u8 select_sfx);

// === Menu Lifecycle ===

/**
 * Show the menu with entrance animation.
 * Menu slides in from off-screen.
 * @param menu Menu handle
 */
void NGMenuShow(NGMenuHandle menu);

/**
 * Hide the menu with exit animation.
 * Menu slides out to off-screen.
 * @param menu Menu handle
 */
void NGMenuHide(NGMenuHandle menu);

/**
 * Update menu state (call once per frame).
 * Handles input navigation and spring animations.
 * @param menu Menu handle
 */
void NGMenuUpdate(NGMenuHandle menu);

/**
 * Draw menu to screen (call once per frame during vblank).
 * Renders panel sprite and updates fix layer text.
 * @param menu Menu handle
 */
void NGMenuDraw(NGMenuHandle menu);

// === Menu State Queries ===

/**
 * Check if menu is visible (shown and not fully hidden).
 * @param menu Menu handle
 * @return 1 if visible, 0 if hidden
 */
u8 NGMenuIsVisible(NGMenuHandle menu);

/**
 * Check if menu is currently animating (entrance/exit).
 * @param menu Menu handle
 * @return 1 if animating, 0 if settled
 */
u8 NGMenuIsAnimating(NGMenuHandle menu);

/**
 * Get currently selected item index.
 * @param menu Menu handle
 * @return Selected item index (0-based)
 */
u8 NGMenuGetSelection(NGMenuHandle menu);

/**
 * Set selected item index.
 * @param menu Menu handle
 * @param index Item index to select
 */
void NGMenuSetSelection(NGMenuHandle menu, u8 index);

/**
 * Check if user confirmed selection (pressed A).
 * Clears the flag after reading.
 * @param menu Menu handle
 * @return 1 if confirmed this frame, 0 otherwise
 */
u8 NGMenuConfirmed(NGMenuHandle menu);

/**
 * Check if user cancelled (pressed B).
 * Clears the flag after reading.
 * @param menu Menu handle
 * @return 1 if cancelled this frame, 0 otherwise
 */
u8 NGMenuCancelled(NGMenuHandle menu);

// === Cleanup ===

/**
 * Destroy menu and free resources.
 * Removes sprites from scene and clears fix layer text.
 * @param menu Menu handle
 */
void NGMenuDestroy(NGMenuHandle menu);

#endif // UI_H
