/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/*
 * Entity pools and depth sorting.
 *
 * A floor plane you can walk around. Moving up and down the floor changes who
 * is in front of whom, and the enemies and hit sparks come from pools rather
 * than hand-managed arrays - the two pieces a beat-em-up is built on.
 *
 * Everything that would normally be bookkeeping is gone: no free-slot search,
 * no alive flags, no skipping dead entries, no z-order to keep in step with
 * movement. Compare the loops below with the same work in the tilemap demo.
 */

#include "brawler.h"
#include "../demo_ids.h"
#include <camera.h>
#include <debug.h>
#include <engine.h>
#include <hitbox.h>
#include <ng_arena.h>
#include <ng_audio.h>
#include <ng_color.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <ng_palette.h>
#include <pool.h>
#include <scene.h>
#include <ui.h>
#include <progear_assets.h>

#define FLOOR_TOP    FIX(120) /* Nearest and furthest the player may stand */
#define FLOOR_BOTTOM FIX(200)
#define FLOOR_LEFT   FIX(24)
#define FLOOR_RIGHT  FIX(296)

#define WALK_SPEED  FIX(1.5)
#define ENEMY_SPEED FIX(0.35)
#define PUNCH_REACH FIX(26)
#define SPARK_LIFE  14
#define MAX_ENEMIES 5
#define MAX_SPARKS  4

/* Pool entities begin with their actor handle - the pool fills it in. */
typedef struct {
    NGActorHandle actor;
    fixed x, y;
    s8 dir;
    u8 hp;
} Enemy;

typedef struct {
    NGActorHandle actor;
    u8 life;
} Spark;

typedef struct BrawlerState {
    NGMenuHandle menu;
    NGActorHandle player;
    fixed player_x, player_y;
    s8 facing;
    u8 menu_open;
    u8 switch_target;
    u8 defeated;
    u16 respawn_timer;
    NGPool enemies;
    NGPool sparks;
} BrawlerState;

static BrawlerState *state;

#define MENU_RESUME  0
#define MENU_TILEMAP 1
#define MENU_BALL    2

static void draw_hud(void) {
    NGTextPrintf(NGFixLayoutOffset(NG_ALIGN_RIGHT, NG_ALIGN_TOP, -1, 1), 0, "KO %d",
                 state->defeated);
}

/* Spawn an enemy at a random-ish spot on the floor. */
static void spawn_enemy(u16 seed) {
    Enemy *e = NGPoolSpawn(&state->enemies);
    if (!e) {
        return;
    }
    e->x = FLOOR_LEFT + FIX((seed * 53) % 250);
    e->y = FLOOR_TOP + FIX((seed * 31) % 78);
    e->dir = (seed & 1) ? 1 : -1;
    e->hp = 2;

    NGActorSetAnimByName(e->actor, "walk");
    NGActorSetPos(e->actor, e->x, e->y);
}

static void spawn_spark(fixed x, fixed y) {
    Spark *s = NGPoolSpawn(&state->sparks);
    if (!s) {
        return;
    }
    s->life = SPARK_LIFE;
    NGActorSetPos(s->actor, x, y);
}

void BrawlerInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, BrawlerState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->defeated = 0;
    state->facing = 1;
    state->respawn_timer = 0;

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
    NGPalSetBackdrop(NG_RGB8(40, 32, 56));

    /* Player: anchored at the feet, so its position is where it stands and
     * that same value sorts its depth. */
    state->player_x = FIX(160);
    state->player_y = FIX(170);
    state->player = NGActorCreate(&NGVisualAsset_ball, 0, 0);
    NGActorSetAnchor(state->player, NG_ANCHOR_BOTTOM);
    NGActorSetDepthFromY(state->player, 1);
    NGActorAddToScene(state->player, state->player_x, state->player_y, 0);

    /* Pools create their actors once, hidden. Spawning never allocates. */
    NGPoolConfig enemies = {
        .arena = &ng_arena_state,
        .stride = sizeof(Enemy),
        .capacity = MAX_ENEMIES,
        .asset = &NGVisualAsset_walker,
        .anchor = NG_ANCHOR_BOTTOM,
    };
    NGPoolInit(&state->enemies, &enemies);

    NGPoolConfig sparks = {
        .arena = &ng_arena_state,
        .stride = sizeof(Spark),
        .capacity = MAX_SPARKS,
        .asset = &NGVisualAsset_bullet,
        .anchor = NG_ANCHOR_CENTER,
    };
    NGPoolInit(&state->sparks, &sparks);

    for (u8 i = 0; i < MAX_ENEMIES; i++) {
        spawn_enemy((u16)(i * 7 + 3));
    }

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "BRAWLER");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Tilemap Demo");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "BRAWLER");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0,
                "DPAD:WALK  A:PUNCH  UP/DOWN SORTS DEPTH");
    draw_hud();

    NGDebugSetEnabled(1);
    NGDebugResetPeaks();
}

/* Enemies drift along the floor and turn at its edges. */
static void update_enemies(void) {
    NG_POOL_FOREACH(Enemy, e, &state->enemies) {
        e->x += e->dir > 0 ? ENEMY_SPEED : -ENEMY_SPEED;
        if (e->x > FLOOR_RIGHT) {
            e->dir = -1;
        } else if (e->x < FLOOR_LEFT) {
            e->dir = 1;
        }
        NGActorSetHFlip(e->actor, e->dir < 0);
        NGActorSetPos(e->actor, e->x, e->y);
    }
}

static void update_sparks(void) {
    NG_POOL_FOREACH(Spark, s, &state->sparks) {
        if (--s->life == 0) {
            NGPoolRetire(&state->sparks, s);
        }
    }
}

/* A punch reaches in front of the player and only connects with enemies
 * standing at roughly the same depth - the rule that makes a floor plane feel
 * like a floor rather than a flat wall. */
static void punch(void) {
    fixed reach_x = state->player_x + (state->facing > 0 ? PUNCH_REACH : -PUNCH_REACH);

    NG_POOL_FOREACH(Enemy, e, &state->enemies) {
        fixed dx = e->x > reach_x ? e->x - reach_x : reach_x - e->x;
        fixed dy = e->y > state->player_y ? e->y - state->player_y : state->player_y - e->y;
        if (dx > FIX(20) || dy > FIX(10)) {
            continue;
        }

        spawn_spark(e->x, e->y - FIX(16));
        if (--e->hp == 0) {
            NGPoolRetire(&state->enemies, e);
            state->defeated++;
            draw_hud();
        }
    }
    NGSfxPlay(NGSFX_BALL_HIT);
}

u8 BrawlerUpdate(void) {
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (state->menu_open) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        } else {
            NGMenuShow(state->menu);
            state->menu_open = 1;
        }
    }

    NGMenuUpdate(state->menu);

    if (state->menu_open) {
        if (NGMenuConfirmed(state->menu)) {
            switch (NGMenuGetSelection(state->menu)) {
                case MENU_RESUME:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    break;
                case MENU_TILEMAP:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_TILEMAP;
                    break;
                case MENU_BALL:
                    NGMenuHide(state->menu);
                    state->menu_open = 0;
                    state->switch_target = DEMO_ID_BALL;
                    break;
            }
        }
        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            state->menu_open = 0;
        }
        return state->switch_target;
    }

    /* Walk the floor plane. Vertical movement is depth, not height. */
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
        state->player_x -= WALK_SPEED;
        state->facing = -1;
    }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
        state->player_x += WALK_SPEED;
        state->facing = 1;
    }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP)) {
        state->player_y -= WALK_SPEED;
    }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN)) {
        state->player_y += WALK_SPEED;
    }

    if (state->player_x < FLOOR_LEFT)
        state->player_x = FLOOR_LEFT;
    if (state->player_x > FLOOR_RIGHT)
        state->player_x = FLOOR_RIGHT;
    if (state->player_y < FLOOR_TOP)
        state->player_y = FLOOR_TOP;
    if (state->player_y > FLOOR_BOTTOM)
        state->player_y = FLOOR_BOTTOM;

    NGActorSetHFlip(state->player, state->facing < 0);
    NGActorSetPos(state->player, state->player_x, state->player_y);

    if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
        punch();
    }

    update_enemies();
    update_sparks();

    /* Keep the floor populated so there is always something to sort against */
    if (NGPoolCount(&state->enemies) < MAX_ENEMIES) {
        if (++state->respawn_timer > 90) {
            state->respawn_timer = 0;
            spawn_enemy((u16)(state->defeated * 11 + 5));
        }
    }

    NGDebugDrawHUD(26);
    return state->switch_target;
}

void BrawlerCleanup(void) {
    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 4, 40, 1);
    NGFixClear(0, 26, 40, 1);
    NGFixClear(0, 27, 40, 1);

    NGPoolRetireAll(&state->enemies);
    NGPoolRetireAll(&state->sparks);

    NGActorRemoveFromScene(state->player);
    NGActorDestroy(state->player);

    NGMenuDestroy(state->menu);
    NGSceneReset();
    NGPalSetBackdrop(NG_COLOR_BLACK);
}
