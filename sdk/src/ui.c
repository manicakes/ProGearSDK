/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// ui.c - UI system implementation

#include <ui.h>
#include <actor.h>
#include <input.h>
#include <fix.h>
#include <scene.h>
#include <palette.h>
#include <audio.h>

// === Internal Constants ===

#define MENU_ITEM_HEIGHT      8    // Pixels between menu items (must match tile size)
#define MENU_TEXT_OFFSET_X    3    // Text X offset from panel left (in tiles)
#define MENU_TEXT_OFFSET_Y    2    // Text Y offset from title (in tiles)
#define MENU_TITLE_OFFSET_Y   3    // Title Y offset from panel top (in tiles)
#define MENU_CURSOR_OFFSET_X -14   // Cursor X offset from text
#define MENU_CURSOR_OFFSET_Y -4    // Cursor Y offset to center on text

// Off-screen position for hidden menu (slides in from top)
#define MENU_HIDDEN_OFFSET_FIX   FIX_FROM_FLOAT(-120.0)

// Confirmation blink animation
#define MENU_BLINK_COUNT         3    // Number of blinks
#define MENU_BLINK_FRAMES        4    // Frames per blink state (on/off)

// Cursor bounce animation
#define CURSOR_BOUNCE_SPEED      3    // Phase increment per frame (slower = subtler)
#define CURSOR_BOUNCE_AMPLITUDE  2    // Max pixels of horizontal movement

// Palette dimming animation
#define DIM_ANIM_SPEED           2    // Fade amount change per frame (higher = faster)
#define DIM_BACKUP_MAX_PALETTES 32    // Max palettes to backup (32 * 16 * 2 = 1KB)

// === Internal Types ===

typedef struct NGMenu {
    // Assets
    const NGVisualAsset *panel_asset;
    const NGVisualAsset *cursor_asset;

    // Actors
    NGActorHandle panel_actor;
    NGActorHandle cursor_actor;

    // Position (viewport coordinates)
    s16 viewport_x;
    s16 viewport_y;

    // Animation
    NGSpring panel_y_spring;    // Panel entrance/exit animation
    NGSpring cursor_y_spring;   // Cursor movement animation

    // Items
    const char *title;
    const char *items[NG_MENU_MAX_ITEMS];
    u8 item_selectable[NG_MENU_MAX_ITEMS];  // 1 = selectable, 0 = separator
    u8 item_count;

    // Selection
    u8 selection;
    u8 confirmed;
    u8 cancelled;

    // State
    u8 visible;          // Target visibility
    u8 showing;          // Currently visible (for rendering)
    u8 text_visible;     // Text is currently drawn on fix layer
    u8 text_dirty;       // Need to redraw fix layer text

    // Confirmation blink animation
    u8 blink_count;      // Blinks remaining (0 = not blinking)
    u8 blink_timer;      // Frames until next toggle
    u8 blink_on;         // Current blink state

    // Cursor bounce animation
    u8 bounce_phase;     // Phase of bounce wave (0-255)

    // Text palettes (fix layer)
    u8 normal_pal;
    u8 selected_pal;

    // Palette dimming (replaces sprite overlay)
    u8 dim_target;       // Target dimming amount (0-31, 0 = no dimming)
    u8 dim_current;      // Current dimming (animated toward target)
    u8 dim_start_pal;    // First palette to dim
    u8 dim_end_pal;      // Last palette to dim (inclusive)
    u8 panel_pal;        // Panel palette (excluded from dimming)
    u8 cursor_pal;       // Cursor palette (excluded from dimming)
    NGColor *pal_backup; // Backup of original palette colors (arena-allocated)
    u8 backup_count;     // Number of palettes backed up

    // Sound effects
    u8 sfx_move;         // SFX index for cursor movement (0xFF = none)
    u8 sfx_select;       // SFX index for selection (0xFF = none)
} NGMenu;

// === Helper Functions ===

// Get Y position for a menu item (relative to panel top, in pixels)
// Fix layer row N appears at screen pixel (N - NG_FIX_VISIBLE_TOP) * 8
// Text is at fix row (viewport_y/8 + TITLE + TEXT + index)
// So cursor offset must subtract 16 pixels (2 tiles) to match fix layer visible offset
static s16 get_item_y_offset(u8 index) {
    return (MENU_TITLE_OFFSET_Y + MENU_TEXT_OFFSET_Y + index) * 8 - (NG_FIX_VISIBLE_TOP * 8);
}

// Find first selectable item (returns 0 if none found)
static u8 find_first_selectable(NGMenu *menu) {
    for (u8 i = 0; i < menu->item_count; i++) {
        if (menu->item_selectable[i]) return i;
    }
    return 0;
}

// Find next selectable item going down (returns current if none found)
static u8 find_next_selectable(NGMenu *menu, u8 current) {
    for (u8 i = current + 1; i < menu->item_count; i++) {
        if (menu->item_selectable[i]) return i;
    }
    return current;  // No selectable item found, stay put
}

// Find previous selectable item going up (returns current if none found)
static u8 find_prev_selectable(NGMenu *menu, u8 current) {
    if (current == 0) return current;
    for (u8 i = current - 1; ; i--) {
        if (menu->item_selectable[i]) return i;
        if (i == 0) break;
    }
    return current;  // No selectable item found, stay put
}

// Clear menu text area on fix layer (always at final position)
static void clear_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X - 1;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    s16 width = 16;
    s16 height = menu->item_count + 2;

    if (fix_x >= 0 && fix_y >= 0) {
        NGFixClear(fix_x, fix_y, width, height);
    }
}

// Draw menu text on fix layer (at final position, no animation)
static void draw_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    // Draw title if set
    if (menu->title && fix_y >= 0 && fix_y < 28) {
        NGTextPrint(NGFixLayoutXY(fix_x, fix_y), menu->normal_pal, menu->title);
    }

    // Draw items
    for (u8 i = 0; i < menu->item_count; i++) {
        s16 item_y = fix_y + MENU_TEXT_OFFSET_Y + i;
        if (item_y >= 0 && item_y < 28) {
            // During blink animation: hide selected item when blink_on is false
            if (i == menu->selection && menu->blink_count > 0 && !menu->blink_on) {
                // Clear the item text (will be redrawn next blink cycle)
                NGFixClear(fix_x, item_y, 12, 1);
            } else {
                // Only highlight selectable items, separators always use normal palette
                u8 pal = (i == menu->selection && menu->item_selectable[i])
                         ? menu->selected_pal : menu->normal_pal;
                NGTextPrint(NGFixLayoutXY(fix_x, item_y), pal, menu->items[i]);
            }
        }
    }
}

// Backup palettes that will be dimmed
static void backup_palettes(NGMenu *menu) {
    if (!menu->pal_backup || menu->backup_count == 0) return;

    u8 backup_idx = 0;
    for (u8 pal = menu->dim_start_pal; pal <= menu->dim_end_pal && backup_idx < menu->backup_count; pal++) {
        // Skip menu's own palettes
        if (pal == menu->panel_pal || pal == menu->cursor_pal) continue;

        NGPalBackup(pal, &menu->pal_backup[backup_idx * NG_PAL_SIZE]);
        backup_idx++;
    }
}

// Apply current dimming level from backed-up palettes
static void apply_dimming(NGMenu *menu) {
    if (!menu->pal_backup || menu->backup_count == 0) return;

    u8 backup_idx = 0;
    for (u8 pal = menu->dim_start_pal; pal <= menu->dim_end_pal && backup_idx < menu->backup_count; pal++) {
        // Skip menu's own palettes
        if (pal == menu->panel_pal || pal == menu->cursor_pal) continue;

        // Restore from backup first
        NGPalRestore(pal, &menu->pal_backup[backup_idx * NG_PAL_SIZE]);

        // Then apply current dimming
        if (menu->dim_current > 0) {
            NGPalFadeToBlack(pal, menu->dim_current);
        }

        backup_idx++;
    }
}

// Restore original palettes from backup
static void restore_palettes(NGMenu *menu) {
    if (!menu->pal_backup || menu->backup_count == 0) return;

    u8 backup_idx = 0;
    for (u8 pal = menu->dim_start_pal; pal <= menu->dim_end_pal && backup_idx < menu->backup_count; pal++) {
        // Skip menu's own palettes
        if (pal == menu->panel_pal || pal == menu->cursor_pal) continue;

        NGPalRestore(pal, &menu->pal_backup[backup_idx * NG_PAL_SIZE]);
        backup_idx++;
    }
}

// === Public API ===

NGMenuHandle NGMenuCreate(NGArena *arena,
                          const NGVisualAsset *panel_asset,
                          const NGVisualAsset *cursor_asset,
                          u8 dim_amount) {
    if (!arena || !panel_asset || !cursor_asset) return 0;

    NGMenu *menu = NG_ARENA_ALLOC(arena, NGMenu);
    if (!menu) return 0;

    // Store assets
    menu->panel_asset = panel_asset;
    menu->cursor_asset = cursor_asset;

    // Create actors (but don't add to scene yet)
    menu->panel_actor = NGActorCreate(panel_asset, 0, 0);
    menu->cursor_actor = NGActorCreate(cursor_asset, 0, 0);

    if (menu->panel_actor == NG_ACTOR_INVALID ||
        menu->cursor_actor == NG_ACTOR_INVALID) {
        return 0;
    }

    // Set as screen-space (ignore camera position and zoom)
    NGActorSetScreenSpace(menu->panel_actor, 1);
    NGActorSetScreenSpace(menu->cursor_actor, 1);

    // Default position (centered horizontally)
    menu->viewport_x = (320 - panel_asset->width_pixels) / 2;
    menu->viewport_y = 40;

    // Initialize springs (start hidden)
    NGSpringInit(&menu->panel_y_spring, MENU_HIDDEN_OFFSET_FIX);
    NGSpringInit(&menu->cursor_y_spring, FIX(0));

    menu->panel_y_spring.stiffness = NG_SPRING_BOUNCY_STIFFNESS;
    menu->panel_y_spring.damping = NG_SPRING_BOUNCY_DAMPING;
    menu->cursor_y_spring.stiffness = NG_SPRING_SNAPPY_STIFFNESS;
    menu->cursor_y_spring.damping = NG_SPRING_SNAPPY_DAMPING;

    // Initialize state
    menu->title = 0;
    menu->item_count = 0;
    menu->selection = 0;
    menu->confirmed = 0;
    menu->cancelled = 0;
    menu->visible = 0;
    menu->showing = 0;
    menu->text_visible = 0;
    menu->text_dirty = 0;
    menu->blink_count = 0;
    menu->blink_timer = 0;
    menu->blink_on = 1;
    menu->bounce_phase = 0;

    // Default palettes
    menu->normal_pal = 0;
    menu->selected_pal = 0;

    // Initialize palette dimming
    menu->dim_target = dim_amount;
    menu->dim_current = 0;
    menu->dim_start_pal = 1;    // Start at palette 1 (skip 0 which is used for menu text)
    menu->dim_end_pal = 63;     // End at palette 63 (enough for most games, avoids u8 overflow)
    menu->panel_pal = panel_asset->palette;
    menu->cursor_pal = cursor_asset->palette;
    menu->pal_backup = 0;
    menu->backup_count = 0;

    // Allocate palette backup if dimming is enabled
    if (dim_amount > 0) {
        // Count palettes to backup (excluding menu's own palettes)
        u8 count = 0;
        for (u8 pal = menu->dim_start_pal; pal <= menu->dim_end_pal; pal++) {
            if (pal != menu->panel_pal && pal != menu->cursor_pal) {
                count++;
                if (count >= DIM_BACKUP_MAX_PALETTES) break;
            }
        }

        if (count > 0) {
            menu->backup_count = count;
            menu->pal_backup = NG_ARENA_ALLOC_ARRAY(arena, NGColor, count * NG_PAL_SIZE);
        }
    }

    // No sound effects by default
    menu->sfx_move = 0xFF;
    menu->sfx_select = 0xFF;

    return menu;
}

void NGMenuSetTitle(NGMenuHandle menu, const char *title) {
    if (!menu) return;
    menu->title = title;
    menu->text_dirty = 1;
}

void NGMenuSetPosition(NGMenuHandle menu, s16 viewport_x, s16 viewport_y) {
    if (!menu) return;
    menu->viewport_x = viewport_x;
    menu->viewport_y = viewport_y;
    menu->text_dirty = 1;
}

void NGMenuSetTextPalette(NGMenuHandle menu, u8 normal_pal, u8 selected_pal) {
    if (!menu) return;
    menu->normal_pal = normal_pal;
    menu->selected_pal = selected_pal;
    menu->text_dirty = 1;
}

u8 NGMenuAddItem(NGMenuHandle menu, const char *label) {
    if (!menu || menu->item_count >= NG_MENU_MAX_ITEMS) return 0xFF;

    u8 index = menu->item_count;
    menu->items[index] = label;
    menu->item_selectable[index] = 1;  // Normal items are selectable
    menu->item_count++;
    menu->text_dirty = 1;

    return index;
}

u8 NGMenuAddSeparator(NGMenuHandle menu, const char *label) {
    if (!menu || menu->item_count >= NG_MENU_MAX_ITEMS) return 0xFF;

    u8 index = menu->item_count;
    menu->items[index] = label;
    menu->item_selectable[index] = 0;  // Separators are not selectable
    menu->item_count++;
    menu->text_dirty = 1;

    return index;
}

void NGMenuSetItemText(NGMenuHandle menu, u8 index, const char *label) {
    if (!menu || index >= menu->item_count) return;

    menu->items[index] = label;
    menu->text_dirty = 1;
}

void NGMenuSetSounds(NGMenuHandle menu, u8 move_sfx, u8 select_sfx) {
    if (!menu) return;
    menu->sfx_move = move_sfx;
    menu->sfx_select = select_sfx;
}

void NGMenuShow(NGMenuHandle menu) {
    if (!menu) return;

    menu->visible = 1;
    menu->confirmed = 0;
    menu->cancelled = 0;

    // Start on first selectable item
    menu->selection = find_first_selectable(menu);

    // Set spring target to visible position
    NGSpringSetTarget(&menu->panel_y_spring, FIX(menu->viewport_y));

    // Set cursor offset relative to panel top (not absolute Y)
    s16 cursor_offset = get_item_y_offset(menu->selection) + MENU_CURSOR_OFFSET_Y;
    NGSpringSnap(&menu->cursor_y_spring, FIX(cursor_offset));
    NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));

    // Backup palettes and start dimming animation
    if (menu->dim_target > 0) {
        backup_palettes(menu);
        menu->dim_current = 0;  // Will animate toward dim_target
    }

    // Add actors to scene at high Z-index (screen-space, so use screen coords directly)
    s16 panel_y = NGSpringGetInt(&menu->panel_y_spring);
    NGActorAddToScene(menu->panel_actor, FIX(menu->viewport_x), FIX(panel_y), NG_MENU_Z_INDEX);

    s16 cursor_x = menu->viewport_x + MENU_TEXT_OFFSET_X * 8 + MENU_CURSOR_OFFSET_X;
    s16 cursor_y = panel_y + NGSpringGetInt(&menu->cursor_y_spring);
    NGActorAddToScene(menu->cursor_actor, FIX(cursor_x), FIX(cursor_y), NG_MENU_Z_INDEX + 1);

    menu->showing = 1;
    menu->text_dirty = 1;
}

void NGMenuHide(NGMenuHandle menu) {
    if (!menu) return;

    menu->visible = 0;

    // Set spring target to hidden position
    NGSpringSetTarget(&menu->panel_y_spring, MENU_HIDDEN_OFFSET_FIX);

    // Dimming will animate back to 0 in NGMenuUpdate

    // Clear text immediately
    if (menu->text_visible) {
        clear_menu_text(menu);
        menu->text_visible = 0;
    }
}

void NGMenuUpdate(NGMenuHandle menu) {
    if (!menu) return;

    // Update springs
    NGSpringUpdate(&menu->panel_y_spring);
    NGSpringUpdate(&menu->cursor_y_spring);

    // Animate palette dimming
    if (menu->dim_target > 0) {
        u8 target = menu->visible ? menu->dim_target : 0;

        if (menu->dim_current < target) {
            // Fade in (darken)
            menu->dim_current += DIM_ANIM_SPEED;
            if (menu->dim_current > target) menu->dim_current = target;
            apply_dimming(menu);
        } else if (menu->dim_current > target) {
            // Fade out (restore brightness)
            if (menu->dim_current >= DIM_ANIM_SPEED) {
                menu->dim_current -= DIM_ANIM_SPEED;
            } else {
                menu->dim_current = 0;
            }
            apply_dimming(menu);
        }
    }

    // Check if fully hidden
    if (!menu->visible && NGSpringSettled(&menu->panel_y_spring) && menu->dim_current == 0) {
        if (menu->showing) {
            // Restore original palettes
            restore_palettes(menu);
            // Remove panel and cursor
            NGActorRemoveFromScene(menu->panel_actor);
            NGActorRemoveFromScene(menu->cursor_actor);
            menu->showing = 0;
        }
        return;
    }

    // Handle input only when visible and not animating out
    if (menu->visible) {
        // Handle blink animation
        if (menu->blink_count > 0) {
            menu->blink_timer--;
            if (menu->blink_timer == 0) {
                menu->blink_on = !menu->blink_on;
                menu->text_dirty = 1;
                if (menu->blink_on) {
                    // Completed one full blink (off->on)
                    menu->blink_count--;
                    if (menu->blink_count == 0) {
                        // Blink animation complete - confirm now
                        menu->confirmed = 1;
                    }
                }
                menu->blink_timer = MENU_BLINK_FRAMES;
            }
        } else {
            // Normal input (only when not blinking)

            // Navigation - skip non-selectable items
            if (NGInputPressed(NG_PLAYER_1, NG_BTN_UP)) {
                u8 prev = find_prev_selectable(menu, menu->selection);
                if (prev != menu->selection) {
                    menu->selection = prev;
                    s16 cursor_offset = get_item_y_offset(menu->selection) + MENU_CURSOR_OFFSET_Y;
                    NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));
                    menu->text_dirty = 1;
                    if (menu->sfx_move != 0xFF) {
                        NGSfxPlay(menu->sfx_move);
                    }
                }
            }
            if (NGInputPressed(NG_PLAYER_1, NG_BTN_DOWN)) {
                u8 next = find_next_selectable(menu, menu->selection);
                if (next != menu->selection) {
                    menu->selection = next;
                    s16 cursor_offset = get_item_y_offset(menu->selection) + MENU_CURSOR_OFFSET_Y;
                    NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));
                    menu->text_dirty = 1;
                    if (menu->sfx_move != 0xFF) {
                        NGSfxPlay(menu->sfx_move);
                    }
                }
            }

            // Confirmation - start blink animation
            if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
                menu->blink_count = MENU_BLINK_COUNT;
                menu->blink_timer = MENU_BLINK_FRAMES;
                menu->blink_on = 0;  // Start with text off
                menu->text_dirty = 1;
                if (menu->sfx_select != 0xFF) {
                    NGSfxPlay(menu->sfx_select);
                }
            }

            // Cancellation
            if (NGInputPressed(NG_PLAYER_1, NG_BTN_B)) {
                menu->cancelled = 1;
            }
        }
    }

    // Update actor positions (screen-space, so use screen coords directly)
    s16 panel_y = NGSpringGetInt(&menu->panel_y_spring);
    NGActorSetPos(menu->panel_actor, FIX(menu->viewport_x), FIX(panel_y));

    // Cursor position
    s16 cursor_x = menu->viewport_x + MENU_TEXT_OFFSET_X * 8 + MENU_CURSOR_OFFSET_X;
    s16 cursor_y = panel_y + NGSpringGetInt(&menu->cursor_y_spring);

    // Bounce animation: only when cursor is settled and menu is visible
    if (menu->visible && NGSpringSettled(&menu->cursor_y_spring) && menu->blink_count == 0) {
        menu->bounce_phase += CURSOR_BOUNCE_SPEED;

        // Simple toggle: 0 pixels for first half, 2 pixels for second half
        if (menu->bounce_phase >= 128) {
            cursor_x += CURSOR_BOUNCE_AMPLITUDE;
        }
    }

    NGActorSetPos(menu->cursor_actor, FIX(cursor_x), FIX(cursor_y));
}

void NGMenuDraw(NGMenuHandle menu) {
    if (!menu || !menu->showing) return;

    // Show text only when panel has arrived (spring settled)
    u8 panel_arrived = menu->visible && NGSpringSettled(&menu->panel_y_spring);

    if (panel_arrived && !menu->text_visible) {
        // Panel just arrived - show text
        draw_menu_text(menu);
        menu->text_visible = 1;
        menu->text_dirty = 0;
    } else if (menu->text_visible && menu->text_dirty) {
        // Text visible and needs redraw (selection changed)
        // No need to clear - just overdraw with new palettes
        draw_menu_text(menu);
        menu->text_dirty = 0;
    }
}

u8 NGMenuIsVisible(NGMenuHandle menu) {
    if (!menu) return 0;
    return menu->showing;
}

u8 NGMenuIsAnimating(NGMenuHandle menu) {
    if (!menu) return 0;
    // Animating if spring not settled OR dimming is still transitioning
    u8 target = menu->visible ? menu->dim_target : 0;
    return !NGSpringSettled(&menu->panel_y_spring) || (menu->dim_current != target);
}

u8 NGMenuGetSelection(NGMenuHandle menu) {
    if (!menu) return 0;
    return menu->selection;
}

void NGMenuSetSelection(NGMenuHandle menu, u8 index) {
    if (!menu || index >= menu->item_count) return;
    menu->selection = index;

    if (menu->visible) {
        s16 cursor_offset = get_item_y_offset(index) + MENU_CURSOR_OFFSET_Y;
        NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));
    }

    menu->text_dirty = 1;
}

u8 NGMenuConfirmed(NGMenuHandle menu) {
    if (!menu) return 0;
    u8 result = menu->confirmed;
    menu->confirmed = 0;
    return result;
}

u8 NGMenuCancelled(NGMenuHandle menu) {
    if (!menu) return 0;
    u8 result = menu->cancelled;
    menu->cancelled = 0;
    return result;
}

void NGMenuDestroy(NGMenuHandle menu) {
    if (!menu) return;

    // Restore palettes if dimmed
    if (menu->dim_current > 0) {
        restore_palettes(menu);
    }

    // Clear text if visible
    if (menu->text_visible) {
        clear_menu_text(menu);
    }

    // Destroy actors
    if (menu->panel_actor != NG_ACTOR_INVALID) {
        NGActorDestroy(menu->panel_actor);
    }
    if (menu->cursor_actor != NG_ACTOR_INVALID) {
        NGActorDestroy(menu->cursor_actor);
    }

    // Note: pal_backup is arena-allocated, will be freed with arena
}
