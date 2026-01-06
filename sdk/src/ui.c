/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <ui.h>
#include <actor.h>
#include <input.h>
#include <fix.h>
#include <scene.h>
#include <palette.h>
#include <audio.h>
#include <neogeo.h>

#define MENU_ITEM_HEIGHT      8
#define MENU_TEXT_OFFSET_X    3
#define MENU_TEXT_OFFSET_Y    2
#define MENU_TITLE_OFFSET_Y   3
#define MENU_CURSOR_OFFSET_X -14
#define MENU_CURSOR_OFFSET_Y -4

#define PANEL_MIN_ROWS        7
#define PANEL_COLS           11
#define PANEL_TOP_ROWS        1
#define PANEL_BOTTOM_ROWS     1
#define PANEL_MIDDLE_ROW      3
#define PANEL_SPRITE_BASE   320

#define MENU_HIDDEN_OFFSET_FIX   FIX_FROM_FLOAT(-120.0)

#define MENU_BLINK_COUNT         3
#define MENU_BLINK_FRAMES        4

#define CURSOR_BOUNCE_SPEED      3
#define CURSOR_BOUNCE_AMPLITUDE  2

#define DIM_ANIM_SPEED           2
#define DIM_BACKUP_MAX_PALETTES 32

typedef struct NGMenu {
    const NGVisualAsset *panel_asset;
    const NGVisualAsset *cursor_asset;

    u16 panel_first_sprite;
    u8 panel_height_tiles;
    u8 panel_sprites_allocated;

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

    u8 dim_target;
    u8 dim_current;
    u8 dim_start_pal;
    u8 dim_end_pal;
    u8 panel_pal;
    u8 cursor_pal;
    NGColor *pal_backup;
    u8 backup_count;

    u8 sfx_move;
    u8 sfx_select;
} NGMenu;

// Get Y position for a menu item, accounting for fix layer's 2-tile visible offset
static s16 get_item_y_offset(u8 index) {
    return (MENU_TITLE_OFFSET_Y + MENU_TEXT_OFFSET_Y + index) * 8 - (NG_FIX_VISIBLE_TOP * 8);
}

static u8 find_first_selectable(NGMenu *menu) {
    for (u8 i = 0; i < menu->item_count; i++) {
        if (menu->item_selectable[i]) return i;
    }
    return 0;
}

static u8 find_next_selectable(NGMenu *menu, u8 current) {
    for (u8 i = current + 1; i < menu->item_count; i++) {
        if (menu->item_selectable[i]) return i;
    }
    return current;
}

static u8 find_prev_selectable(NGMenu *menu, u8 current) {
    if (current == 0) return current;
    for (u8 i = current - 1; ; i--) {
        if (menu->item_selectable[i]) return i;
        if (i == 0) break;
    }
    return current;
}

static void clear_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X - 1;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    s16 width = 16;
    s16 height = menu->item_count + 2;

    if (fix_x >= 0 && fix_y >= 0) {
        NGFixClear(fix_x, fix_y, width, height);
    }
}

static void draw_menu_text(NGMenu *menu) {
    s16 fix_x = menu->viewport_x / 8 + MENU_TEXT_OFFSET_X;
    s16 fix_y = menu->viewport_y / 8 + MENU_TITLE_OFFSET_Y;

    if (menu->title && fix_y >= 0 && fix_y < 28) {
        NGTextPrint(NGFixLayoutXY(fix_x, fix_y), menu->normal_pal, menu->title);
    }

    for (u8 i = 0; i < menu->item_count; i++) {
        s16 item_y = fix_y + MENU_TEXT_OFFSET_Y + i;
        if (item_y >= 0 && item_y < 28) {
            if (i == menu->selection && menu->blink_count > 0 && !menu->blink_on) {
                NGFixClear(fix_x, item_y, 12, 1);
            } else {
                u8 pal = (i == menu->selection && menu->item_selectable[i])
                         ? menu->selected_pal : menu->normal_pal;
                NGTextPrint(NGFixLayoutXY(fix_x, item_y), pal, menu->items[i]);
            }
        }
    }
}

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

static void apply_dimming(NGMenu *menu) {
    if (!menu->pal_backup || menu->backup_count == 0) return;

    u8 backup_idx = 0;
    for (u8 pal = menu->dim_start_pal; pal <= menu->dim_end_pal && backup_idx < menu->backup_count; pal++) {
        if (pal == menu->panel_pal || pal == menu->cursor_pal) continue;

        NGPalRestore(pal, &menu->pal_backup[backup_idx * NG_PAL_SIZE]);

        if (menu->dim_current > 0) {
            NGPalFadeToBlack(pal, menu->dim_current);
        }

        backup_idx++;
    }
}

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

#define SCB1_BASE  0x0000
#define SCB2_BASE  0x8000
#define SCB3_BASE  0x8200
#define SCB4_BASE  0x8400

static u8 calc_panel_height(u8 item_count) {
    u8 text_fix_rows = MENU_TITLE_OFFSET_Y + MENU_TEXT_OFFSET_Y + item_count;
    u16 content_height_px = (text_fix_rows * 8) + 8;
    u8 sprite_tiles = (content_height_px + 15) / 16;
    if (sprite_tiles < PANEL_MIN_ROWS) sprite_tiles = PANEL_MIN_ROWS;
    return sprite_tiles;
}

static u16 get_panel_tile(const NGVisualAsset *asset, u8 col, u8 row) {
    u16 idx = row * asset->width_tiles + col;
    u16 entry = asset->tilemap[idx];
    u16 tile_offset = entry & 0x7FFF;
    return asset->base_tile + tile_offset;
}

static void render_panel_9slice(NGMenu *menu, s16 screen_x, s16 screen_y) {
    const NGVisualAsset *asset = menu->panel_asset;
    u8 target_height = calc_panel_height(menu->item_count);
    u8 orig_height = asset->height_tiles;  // 7 for ui_panel
    u8 extra_rows = (target_height > orig_height) ? (target_height - orig_height) : 0;
    u8 actual_height = orig_height + extra_rows;

    if (actual_height > 32) actual_height = 32;

    menu->panel_height_tiles = actual_height;

    u16 first_sprite = menu->panel_first_sprite;
    u8 palette = asset->palette;
    u8 num_cols = asset->width_tiles;

    s16 y_val = 496 - screen_y;
    if (y_val < 0) y_val += 512;
    y_val &= 0x1FF;

    u16 scb3_val = ((u16)y_val << 7) | (actual_height & 0x3F);

    for (u8 col = 0; col < num_cols; col++) {
        u16 spr = first_sprite + col;

        NG_REG_VRAMADDR = SCB1_BASE + (spr * 64);
        NG_REG_VRAMMOD = 1;

        u8 row_out = 0;
        u16 attr = ((u16)palette << 8) | 0x01;

        for (u8 r = 0; r < PANEL_TOP_ROWS; r++) {
            u16 tile = get_panel_tile(asset, col, r);
            NG_REG_VRAMDATA = tile;
            NG_REG_VRAMDATA = attr;
            row_out++;
        }

        u8 orig_middle_start = PANEL_TOP_ROWS;
        u8 orig_middle_end = orig_height - PANEL_BOTTOM_ROWS;

        for (u8 r = orig_middle_start; r < orig_middle_end; r++) {
            u16 tile = get_panel_tile(asset, col, r);
            NG_REG_VRAMDATA = tile;
            NG_REG_VRAMDATA = attr;
            row_out++;

            if (r == PANEL_MIDDLE_ROW) {
                u16 repeat_tile = get_panel_tile(asset, col, PANEL_MIDDLE_ROW);
                for (u8 e = 0; e < extra_rows; e++) {
                    NG_REG_VRAMDATA = repeat_tile;
                    NG_REG_VRAMDATA = attr;
                    row_out++;
                }
            }
        }

        for (u8 r = orig_height - PANEL_BOTTOM_ROWS; r < orig_height; r++) {
            u16 tile = get_panel_tile(asset, col, r);
            NG_REG_VRAMDATA = tile;
            NG_REG_VRAMDATA = attr;
            row_out++;
        }

        // Clear remaining tile slots (up to 32)
        while (row_out < 32) {
            NG_REG_VRAMDATA = 0;
            NG_REG_VRAMDATA = 0;
            row_out++;
        }

        // Write shrink value to SCB2 (no shrink = 0x0FFF)
        NG_REG_VRAMADDR = SCB2_BASE + spr;
        NG_REG_VRAMDATA = 0x0FFF;

        // Write Y position and height to SCB3
        NG_REG_VRAMADDR = SCB3_BASE + spr;
        NG_REG_VRAMDATA = scb3_val;

        // Write X position to SCB4
        s16 x_pos = screen_x + (col * 16);
        NG_REG_VRAMADDR = SCB4_BASE + spr;
        NG_REG_VRAMDATA = (x_pos & 0x1FF) << 7;
    }
}

static void hide_panel_sprites(NGMenu *menu) {
    if (!menu->panel_sprites_allocated) return;

    for (u8 col = 0; col < PANEL_COLS; col++) {
        u16 spr = menu->panel_first_sprite + col;
        NG_REG_VRAMADDR = SCB3_BASE + spr;
        NG_REG_VRAMDATA = 0;
    }
}

NGMenuHandle NGMenuCreate(NGArena *arena,
                          const NGVisualAsset *panel_asset,
                          const NGVisualAsset *cursor_asset,
                          u8 dim_amount) {
    if (!arena || !panel_asset || !cursor_asset) return 0;

    NGMenu *menu = NG_ARENA_ALLOC(arena, NGMenu);
    if (!menu) return 0;

    menu->panel_asset = panel_asset;
    menu->cursor_asset = cursor_asset;

    menu->panel_first_sprite = 0;
    menu->panel_height_tiles = 0;
    menu->panel_sprites_allocated = 0;

    menu->cursor_actor = NGActorCreate(cursor_asset, 0, 0);

    if (menu->cursor_actor == NG_ACTOR_INVALID) {
        return 0;
    }

    NGActorSetScreenSpace(menu->cursor_actor, 1);

    menu->viewport_x = (320 - panel_asset->width_pixels) / 2;
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

    menu->dim_target = dim_amount;
    menu->dim_current = 0;
    menu->dim_start_pal = 1;
    menu->dim_end_pal = 63;
    menu->panel_pal = panel_asset->palette;
    menu->cursor_pal = cursor_asset->palette;
    menu->pal_backup = 0;
    menu->backup_count = 0;

    if (dim_amount > 0) {
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

    menu->selection = find_first_selectable(menu);

    NGSpringSetTarget(&menu->panel_y_spring, FIX(menu->viewport_y));

    s16 cursor_offset = get_item_y_offset(menu->selection) + MENU_CURSOR_OFFSET_Y;
    NGSpringSnap(&menu->cursor_y_spring, FIX(cursor_offset));
    NGSpringSetTarget(&menu->cursor_y_spring, FIX(cursor_offset));

    if (menu->dim_target > 0) {
        backup_palettes(menu);
        menu->dim_current = 0;
    }

    menu->panel_first_sprite = PANEL_SPRITE_BASE;
    menu->panel_sprites_allocated = 1;

    s16 panel_y = NGSpringGetInt(&menu->panel_y_spring);
    render_panel_9slice(menu, menu->viewport_x, panel_y);

    s16 cursor_x = menu->viewport_x + MENU_TEXT_OFFSET_X * 8 + MENU_CURSOR_OFFSET_X;
    s16 cursor_y = panel_y + NGSpringGetInt(&menu->cursor_y_spring);
    NGActorAddToScene(menu->cursor_actor, FIX(cursor_x), FIX(cursor_y), NG_MENU_Z_INDEX + 1);

    menu->showing = 1;
    menu->text_dirty = 1;
}

void NGMenuHide(NGMenuHandle menu) {
    if (!menu) return;

    menu->visible = 0;

    NGSpringSetTarget(&menu->panel_y_spring, MENU_HIDDEN_OFFSET_FIX);

    if (menu->text_visible) {
        clear_menu_text(menu);
        menu->text_visible = 0;
    }
}

void NGMenuUpdate(NGMenuHandle menu) {
    if (!menu) return;

    NGSpringUpdate(&menu->panel_y_spring);
    NGSpringUpdate(&menu->cursor_y_spring);

    if (menu->dim_target > 0) {
        u8 target = menu->visible ? menu->dim_target : 0;

        if (menu->dim_current < target) {
            menu->dim_current += DIM_ANIM_SPEED;
            if (menu->dim_current > target) menu->dim_current = target;
            apply_dimming(menu);
        } else if (menu->dim_current > target) {
            if (menu->dim_current >= DIM_ANIM_SPEED) {
                menu->dim_current -= DIM_ANIM_SPEED;
            } else {
                menu->dim_current = 0;
            }
            apply_dimming(menu);
        }
    }

    if (!menu->visible && NGSpringSettled(&menu->panel_y_spring) && menu->dim_current == 0) {
        if (menu->showing) {
            restore_palettes(menu);
            hide_panel_sprites(menu);
            menu->panel_sprites_allocated = 0;
            NGActorRemoveFromScene(menu->cursor_actor);
            menu->showing = 0;
        }
        return;
    }

    if (menu->visible) {
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
    if (menu->panel_sprites_allocated) {
        render_panel_9slice(menu, menu->viewport_x, panel_y);
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

void NGMenuDraw(NGMenuHandle menu) {
    if (!menu || !menu->showing) return;

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
    if (!menu) return 0;
    return menu->showing;
}

u8 NGMenuIsAnimating(NGMenuHandle menu) {
    if (!menu) return 0;
    // Animating if spring not settled OR dimming is still transitioning
    u8 target = menu->visible ? menu->dim_target : 0;
    return !NGSpringSettled(&menu->panel_y_spring) || menu->dim_current != target;
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

    if (menu->dim_current > 0) {
        restore_palettes(menu);
    }

    if (menu->text_visible) {
        clear_menu_text(menu);
    }

    hide_panel_sprites(menu);
    menu->panel_sprites_allocated = 0;

    if (menu->cursor_actor != NG_ACTOR_INVALID) {
        NGActorDestroy(menu->cursor_actor);
    }
}
