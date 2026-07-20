/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file scenemenu.h
 * @brief A pause menu that can reach every registered scene.
 *
 * Games end up writing the same pause menu in every screen: a few options
 * belonging to that screen, then some way to get to the others. Wiring the
 * "some way to get to the others" part by hand means each screen knows about
 * a hand-picked subset, and a new screen is only reachable from wherever
 * somebody remembered to add it.
 *
 * NGSceneMenu takes that over. A screen declares its own options and nothing
 * else; the menu appends a separator and a "Scenes" entry that opens the full
 * list from NGStateRegister(). The list pages itself when it does not fit -
 * "More Scenes" to go on, "Back" to return - so adding a scene anywhere makes
 * it reachable from everywhere with no further wiring.
 *
 * @code
 * // setup
 * NGSceneMenuInit(&sm, menu, "BALL DEMO");
 * NGSceneMenuAddAction(&sm, ACT_RESUME, "Resume");
 * NGSceneMenuAddAction(&sm, ACT_MUSIC,  "Pause Music");
 * NGSceneMenuBuild(&sm);
 *
 * // each frame, while the menu is open
 * NGSceneMenuEvent ev = NGSceneMenuUpdate(&sm);
 * if (ev.kind == NG_SCENE_MENU_ACTION) {
 *     handle(ev.action);           // one of ours
 * } else if (ev.kind == NG_SCENE_MENU_SWITCH) {
 *     return ev.scene;             // go there
 * }
 * @endcode
 */

#ifndef NG_SCENEMENU_H
#define NG_SCENEMENU_H

#include <ng_types.h>
#include <ui.h>

/**
 * @defgroup scenemenu Scene Menu
 * @ingroup sdk
 * @brief Pause menu with built-in navigation to every registered scene.
 * @{
 */

/** Most scene-specific options one menu can hold. */
#define NG_SCENE_MENU_MAX_ACTIONS 8

/**
 * Scenes listed per page.
 *
 * The panel grows with its contents and the fix layer is only so tall, so a
 * long list has to page. Six leaves room for the separator and the
 * "More Scenes"/"Back" entry within a panel that still fits comfortably.
 */
#define NG_SCENE_MENU_PAGE 6

/** What a frame of menu interaction produced. */
typedef enum {
    NG_SCENE_MENU_NONE = 0, /**< Nothing chosen, or an internal page change */
    NG_SCENE_MENU_ACTION,   /**< One of the screen's own options */
    NG_SCENE_MENU_SWITCH,   /**< A scene was chosen */
    NG_SCENE_MENU_CLOSED    /**< Cancelled out of the root page */
} NGSceneMenuEventKind;

/** Result of NGSceneMenuUpdate(). */
typedef struct {
    NGSceneMenuEventKind kind;
    u8 action; /**< Valid when kind is NG_SCENE_MENU_ACTION */
    u8 scene;  /**< Valid when kind is NG_SCENE_MENU_SWITCH */
} NGSceneMenuEvent;

/** Menu state. Treat as opaque; it lives in the game's own state struct. */
typedef struct {
    NGMenuHandle menu;
    const char *title;
    const char *labels[NG_SCENE_MENU_MAX_ACTIONS];
    u8 actions[NG_SCENE_MENU_MAX_ACTIONS];
    u8 action_count;
    u8 page;  /**< 0 = root (the screen's options), 1+ = scene list page */
    u8 first; /**< Index into the scene list shown at the top of this page */
} NGSceneMenu;

/** @name Setup */
/** @{ */

/**
 * Bind a scene menu to an existing NGMenu.
 *
 * @param sm Scene menu to initialise
 * @param menu Menu to drive
 * @param title Title shown on the root page
 */
void NGSceneMenuInit(NGSceneMenu *sm, NGMenuHandle menu, const char *title);

/**
 * Add one of this screen's own options.
 *
 * @param sm Scene menu
 * @param action Value handed back in NGSceneMenuEvent::action when chosen
 * @param label Text to show
 */
void NGSceneMenuAddAction(NGSceneMenu *sm, u8 action, const char *label);

/**
 * Populate the menu with the root page: this screen's options, then a
 * separator and the entry into the scene list.
 *
 * Call once after adding actions, and again if the options change.
 *
 * @param sm Scene menu
 */
void NGSceneMenuBuild(NGSceneMenu *sm);
/** @} */

/** @name Runtime */
/** @{ */

/**
 * Advance the menu and report what the player chose.
 *
 * Paging and returning are handled internally and reported as
 * NG_SCENE_MENU_NONE, so a caller only ever sees its own actions, a scene to
 * switch to, or the menu being cancelled.
 *
 * Call once per frame while the menu is showing, after NGMenuUpdate().
 *
 * @param sm Scene menu
 * @return What happened this frame
 */
NGSceneMenuEvent NGSceneMenuUpdate(NGSceneMenu *sm);

/**
 * Return to the root page.
 *
 * Worth calling when the menu is hidden, so reopening it starts at the
 * screen's own options rather than wherever the player last browsed to.
 *
 * @param sm Scene menu
 */
void NGSceneMenuReset(NGSceneMenu *sm);
/** @} */

/** @} */ /* end of scenemenu group */

#endif /* NG_SCENEMENU_H */
