/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/*
 * Entity pools, depth sorting, and the wave loop a beat-em-up is paced by.
 *
 * The level is a long strip of ground. While a wave is alive the camera is
 * locked to an arena and the player cannot leave it; clear the wave and an
 * arrow appears, the lock releases, and walking right scrolls to the next
 * arena, where the camera locks again and the next group walks on.
 *
 * The bookkeeping a game would normally carry is gone: entities come from
 * pools rather than hand-managed arrays, and depth is derived from position
 * rather than maintained by hand.
 */

#include "brawler.h"
#include "../demo_ids.h"
#include <backdrop.h>
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

/* The world is several screens wide; the floor is the band fighters stand on */
#define LEVEL_WIDTH  1200
#define FLOOR_TOP    FIX(140)
#define FLOOR_BOTTOM FIX(196)
#define GROUND_Y     132 /* top edge of the paving, in screen pixels */

#define WALK_SPEED   FIX(1.6)
#define ENEMY_SPEED  FIX(0.45)
#define PUNCH_FRAMES 10
#define SPARK_LIFE   12
#define MAX_ENEMIES  4
#define MAX_SPARKS   4
#define ENEMY_HP     2

/* Arenas: where the camera locks and a wave is fought */
#define ARENA_COUNT  3
#define ARENA_MARGIN 24 /* how close to the screen edge the player may get */

#define WAVE_CLEAR_PAUSE 45
#define WAVE_ENTER_STEP  20
#define ARROW_BLINK      20

typedef enum {
    WAVE_FIGHTING, /* camera locked, group alive */
    WAVE_CLEARED,  /* beat after the last enemy falls */
    WAVE_ADVANCE   /* lock released; walk right to the next arena */
} WavePhase;

/* Pool entities begin with their actor handle - the pool fills it in. */
typedef struct {
    NGActorHandle actor;
    fixed x, y;
    s8 dir;
    u8 hp;
    u8 flash;
} Enemy;

typedef struct {
    NGActorHandle actor;
    u8 life;
} Spark;

typedef struct BrawlerState {
    NGMenuHandle menu;
    NGActorHandle player;
    NGBackdropHandle sky_far, sky_near, ground;

    fixed player_x, player_y;
    s8 facing;
    u8 punch_timer;
    u8 walking;

    u8 menu_open;
    u8 switch_target;
    u8 defeated;

    u8 arena;
    WavePhase phase;
    u16 phase_timer;
    u8 pending; /* enemies still to walk on */
    u8 enter_timer;
    u8 arrow_timer;

    NGPool enemies;
    NGPool sparks;
} BrawlerState;

static BrawlerState *state;

#define MENU_RESUME  0
#define MENU_TILEMAP 1
#define MENU_BALL    2

/* Left edge of an arena's camera lock */
static fixed arena_camera_x(u8 arena) {
    return FIX((s16)arena * 320);
}

static void draw_hud(void) {
    NGTextPrintf(NGFixLayoutOffset(NG_ALIGN_RIGHT, NG_ALIGN_TOP, -1, 1), 0, "AREA %d  KO %d",
                 state->arena + 1, state->defeated);
}

/* ---------------------------------------------------------------- spawning */

static void spawn_enemy(u16 seed) {
    Enemy *e = NGPoolSpawn(&state->enemies);
    if (!e) {
        return;
    }
    /* Walk on from one side of the current arena */
    fixed cam = arena_camera_x(state->arena);
    u8 from_left = (u8)(seed & 1);
    e->x = cam + (from_left ? FIX(-16) : FIX(336));
    e->y = FLOOR_TOP + FIX((seed * 37) % 52);
    e->dir = from_left ? 1 : -1;
    e->hp = ENEMY_HP;
    e->flash = 0;

    NGActorSetPalette(e->actor, NGPAL_FIGHTER_ENEMY);
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

/* Nothing is alive between waves, so the pools can simply be emptied. */
static void begin_wave(u8 arena) {
    NGPoolRetireAll(&state->enemies);
    NGPoolRetireAll(&state->sparks);

    state->arena = arena;
    state->phase = WAVE_FIGHTING;
    state->pending = (u8)(2 + (arena < 2 ? arena : 2));
    if (state->pending > MAX_ENEMIES) {
        state->pending = MAX_ENEMIES;
    }
    state->enter_timer = 0;
    draw_hud();
}

/* ---------------------------------------------------------------- updates  */

static void update_enemies(void) {
    NG_POOL_FOREACH(Enemy, e, &state->enemies) {
        if (e->flash && --e->flash == 0) {
            NGActorSetPalette(e->actor, NGPAL_FIGHTER_ENEMY);
        }

        /* Close on the player rather than patrolling blindly */
        e->dir = (e->x < state->player_x) ? 1 : -1;
        fixed gap = (e->x > state->player_x) ? e->x - state->player_x : state->player_x - e->x;
        if (gap > FIX(26)) {
            e->x += e->dir > 0 ? ENEMY_SPEED : -ENEMY_SPEED;
        }
        if (e->y < state->player_y - FIX(2)) {
            e->y += FIX(0.25);
        } else if (e->y > state->player_y + FIX(2)) {
            e->y -= FIX(0.25);
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

/*
 * The punch frame carries the hitbox, so contact is a box test rather than a
 * distance check. Depth still has to agree, or a punch would land on someone
 * standing well behind - that check is what makes the floor read as a floor.
 */
static void resolve_punch(void) {
    NGRect fist;
    if (!NGActorGetBox(state->player, NG_BOX_HIT, 0, &fist)) {
        return;
    }

    NG_POOL_FOREACH(Enemy, e, &state->enemies) {
        NGRect hurt;
        if (!NGActorGetBox(e->actor, NG_BOX_HURT, 0, &hurt)) {
            continue;
        }
        fixed dy = (e->y > state->player_y) ? e->y - state->player_y : state->player_y - e->y;
        if (dy > FIX(12) || !NGRectOverlap(&fist, &hurt, 0)) {
            continue;
        }
        if (NGCombatStrike(state->player, e->actor, 0) != NG_STRIKE_HIT) {
            continue;
        }

        spawn_spark(e->x, e->y - FIX(26));
        NGSfxPlay(NGSFX_BALL_HIT);
        e->flash = 4;
        NGActorSetPalette(e->actor, NGPAL_BALL_WHITE);
        e->x += (state->facing > 0) ? FIX(8) : FIX(-8);

        if (--e->hp == 0) {
            NGPoolRetire(&state->enemies, e);
            state->defeated++;
            draw_hud();
        }
    }
}

static void update_wave(void) {
    switch (state->phase) {
        case WAVE_FIGHTING:
            if (state->pending > 0) {
                if (state->enter_timer > 0) {
                    state->enter_timer--;
                } else {
                    spawn_enemy((u16)(state->arena * 13 + state->pending));
                    state->pending--;
                    state->enter_timer = WAVE_ENTER_STEP;
                }
            } else if (NGPoolCount(&state->enemies) == 0) {
                state->phase = WAVE_CLEARED;
                state->phase_timer = WAVE_CLEAR_PAUSE;
            }
            break;

        case WAVE_CLEARED:
            if (--state->phase_timer == 0) {
                if (state->arena + 1 < ARENA_COUNT) {
                    state->phase = WAVE_ADVANCE;
                    state->arrow_timer = 0;
                } else {
                    begin_wave(state->arena); /* last arena keeps them coming */
                }
            }
            break;

        case WAVE_ADVANCE:
            /* Reaching the next arena re-locks the camera and starts its wave */
            if (NGCameraGetX() >= arena_camera_x((u8)(state->arena + 1))) {
                begin_wave((u8)(state->arena + 1));
            }
            break;
    }
}

/* ---------------------------------------------------------------- lifecycle */

void BrawlerInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, BrawlerState);
    state->switch_target = 0;
    state->menu_open = 0;
    state->defeated = 0;
    state->facing = 1;
    state->punch_timer = 0;
    state->walking = 0;
    state->arrow_timer = 0;
    state->phase_timer = 0;

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
    NGCameraSetBounds(LEVEL_WIDTH, 224);
    NGPalSetBackdrop(NG_RGB8(72, 132, 196));

    /* Sky: two cloud bands at different parallax rates, reused from the
     * tilemap demo. The paving is a third layer tracking the camera one to
     * one, so it reads as the plane the fighters stand on. */
    state->sky_far =
        NGBackdropCreate(&NGVisualAsset_cloud_far, NG_BACKDROP_WIDTH_INFINITE, 0, FIX(0.15), 0);
    NGBackdropAddToScene(state->sky_far, 0, 16, 0);

    state->sky_near =
        NGBackdropCreate(&NGVisualAsset_cloud_near, NG_BACKDROP_WIDTH_INFINITE, 0, FIX(0.45), 0);
    NGBackdropAddToScene(state->sky_near, 0, 70, 1);

    state->ground = NGBackdropCreate(&NGVisualAsset_ground, NG_BACKDROP_WIDTH_INFINITE,
                                     224 - GROUND_Y, FIX(1.0), 0);
    NGBackdropAddToScene(state->ground, 0, GROUND_Y, 2);

    /* Player: anchored at the feet, so one coordinate both places it and
     * sorts its depth against the enemies. */
    state->player_x = FIX(120);
    state->player_y = FIX(176);
    state->player = NGActorCreate(&NGVisualAsset_fighter, 0, 0);
    NGActorSetAnchor(state->player, NG_ANCHOR_BOTTOM);
    NGActorSetDepthFromY(state->player, 1);
    NGActorAddToScene(state->player, state->player_x, state->player_y, 0);

    NGPoolConfig enemies = {
        .arena = &ng_arena_state,
        .stride = sizeof(Enemy),
        .capacity = MAX_ENEMIES,
        .asset = &NGVisualAsset_fighter,
        .anchor = NG_ANCHOR_BOTTOM,
        .depth_from_y = 1,
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

    begin_wave(0);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "BRAWLER");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Tilemap Demo");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "BRAWLER");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0, "DPAD:WALK  A:PUNCH");

    NGDebugSetEnabled(1);
    NGDebugResetPeaks();
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

    /* --- movement ------------------------------------------------------ */
    u8 was_walking = state->walking;
    state->walking = 0;

    if (state->punch_timer == 0) {
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
            state->player_x -= WALK_SPEED;
            state->facing = -1;
            state->walking = 1;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
            state->player_x += WALK_SPEED;
            state->facing = 1;
            state->walking = 1;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP)) {
            state->player_y -= WALK_SPEED;
            state->walking = 1;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN)) {
            state->player_y += WALK_SPEED;
            state->walking = 1;
        }
    }

    if (state->player_y < FLOOR_TOP)
        state->player_y = FLOOR_TOP;
    if (state->player_y > FLOOR_BOTTOM)
        state->player_y = FLOOR_BOTTOM;

    /* While a wave is alive the arena holds the player; once it is cleared
     * the right-hand wall opens and the camera follows them onward. */
    fixed cam_left = arena_camera_x(state->arena);
    fixed min_x = cam_left + FIX(ARENA_MARGIN);
    fixed max_x = (state->phase == WAVE_ADVANCE) ? FIX(LEVEL_WIDTH - ARENA_MARGIN)
                                                 : cam_left + FIX(320 - ARENA_MARGIN);
    if (state->player_x < min_x)
        state->player_x = min_x;
    if (state->player_x > max_x)
        state->player_x = max_x;

    if (state->phase == WAVE_ADVANCE) {
        /* Camera trails the player and never scrolls back */
        fixed want = state->player_x - FIX(160);
        if (want > NGCameraGetX()) {
            NGCameraSetPos(want, 0);
        }
    } else {
        NGCameraSetPos(cam_left, 0);
    }

    /* --- punch --------------------------------------------------------- */
    if (state->punch_timer > 0) {
        state->punch_timer--;
        resolve_punch();
        if (state->punch_timer == 0) {
            NGActorSetAnimByName(state->player, "idle");
        }
    } else if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
        state->punch_timer = PUNCH_FRAMES;
        NGActorSetAnimByName(state->player, "punch");
        NGCombatBeginAttack(state->player);
        NGSfxPlay(NGSFX_PROGEARSDK_UI_CLICK);
    } else if (state->walking != was_walking) {
        NGActorSetAnimByName(state->player, state->walking ? "walk" : "idle");
    }

    NGActorSetHFlip(state->player, state->facing < 0);
    NGActorSetPos(state->player, state->player_x, state->player_y);

    update_enemies();
    update_sparks();
    update_wave();

    /* --- forward prompt ------------------------------------------------ */
    /* Written every frame either way: the fix layer keeps whatever was last
     * put there, so a prompt drawn once would never clear itself. */
    state->arrow_timer++;
    u8 show_arrow = (state->phase == WAVE_ADVANCE) && (u8)((state->arrow_timer / ARROW_BLINK) & 1);
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_RIGHT, NG_ALIGN_MIDDLE, -2, 0), 0,
                show_arrow ? ">>>" : "   ");

    NGDebugDrawHUD(26);
    return state->switch_target;
}

void BrawlerCleanup(void) {
    /* The fix layer is not part of the scene and survives NGSceneReset, so
     * everything this demo wrote has to be cleared by hand. */
    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 4, 40, 1);
    NGFixClear(0, 15, 40, 1);
    NGFixClear(0, 26, 40, 1);
    NGFixClear(0, 27, 40, 1);

    NGPoolRetireAll(&state->enemies);
    NGPoolRetireAll(&state->sparks);

    NGActorRemoveFromScene(state->player);
    NGActorDestroy(state->player);

    NGBackdropRemoveFromScene(state->ground);
    NGBackdropDestroy(state->ground);
    NGBackdropRemoveFromScene(state->sky_near);
    NGBackdropDestroy(state->sky_near);
    NGBackdropRemoveFromScene(state->sky_far);
    NGBackdropDestroy(state->sky_far);

    NGMenuDestroy(state->menu);
    NGSceneReset();
    NGCameraSetPos(0, 0);
    NGPalSetBackdrop(NG_COLOR_BLACK);
}
