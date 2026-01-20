/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file menu.h
 * @brief Animated menu system.
 *
 * Create beautiful menus with spring-animated panels and cursors.
 * Handles input, selection, and visual feedback automatically.
 *
 * @example
 * // Create a menu
 * Menu menu = MenuCreate(arena, &panel_asset, &cursor_asset);
 * MenuSetTitle(menu, "MAIN MENU");
 * MenuAddItem(menu, "START GAME");
 * MenuAddItem(menu, "OPTIONS");
 * MenuAddItem(menu, "EXIT");
 * MenuSetColors(menu, 1, 2);
 *
 * // Show and update each frame
 * MenuShow(menu);
 * while (!MenuConfirmed(menu)) {
 *     MenuUpdate(menu);
 *     EngineFrameEnd();
 * }
 * u8 choice = MenuSelection(menu);
 */

#ifndef MENU_H
#define MENU_H

#include <types.h>
#include <arena.h>
#include <visual.h>

/**
 * @defgroup menu Menu System
 * @brief Beautiful animated menus with minimal code.
 * @{
 */

/** Maximum items per menu */
#define MENU_MAX_ITEMS 8

/** Menu handle (opaque pointer) */
typedef struct Menu *Menu;

/**
 * Create a new menu.
 *
 * @param arena Memory arena for allocation
 * @param panel Visual asset for the menu panel (9-slice stretched)
 * @param cursor Visual asset for the selection cursor
 * @return Menu handle, or NULL on failure
 */
Menu MenuCreate(Arena *arena, const VisualAsset *panel, const VisualAsset *cursor);

/**
 * Set the menu title.
 *
 * @param menu Menu handle
 * @param title Title string (displayed at top of panel)
 */
void MenuSetTitle(Menu menu, const char *title);

/**
 * Set menu position on screen.
 *
 * @param menu Menu handle
 * @param x X position in pixels
 * @param y Y position in pixels
 */
void MenuSetPosition(Menu menu, s16 x, s16 y);

/**
 * Set text palette colors.
 *
 * @param menu Menu handle
 * @param normal Palette for unselected items
 * @param selected Palette for selected item
 */
void MenuSetColors(Menu menu, u8 normal, u8 selected);

/**
 * Set menu sound effects.
 *
 * @param menu Menu handle
 * @param move_sfx SFX index for cursor movement (0xFF = none)
 * @param select_sfx SFX index for selection (0xFF = none)
 */
void MenuSetSounds(Menu menu, u8 move_sfx, u8 select_sfx);

/**
 * Set background dimming amount.
 *
 * @param menu Menu handle
 * @param amount Dim level (0 = none, 10 = 50% brightness, 20 = black)
 */
void MenuSetDim(Menu menu, u8 amount);

/**
 * Add a selectable item to the menu.
 *
 * @param menu Menu handle
 * @param label Item text
 * @return Item index, or 0xFF if menu is full
 */
u8 MenuAddItem(Menu menu, const char *label);

/**
 * Add a non-selectable separator/label.
 *
 * @param menu Menu handle
 * @param label Separator text
 * @return Item index, or 0xFF if menu is full
 */
u8 MenuAddSeparator(Menu menu, const char *label);

/**
 * Update item text.
 *
 * @param menu Menu handle
 * @param index Item index
 * @param label New text
 */
void MenuSetItemText(Menu menu, u8 index, const char *label);

/**
 * Show the menu with slide-in animation.
 */
void MenuShow(Menu menu);

/**
 * Hide the menu with slide-out animation.
 */
void MenuHide(Menu menu);

/**
 * Update menu state. Call once per frame when menu is active.
 * Handles input, animations, and rendering.
 */
void MenuUpdate(Menu menu);

/**
 * Draw menu text. Call after MenuUpdate() during the render phase.
 * Separated from Update to respect VBlank timing.
 */
void MenuDraw(Menu menu);

/**
 * Check if menu needs drawing this frame.
 */
u8 MenuNeedsDraw(Menu menu);

/**
 * Check if menu is currently visible.
 */
u8 MenuVisible(Menu menu);

/**
 * Check if menu is animating (showing/hiding).
 */
u8 MenuAnimating(Menu menu);

/**
 * Get current selection index.
 */
u8 MenuSelection(Menu menu);

/**
 * Set selection index.
 */
void MenuSetSelection(Menu menu, u8 index);

/**
 * Check if user confirmed selection (pressed A).
 * Clears the flag after reading.
 */
u8 MenuConfirmed(Menu menu);

/**
 * Check if user cancelled (pressed B).
 * Clears the flag after reading.
 */
u8 MenuCancelled(Menu menu);

/**
 * Destroy menu and free resources.
 */
void MenuDestroy(Menu menu);

/** @} */

#endif /* MENU_H */
