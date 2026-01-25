/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <ui.h>
#include <actor.h>
#include <ng_input.h>
#include <ng_fix.h>
#include <scene.h>
#include <ng_palette.h>
#include <lighting.h>
#include <ng_audio.h>
#include <graphic.h>
#include <visual.h>

#define MENU_ITEM_HEIGHT     8
#define MENU_TEXT_OFFSET_X   3
#define MENU_TEXT_OFFSET_Y   2
#define MENU_TITLE_OFFSET_Y  3
#define MENU_CURSOR_OFFSET_X -14
#define MENU_CURSOR_OFFSET_Y -4

#define PANEL_MIN_ROWS    7
#define PANEL_TOP_ROWS    1
#define PANEL_BOTTOM_ROWS 1

#define MENU_HIDDEN_OFFSET_FIX FIX_FROM_FLOAT(-120.0)

#define MENU_BLINK_COUNT  3
#define MENU_BLINK_FRAMES 4

#define CURSOR_BOUNCE_SPEED     3
#define CURSOR_BOUNCE_AMPLITUDE 2

typedef struct NGMenu {
    const NGVisualAsset *panel_asset;
    const NGVisualAsset *cursor_asset;

    NGGraphic *panel_graphic;
    u8 panel_height_pixels;

    NGActorHandle cursor_actor;

    s16 viewport_x;
    s16 viewport_y;

    NGSpring panel_y_spring;
    NGSpring cursor_y_spring;

    const char *title;
    const char *items[NG_MENU_MAX_ITEMS];
    u8 item_selectable[NG_MENU_MAX_ITEMS];
    u8 item_count;

    u8 selection;
    u8 confirmed;
    u8 cancelled;

    u8 visible;
    u8 showing;
    u8 text_visible;
    u8 text_dirty;

    u8 blink_count;
    u8 blink_timer;
    u8 blink_on;

    u8 bounce_phase;

    u8 normal_pal;
    u8 selected_pal;

    u8 dim_amount;
    u8 panel_pal;
    u8 cursor_pal;
    NGLightingLayerHandle dim_layer;

    u8 sfx_move;
    u8 sfx_select;
} NGMenu;

// Get Y position for a menu item, accounting for fix layer's 2-tile visible offset
static s16 get_item_y_offset(u8 index) {
    return (s16)((MENU_TITLE_OFFSET_Y + MENU_TEXT_OFFSET_Y + index) * 8 - (NG_FIX_VISIBLE_TOP * 8));
}

static u8 find_first_selectable(NGMenu *menu) {
    for (u8 i = 0; i < menu->item_count; i++) {
        if (menu->item_selectable[i])
            return i;
    }
    return 0;
}

static u8 find_next_selectable(NGMenu *menu, u8 current) {
    for (u8 i = current + 1; i < menu->item_count; i++) {
        if (menu->item_selectable[i])
            return i;
    }
    return current;
}

static u8 find_prev_selectable(NGMenu *menu, u8 current) {
    if (current == 0)
        return current;
    for (u8 i = current - 1;; i--) {
        if (menu->item_selectable[i])
            return i;
        if (i == 0)
            break;
    }
    return current;
}

static void clear_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X - 1;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    s16 width = 16;
    s16 height = (s16)(menu->item_count + 2);

    if (fix_x >= 0 && fix_y >= 0) {
        NGFixClear((u8)fix_x, (u8)fix_y, (u8)width, (u8)height);
    }
}

static void draw_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    if (menu->title && fix_y >= 0 && fix_y <= NG_FIX_VISIBLE_BOTTOM) {
        NGTextPrint(NGFixLayoutXY((u8)fix_x, (u8)fix_y), menu->normal_pal, menu->title);
    }

    for (u8 i = 0; i < menu->item_count; i++) {
        s16 item_y = (s16)(fix_y + MENU_TEXT_OFFSET_Y + i);
        if (item_y >= 0 && item_y <= NG_FIX_VISIBLE_BOTTOM) {
            if (i == menu->selection && menu->blink_count > 0 && !menu->blink_on) {
                NGFixClear((u8)fix_x, (u8)item_y, 12, 1);
            } else {
                u8 pal = (i == menu->selection && menu->item_selectable[i]) ? menu->selected_pal
                                                                            : menu->normal_pal;
                NGTextPrint(NGFixLayoutXY((u8)fix_x, (u8)item_y), pal, menu->items[i]);
            }
        }
    }
}

static u16 calc_panel_height_pixels(u8 item_count) {
    u8 text_fix_rows = MENU_TITLE_OFFSET_Y + MENU_TEXT_OFFSET_Y + item_count;
    u16 content_height_px = (u16)((text_fix_rows * 8) + 8);
    u16 min_height_px = (u16)(PANEL_MIN_ROWS * 16);
    if (content_height_px < min_height_px)
        content_height_px = min_height_px;
    return content_height_px;
}

/**
 * Create or update the panel graphic for the menu.
 */
static void setup_panel_graphic(NGMenu *menu) {
    if (menu->panel_graphic) {
        return; /* Already created */
    }

    const NGVisualAsset *asset = menu->panel_asset;
    u16 height_px = calc_panel_height_pixels(menu->item_count);

    /* Create graphic with 9-slice mode */
    NGGraphicConfig cfg = {.width = asset->width_pixels,
                           .height = height_px,
                           .tile_mode = NG_GRAPHIC_TILE_9SLICE,
                           .layer = NG_GRAPHIC_LAYER_UI,
                           .z_order = NG_MENU_Z_INDEX};
    menu->panel_graphic = NGGraphicCreate(&cfg);
    if (!menu->panel_graphic) {
        return;
    }

    /* Configure 9-slice borders */
    NGGraphicSet9SliceBorders(menu->panel_graphic, PANEL_TOP_ROWS * 16, PANEL_BOTTOM_ROWS * 16, 16,
                              16);

    /* Set source */
    NGGraphicSetSource(menu->panel_graphic, asset, asset->palette);

    menu->panel_height_pixels = (u8)height_px;
}

static void update_panel_position(NGMenu *menu, s16 screen_y) {
    if (!menu->panel_graphic)
        return;

    NGGraphicSetPosition(menu->panel_graphic, menu->viewport_x, screen_y);
}

static void show_panel_graphic(NGMenu *menu) {
    if (!menu->panel_graphic)
        return;

    NGGraphicSetVisible(menu->panel_graphic, 1);
}

static void hide_panel_graphic(NGMenu *menu) {
    if (!menu->panel_graphic)
        return;

    NGGraphicSetVisible(menu->panel_graphic, 0);
}

NGMenuHandle NGMenuCreate(NGArena *arena, const NGVisualAsset *panel_asset,
                          const NGVisualAsset *cursor_asset, u8 dim_amount) {
    if (!arena || !panel_asset || !cursor_asset)
        return 0;

    NGMenu *menu = NG_ARENA_ALLOC(arena, NGMenu);
    if (!menu)
        return 0;

    menu->panel_asset = panel_asset;
    menu->cursor_asset = cursor_asset;

    menu->panel_graphic = NULL;
    menu->panel_height_pixels = 0;

    menu->cursor_actor = NGActorCreate(cursor_asset, 0, 0);

    if (menu->cursor_actor == NG_ACTOR_INVALID) {
        return 0;
    }

    NGActorSetScreenSpace(menu->cursor_actor, 1);

    menu->viewport_x = (s16)((320 - panel_asset->width_pixels) / 2);
    menu->viewport_y = 40;

    NGSpringInit(&menu->panel_y_spring, MENU_HIDDEN_OFFSET_FIX);
    NGSpringInit(&menu->cursor_y_spring, FIX(0));

    menu->panel_y_spring.stiffness = NG_SPRING_BOUNCY_STIFFNESS;
    menu->panel_y_spring.damping = NG_SPRING_BOUNCY_DAMPING;
    menu->cursor_y_spring.stiffness = NG_SPRING_SNAPPY_STIFFNESS;
    menu->cursor_y_spring.damping = NG_SPRING_SNAPPY_DAMPING;

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

    menu->normal_pal = 0;
    menu->selected_pal = 0;

    menu->dim_amount = dim_amount;
    menu->panel_pal = panel_asset->palette;
    menu->cursor_pal = cursor_asset->palette;
    menu->dim_layer = NG_LIGHTING_INVALID_HANDLE;

    menu->sfx_move = 0xFF;
    menu->sfx_select = 0xFF;

    return menu;
}

void NGMenuSetTitle(NGMenuHandle menu, const char *title) {
    if (!menu)
        return;
    menu->title = title;
    menu->text_dirty = 1;
}

void NGMenuSetPosition(NGMenuHandle menu, s16 viewport_x, s16 viewport_y) {
    if (!menu)
        return;
    menu->viewport_x = viewport_x;
    menu->viewport_y = viewport_y;
    menu->text_dirty = 1;
}

void NGMenuSetTextPalette(NGMenuHandle menu, u8 normal_pal, u8 selected_pal) {
    if (!menu)
        return;
    menu->normal_pal = normal_pal;
    menu->selected_pal = selected_pal;
    menu->text_dirty = 1;
}

u8 NGMenuAddItem(NGMenuHandle menu, const char *label) {
    if (!menu || menu->item_count >= NG_MENU_MAX_ITEMS)
        return 0xFF;

    u8 index = menu->item_count;
    menu->items[index] = label;
    menu->item_selectable[index] = 1; // Normal items are selectable
    menu->item_count++;
    menu->text_dirty = 1;

    return index;
}

u8 NGMenuAddSeparator(NGMenuHandle menu, const char *label) {
    if (!menu || menu->item_count >= NG_MENU_MAX_ITEMS)
        return 0xFF;

    u8 index = menu->item_count;
    menu->items[index] = label;
    menu->item_selectable[index] = 0; // Separators are not selectable
    menu->item_count++;
    menu->text_dirty = 1;

    return index;
}

void NGMenuSetItemText(NGMenuHandle menu, u8 index, const char *label) {
    if (!menu || index >= menu->item_count)
        return;

    menu->items[index] = label;
    menu->text_dirty = 1;
}

void NGMenuSetSounds(NGMenuHandle menu, u8 move_sfx, u8 select_sfx) {
    if (!menu)
        return;
    menu->sfx_move = move_sfx;
    menu->sfx_select = select_sfx;
}

void NGMenuShow(NGMenuHandle menu) {
    if (!menu)
        return;

    menu->visible = 1;
    menu->confirmed = 0;
    menu->cancelled = 0;

    menu->selection = find_first_selectable(menu);

    NGSpringSetTarget(&menu->panel_y_spring, FIX(menu->viewport_y));

    s16 cursor_offset = get_item_y_offset(menu->selection) + MENU_CURSOR_OFFSET_Y;
    NGSpringSnap(&menu->cursor_y_spring, FIX(cursor_offset));
    NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));

    /* Create lighting layer for dimming effect */
    if (menu->dim_amount > 0 && menu->dim_layer == NG_LIGHTING_INVALID_HANDLE) {
        menu->dim_layer = NGLightingPush(NG_LIGHTING_PRIORITY_OVERLAY);
        if (menu->dim_layer != NG_LIGHTING_INVALID_HANDLE) {
            /* Animate brightness from 1.0 to target dim level.
             * dim_amount=10 produces 50% brightness (50% dimming).
             * dim_amount=20 produces 0% brightness (full black). */
            fixed target_brightness = FIX_ONE - FIX(menu->dim_amount) / 20;
            NGLightingFadeBrightness(menu->dim_layer, target_brightness, 8);
        }
    }

    /* Create panel graphic if needed */
    setup_panel_graphic(menu);

    s16 panel_y = NGSpringGetInt(&menu->panel_y_spring);
    update_panel_position(menu, panel_y);
    show_panel_graphic(menu);

    s16 cursor_x = menu->viewport_x + MENU_TEXT_OFFSET_X * 8 + MENU_CURSOR_OFFSET_X;
    s16 cursor_y = panel_y + NGSpringGetInt(&menu->cursor_y_spring);
    NGActorAddToScene(menu->cursor_actor, FIX(cursor_x), FIX(cursor_y), NG_MENU_Z_INDEX + 1);

    /* Restore menu palettes to original values.
     * UI elements should be exempt from lighting effects. */
    if (menu->panel_asset && menu->panel_asset->palette_data) {
        NGPalSet(menu->panel_pal, menu->panel_asset->palette_data);
    }
    if (menu->cursor_asset && menu->cursor_asset->palette_data) {
        NGPalSet(menu->cursor_pal, menu->cursor_asset->palette_data);
    }

    menu->showing = 1;
    menu->text_dirty = 1;
}

void NGMenuHide(NGMenuHandle menu) {
    if (!menu)
        return;

    menu->visible = 0;

    NGSpringSetTarget(&menu->panel_y_spring, MENU_HIDDEN_OFFSET_FIX);

    if (menu->text_visible) {
        clear_menu_text(menu);
        menu->text_visible = 0;
    }

    /* Start fade-out animation on lighting layer */
    if (menu->dim_layer != NG_LIGHTING_INVALID_HANDLE) {
        NGLightingFadeBrightness(menu->dim_layer, FIX_ONE, 8);
    }
}

void NGMenuUpdate(NGMenuHandle menu) {
    if (!menu)
        return;

    NGSpringUpdate(&menu->panel_y_spring);
    NGSpringUpdate(&menu->cursor_y_spring);

    /* Check if we can clean up after hiding */
    u8 dim_done = (menu->dim_layer == NG_LIGHTING_INVALID_HANDLE) || !NGLightingIsAnimating();
    if (!menu->visible && NGSpringSettled(&menu->panel_y_spring) && dim_done) {
        if (menu->showing) {
            /* Remove lighting layer when fully hidden */
            if (menu->dim_layer != NG_LIGHTING_INVALID_HANDLE) {
                NGLightingPop(menu->dim_layer);
                menu->dim_layer = NG_LIGHTING_INVALID_HANDLE;
            }
            hide_panel_graphic(menu);
            NGActorRemoveFromScene(menu->cursor_actor);
            menu->showing = 0;
        }
        return;
    }

    if (menu->visible) {
        /* Keep menu palettes at original values.
         * Must be done every frame since lighting system may overwrite them. */
        if (menu->panel_asset && menu->panel_asset->palette_data) {
            NGPalSet(menu->panel_pal, menu->panel_asset->palette_data);
        }
        if (menu->cursor_asset && menu->cursor_asset->palette_data) {
            NGPalSet(menu->cursor_pal, menu->cursor_asset->palette_data);
        }

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

            if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
                menu->blink_count = MENU_BLINK_COUNT;
                menu->blink_timer = MENU_BLINK_FRAMES;
                menu->blink_on = 0;
                menu->text_dirty = 1;
                if (menu->sfx_select != 0xFF) {
                    NGSfxPlay(menu->sfx_select);
                }
            }

            if (NGInputPressed(NG_PLAYER_1, NG_BTN_B)) {
                menu->cancelled = 1;
            }
        }
    }

    s16 panel_y = NGSpringGetInt(&menu->panel_y_spring);
    if (menu->panel_graphic) {
        update_panel_position(menu, panel_y);
    }

    s16 cursor_x = menu->viewport_x + MENU_TEXT_OFFSET_X * 8 + MENU_CURSOR_OFFSET_X;
    s16 cursor_y = panel_y + NGSpringGetInt(&menu->cursor_y_spring);

    if (menu->visible && NGSpringSettled(&menu->cursor_y_spring) && menu->blink_count == 0) {
        menu->bounce_phase += CURSOR_BOUNCE_SPEED;
        if (menu->bounce_phase >= 128) {
            cursor_x += CURSOR_BOUNCE_AMPLITUDE;
        }
    }

    NGActorSetPos(menu->cursor_actor, FIX(cursor_x), FIX(cursor_y));
}

u8 NGMenuNeedsDraw(NGMenuHandle menu) {
    if (!menu || !menu->showing)
        return 0;

    u8 panel_arrived = menu->visible && NGSpringSettled(&menu->panel_y_spring);

    // Need to draw if: panel just arrived and text not shown yet, OR text is dirty
    if (panel_arrived && !menu->text_visible)
        return 1;
    if (menu->text_visible && menu->text_dirty)
        return 1;

    return 0;
}

void NGMenuDraw(NGMenuHandle menu) {
    if (!menu || !menu->showing)
        return;

    u8 panel_arrived = menu->visible && NGSpringSettled(&menu->panel_y_spring);

    if (panel_arrived && !menu->text_visible) {
        draw_menu_text(menu);
        menu->text_visible = 1;
        menu->text_dirty = 0;
    } else if (menu->text_visible && menu->text_dirty) {
        draw_menu_text(menu);
        menu->text_dirty = 0;
    }
}

u8 NGMenuIsVisible(NGMenuHandle menu) {
    if (!menu)
        return 0;
    return menu->showing;
}

u8 NGMenuIsAnimating(NGMenuHandle menu) {
    if (!menu)
        return 0;
    /* Animating if spring not settled OR lighting is fading */
    u8 lighting_animating =
        (menu->dim_layer != NG_LIGHTING_INVALID_HANDLE) && NGLightingIsAnimating();
    return !NGSpringSettled(&menu->panel_y_spring) || lighting_animating;
}

u8 NGMenuGetSelection(NGMenuHandle menu) {
    if (!menu)
        return 0;
    return menu->selection;
}

void NGMenuSetSelection(NGMenuHandle menu, u8 index) {
    if (!menu || index >= menu->item_count)
        return;
    menu->selection = index;

    if (menu->visible) {
        s16 cursor_offset = get_item_y_offset(index) + MENU_CURSOR_OFFSET_Y;
        NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));
    }

    menu->text_dirty = 1;
}

u8 NGMenuConfirmed(NGMenuHandle menu) {
    if (!menu)
        return 0;
    u8 result = menu->confirmed;
    menu->confirmed = 0;
    return result;
}

u8 NGMenuCancelled(NGMenuHandle menu) {
    if (!menu)
        return 0;
    u8 result = menu->cancelled;
    menu->cancelled = 0;
    return result;
}

void NGMenuDestroy(NGMenuHandle menu) {
    if (!menu)
        return;

    /* Remove lighting layer if active */
    if (menu->dim_layer != NG_LIGHTING_INVALID_HANDLE) {
        NGLightingPop(menu->dim_layer);
        menu->dim_layer = NG_LIGHTING_INVALID_HANDLE;
    }

    if (menu->text_visible) {
        clear_menu_text(menu);
    }

    /* Destroy panel graphic */
    if (menu->panel_graphic) {
        NGGraphicDestroy(menu->panel_graphic);
        menu->panel_graphic = NULL;
    }

    if (menu->cursor_actor != NG_ACTOR_INVALID) {
        NGActorDestroy(menu->cursor_actor);
    }
}
