/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <scenemenu.h>
#include <state.h>

#include "sdk_internal.h"

/*
 * Page 0 is the screen's own options plus a way into the scene list. Pages 1
 * and up are slices of the scene list, each ending in the next slice or, on
 * the last one, nothing further. Every list page can go back.
 *
 * Only the current page's items exist in the NGMenu at any time, so the panel
 * is always the size of what it is showing.
 */

/* Labels for the navigation entries the menu adds itself */
static const char *SEPARATOR = "--------";
static const char *LABEL_SCENES = "Scenes";
static const char *LABEL_MORE = "More Scenes";
static const char *LABEL_BACK = "Back";

/* How many scenes this page shows, and whether another page follows */
static u8 page_scene_count(const NGSceneMenu *sm, u8 *out_has_more) {
    u8 total = NGStateCount();
    u8 remaining = (sm->first < total) ? (u8)(total - sm->first) : 0;
    u8 shown = (remaining > NG_SCENE_MENU_PAGE) ? NG_SCENE_MENU_PAGE : remaining;
    if (out_has_more) {
        *out_has_more = (remaining > shown) ? 1 : 0;
    }
    return shown;
}

static void build_root(NGSceneMenu *sm) {
    NGMenuClearItems(sm->menu);
    NGMenuSetTitle(sm->menu, sm->title);

    for (u8 i = 0; i < sm->action_count; i++) {
        NGMenuAddItem(sm->menu, sm->labels[i]);
    }
    NGMenuAddSeparator(sm->menu, SEPARATOR);
    NGMenuAddItem(sm->menu, LABEL_SCENES);
}

static void build_scene_page(NGSceneMenu *sm) {
    NGMenuClearItems(sm->menu);
    NGMenuSetTitle(sm->menu, LABEL_SCENES);

    u8 has_more = 0;
    u8 shown = page_scene_count(sm, &has_more);

    for (u8 i = 0; i < shown; i++) {
        u8 id = NGStateIdAt((u8)(sm->first + i));
        const char *name = NGStateGetName(id);
        NGMenuAddItem(sm->menu, name ? name : "?");
    }

    NGMenuAddSeparator(sm->menu, SEPARATOR);
    if (has_more) {
        NGMenuAddItem(sm->menu, LABEL_MORE);
    }
    NGMenuAddItem(sm->menu, LABEL_BACK);
}

void NGSceneMenuInit(NGSceneMenu *sm, NGMenuHandle menu, const char *title) {
    if (!sm)
        return;
    sm->menu = menu;
    sm->title = title;
    sm->action_count = 0;
    sm->page = 0;
    sm->first = 0;
}

void NGSceneMenuAddAction(NGSceneMenu *sm, u8 action, const char *label) {
    if (!sm || sm->action_count >= NG_SCENE_MENU_MAX_ACTIONS)
        return;
    sm->labels[sm->action_count] = label;
    sm->actions[sm->action_count] = action;
    sm->action_count++;
}

void NGSceneMenuBuild(NGSceneMenu *sm) {
    if (!sm || !sm->menu)
        return;
    sm->page = 0;
    sm->first = 0;
    build_root(sm);
}

void NGSceneMenuReset(NGSceneMenu *sm) {
    if (!sm || !sm->menu)
        return;
    if (sm->page != 0) {
        sm->page = 0;
        sm->first = 0;
        build_root(sm);
    }
}

NGSceneMenuEvent NGSceneMenuUpdate(NGSceneMenu *sm) {
    NGSceneMenuEvent ev = {NG_SCENE_MENU_NONE, 0, NG_STATE_ID_NONE};
    if (!sm || !sm->menu)
        return ev;

    /* Cancel steps back a page, and only closes from the root. */
    if (NGMenuCancelled(sm->menu)) {
        if (sm->page == 0) {
            ev.kind = NG_SCENE_MENU_CLOSED;
        } else {
            sm->page = 0;
            sm->first = 0;
            build_root(sm);
        }
        return ev;
    }

    if (!NGMenuConfirmed(sm->menu))
        return ev;

    u8 sel = NGMenuGetSelection(sm->menu);

    if (sm->page == 0) {
        if (sel < sm->action_count) {
            ev.kind = NG_SCENE_MENU_ACTION;
            ev.action = sm->actions[sel];
            return ev;
        }
        /* Past the actions lies the separator, then "Scenes" */
        sm->page = 1;
        sm->first = 0;
        build_scene_page(sm);
        return ev;
    }

    u8 has_more = 0;
    u8 shown = page_scene_count(sm, &has_more);

    if (sel < shown) {
        ev.kind = NG_SCENE_MENU_SWITCH;
        ev.scene = NGStateIdAt((u8)(sm->first + sel));
        return ev;
    }

    /* sel == shown is the separator, which cannot be selected, so the
     * entries after it are "More Scenes" (when present) then "Back". */
    if (has_more && sel == (u8)(shown + 1)) {
        sm->first = (u8)(sm->first + shown);
        build_scene_page(sm);
        return ev;
    }

    sm->page = 0;
    sm->first = 0;
    build_root(sm);
    return ev;
}
