/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include "tilemap_demo.h"
#include "../demo_ids.h"
#include <ng_hardware.h>
#include <ng_fix.h>
#include <ng_input.h>
#include <scene.h>
#include <camera.h>
#include <ng_palette.h>
#include <ng_color.h>
#include <ui.h>
#include <ng_arena.h>
#include <actor.h>
#include <backdrop.h>
#include <engine.h>
#include <ng_audio.h>
#include <hitbox.h>
#include <progear_assets.h>

#define PLAYER_HALF_W FIX(6)
#define PLAYER_HALF_H FIX(12)
#define PLAYER_SPEED  FIX(2)

// Tuned jump physics (inspired by Celeste/Hollow Knight)
#define JUMP_FORCE         FIX(-6.5) // Initial jump velocity
#define GRAVITY_UP         FIX(0.35) // Gravity while rising
#define GRAVITY_DOWN       FIX(0.55) // Gravity while falling (faster)
#define JUMP_CUT_MULT      FIX(0.4)  // Velocity multiplier when releasing jump early
#define MAX_FALL_SPEED     FIX(10)   // Terminal velocity
#define COYOTE_FRAMES      6         // Frames after leaving ground you can still jump
#define JUMP_BUFFER_FRAMES 6         // Frames before landing a jump press is remembered

/* --- Hitbox showcase ---------------------------------------------------
 * B jumps, A shoots. Bullets carry NG_BOX_HIT and walkers NG_BOX_HURT, so a
 * shot is just NGCombatStrike(). A stomp uses the walker's user1 box - the
 * strip across its shoulders - tested against the player's feet, which is why
 * landing on one kills it while walking into it does not.
 */
#define MAX_BULLETS     4
#define MAX_WALKERS     3
#define BULLET_SPEED    FIX(4)
#define BULLET_LIFETIME 70
#define WALKER_SPEED    FIX(0.4)
#define WALKER_RANGE    FIX(48)
#define STOMP_BOUNCE    FIX(-4.0)

typedef struct {
    NGActorHandle actor;
    fixed x, y;
    s8 dir;
    u8 alive;
    u8 life;
} Bullet;

typedef struct {
    NGActorHandle actor;
    fixed x, y;
    fixed home_x;
    s8 dir;
    u8 alive;
} Walker;

typedef struct TilemapDemoState {
    NGMenuHandle menu;
    NGActorHandle player;
    NGBackdropHandle clouds_far;
    NGBackdropHandle clouds_mid;
    NGBackdropHandle clouds_near;
    u16 level_width;
    u16 level_height;
    fixed player_x;
    fixed player_y;
    fixed player_vel_x;
    fixed player_vel_y;
    u8 menu_open;
    u8 switch_target;
    u8 on_ground;
    u8 coyote_timer;
    u8 jump_buffer;
    u8 jumping;
    s8 facing;
    Bullet bullets[MAX_BULLETS];
    Walker walkers[MAX_WALKERS];
    u8 defeated;
} TilemapDemoState;

static TilemapDemoState *state;

#define MENU_RESUME      0
#define MENU_BALL_DEMO   1
#define MENU_SCROLL_DEMO 2

/* The menu panel, terrain and all three cloud layers can stack past the
 * hardware's 96 sprites per scanline, which drops the highest-numbered
 * sprites - the UI panel. Hide the widest cloud layer while the menu is
 * open to stay under the limit. */
static void set_menu_open(u8 open) {
    state->menu_open = open;
    NGBackdropSetVisible(state->clouds_near, !open);
}

/* Fire from the player's centre in the direction last moved. */
static void fire_bullet(void) {
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &state->bullets[i];
        if (b->alive)
            continue;
        b->x = state->player_x;
        b->y = state->player_y;
        b->dir = state->facing;
        b->life = BULLET_LIFETIME;
        b->alive = 1;
        NGActorSetVisible(b->actor, 1);
        NGActorSetPos(b->actor, b->x, b->y);
        /* A fresh attack instance, so this bullet can hit a walker that a
         * previous bullet already struck. */
        NGCombatBeginAttack(b->actor);
        NGSfxPlay(NGSFX_BALL_HIT);
        return;
    }
}

static void retire_bullet(Bullet *b) {
    b->alive = 0;
    NGActorSetVisible(b->actor, 0);
    NGActorSetPos(b->actor, FIX(-64), FIX(-64));
}

static void update_bullets(void) {
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &state->bullets[i];
        if (!b->alive)
            continue;
        b->x += b->dir > 0 ? BULLET_SPEED : -BULLET_SPEED;
        if (--b->life == 0 || b->x < 0 || b->x > FIX(state->level_width)) {
            retire_bullet(b);
            continue;
        }
        NGActorSetPos(b->actor, b->x, b->y);
    }
}

static void update_walkers(void) {
    for (u8 i = 0; i < MAX_WALKERS; i++) {
        Walker *w = &state->walkers[i];
        if (!w->alive)
            continue;
        w->x += w->dir > 0 ? WALKER_SPEED : -WALKER_SPEED;
        if (w->x > w->home_x + WALKER_RANGE) {
            w->dir = -1;
        } else if (w->x < w->home_x - WALKER_RANGE) {
            w->dir = 1;
        }
        NGActorSetHFlip(w->actor, w->dir < 0);
        NGActorSetPos(w->actor, w->x, w->y);
    }
}

static void draw_hud(void) {
    NGTextPrintf(NGFixLayoutOffset(NG_ALIGN_RIGHT, NG_ALIGN_TOP, -1, 1), 0, "DEFEATED %d/%d",
                 state->defeated, MAX_WALKERS);
}

static void defeat_walker(Walker *w) {
    w->alive = 0;
    NGActorSetVisible(w->actor, 0);
    NGActorRemoveFromScene(w->actor);
    state->defeated++;
    NGSfxPlay(NGSFX_BALL_HIT);
    draw_hud();
}

/*
 * Both defeat routes go through the hitbox system, and the difference between
 * them is only which boxes are compared.
 *
 * A shot is the textbook case: the bullet's NG_BOX_HIT against the walker's
 * NG_BOX_HURT, resolved by NGCombatStrike so one bullet cannot kill two
 * walkers on the same frame it already resolved against one.
 *
 * A stomp asks a different question - is the player descending onto the strip
 * across the walker's shoulders (its user1 box)? Because that box is separate
 * from the hurtbox, walking into the walker's side does nothing, which is
 * exactly the Mario rule.
 */
static void resolve_hits(void) {
    for (u8 i = 0; i < MAX_WALKERS; i++) {
        Walker *w = &state->walkers[i];
        if (!w->alive)
            continue;

        for (u8 j = 0; j < MAX_BULLETS; j++) {
            Bullet *b = &state->bullets[j];
            if (!b->alive)
                continue;
            if (NGCombatStrike(b->actor, w->actor, 0) == NG_STRIKE_HIT) {
                defeat_walker(w);
                retire_bullet(b);
                break;
            }
        }
        if (!w->alive)
            continue;

        /* Only while falling: rising into a walker from below should not
         * count, or a jump under a platform would kill it. */
        if (state->player_vel_y <= 0)
            continue;

        NGRect stomp;
        if (!NGActorGetBox(w->actor, NG_BOX_USER1, 0, &stomp))
            continue;

        NGRect feet;
        feet.x = (s16)(FIX_INT(state->player_x) - 6);
        feet.y = (s16)(FIX_INT(state->player_y) + 4);
        feet.w = 12;
        feet.h = 10;

        if (NGRectOverlap(&feet, &stomp, 0)) {
            defeat_walker(w);
            state->player_vel_y = STOMP_BOUNCE;
            state->jumping = 1;
        }
    }
}

void TilemapDemoInit(void) {
    state = NG_ARENA_ALLOC(&ng_arena_state, TilemapDemoState);
    state->switch_target = 0;
    state->menu_open = 0;

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);

    NGPalSetBackdrop(NG_RGB8(96, 168, 232)); /* daytime sky blue */

    NGPalSet(NGPAL_TILES_SIMPLE, NGPal_tiles_simple);

    /* Three overlapping cloud bands: each one both drifts on its own
     * (wind) and tracks the camera at a different parallax rate, so the
     * sky separates into depth planes as soon as anything moves. */
    state->clouds_far = NGBackdropCreate(&NGVisualAsset_cloud_far, NG_BACKDROP_WIDTH_INFINITE, 0,
                                         FIX(0.10), FIX(0.05));
    NGBackdropSetAutoScroll(state->clouds_far, FIX(0.08), 0);
    NGBackdropAddToScene(state->clouds_far, 0, 4, 0);

    state->clouds_mid = NGBackdropCreate(&NGVisualAsset_cloud_mid, NG_BACKDROP_WIDTH_INFINITE, 0,
                                         FIX(0.30), FIX(0.15));
    NGBackdropSetAutoScroll(state->clouds_mid, FIX(0.22), 0);
    NGBackdropAddToScene(state->clouds_mid, 0, 36, 1);

    state->clouds_near = NGBackdropCreate(&NGVisualAsset_cloud_near, NG_BACKDROP_WIDTH_INFINITE, 0,
                                          FIX(0.60), FIX(0.30));
    NGBackdropSetAutoScroll(state->clouds_near, FIX(0.45), 0);
    NGBackdropAddToScene(state->clouds_near, 0, 62, 2);

    // Set the scene's terrain
    NGSceneSetTerrain(&NGTerrainAsset_tilemap_demo_level);
    NGSceneGetTerrainBounds(&state->level_width, &state->level_height);

    /* Open stretch of ground (x 432-607) with nothing overhead, so the
     * player and the walkers share a plane and both defeat routes are
     * reachable within a few seconds of walking. */
    state->player_x = FIX(445);
    state->player_y = FIX(120);
    state->player_vel_x = 0;
    state->player_vel_y = 0;
    state->on_ground = 0;
    state->coyote_timer = 0;
    state->jump_buffer = 0;
    state->jumping = 0;

    // Offset sprite so it's centered on collision AABB
    state->player = NGActorCreate(&NGVisualAsset_ball, 0, 0);
    NGActorAddToScene(state->player, state->player_x - FIX(16), state->player_y - FIX(16), 10);

    NGCameraTrackActor(state->player);
    NGCameraSetDeadzone(80, 40);
    NGCameraSetFollowSpeed(FIX(0.12));
    NGCameraSetBounds(state->level_width, state->level_height);

    state->menu = NGMenuCreateDefault(&ng_arena_state, 10);
    NGMenuSetTitle(state->menu, "TILEMAP DEMO");
    NGMenuAddItem(state->menu, "Resume");
    NGMenuAddItem(state->menu, "Ball Demo");
    NGMenuAddItem(state->menu, "Scroll Demo");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    /* Bullets: anchored centre so a shot is positioned by where it *is*,
     * not by a corner. Parked off-screen until fired. */
    state->facing = 1;
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &state->bullets[i];
        b->actor = NGActorCreate(&NGVisualAsset_bullet, 0, 0);
        NGActorSetAnchor(b->actor, NG_ANCHOR_CENTER);
        NGActorAddToScene(b->actor, FIX(-64), FIX(-64), 11);
        NGActorSetVisible(b->actor, 0);
        b->alive = 0;
    }

    /* Walkers patrol a short beat on the ground. Anchored bottom-centre, so
     * their position is their feet and they sit on the floor line directly. */
    static const s16 walker_x[MAX_WALKERS] = {505, 550, 592};
    state->defeated = 0;
    for (u8 i = 0; i < MAX_WALKERS; i++) {
        Walker *w = &state->walkers[i];
        w->actor = NGActorCreate(&NGVisualAsset_walker, 0, 0);
        NGActorSetAnchor(w->actor, NG_ANCHOR_BOTTOM);
        NGActorSetAnimByName(w->actor, "walk");
        w->home_x = FIX(walker_x[i]);
        w->x = w->home_x;
        w->y = FIX(192); /* ground line */
        w->dir = (i & 1) ? -1 : 1;
        w->alive = 1;
        NGActorAddToScene(w->actor, w->x, w->y, 9);
    }

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "TILEMAP DEMO");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0,
                "B:JUMP  A:SHOOT  STOMP FROM ABOVE");

    draw_hud();

    /* Sequenced FM stage theme - note data in M1 ROM, not streamed samples */
    NGSongPlay(NGSONG_DOWNTOWN);
}

u8 TilemapDemoUpdate(void) {
    if (NGInputPressed(NG_PLAYER_1, NG_BTN_START)) {
        if (state->menu_open) {
            NGMenuHide(state->menu);
            set_menu_open(0);
        } else {
            NGMenuShow(state->menu);
            set_menu_open(1);
        }
    }

    NGMenuUpdate(state->menu);

    if (state->menu_open) {
        if (NGMenuConfirmed(state->menu)) {
            switch (NGMenuGetSelection(state->menu)) {
                case MENU_RESUME:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    break;
                case MENU_BALL_DEMO:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_BALL;
                    break;
                case MENU_SCROLL_DEMO:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_SCROLL;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            set_menu_open(0);
        }
    } else {
        state->player_vel_x = 0;

        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
            state->player_vel_x = -PLAYER_SPEED;
        }
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
            state->player_vel_x = PLAYER_SPEED;
        }

        if (state->player_vel_x < 0)
            state->facing = -1;
        else if (state->player_vel_x > 0)
            state->facing = 1;

        // Jump buffering: remember presses for a few frames
        if (NGInputPressed(NG_PLAYER_1, NG_BTN_B)) {
            state->jump_buffer = JUMP_BUFFER_FRAMES;
        } else if (state->jump_buffer > 0) {
            state->jump_buffer--;
        }

        if (NGInputPressed(NG_PLAYER_1, NG_BTN_A)) {
            fire_bullet();
        }

        // Coyote time: allow jumping briefly after leaving ground
        if (state->on_ground) {
            state->coyote_timer = COYOTE_FRAMES;
        } else if (state->coyote_timer > 0) {
            state->coyote_timer--;
        }

        u8 can_jump = (state->on_ground || state->coyote_timer > 0);
        u8 want_jump = (state->jump_buffer > 0);

        if (can_jump && want_jump) {
            state->player_vel_y = JUMP_FORCE;
            state->jumping = 1;
            state->coyote_timer = 0;
            state->jump_buffer = 0;
        }

        // Variable jump height: release A early to cut jump short
        if (state->jumping && state->player_vel_y < 0 && !NGInputHeld(NG_PLAYER_1, NG_BTN_B)) {
            state->player_vel_y = FIX_MUL(state->player_vel_y, JUMP_CUT_MULT);
            state->jumping = 0;
        }

        if (state->player_vel_y >= 0) {
            state->jumping = 0;
        }

        // Asymmetric gravity: lower while rising, higher while falling
        if (state->player_vel_y < 0) {
            state->player_vel_y += GRAVITY_UP;
        } else {
            state->player_vel_y += GRAVITY_DOWN;
        }

        if (state->player_vel_y > MAX_FALL_SPEED) {
            state->player_vel_y = MAX_FALL_SPEED;
        }

        // Resolve collision against the scene's terrain
        u8 coll =
            NGSceneResolveCollision(&state->player_x, &state->player_y, PLAYER_HALF_W,
                                    PLAYER_HALF_H, &state->player_vel_x, &state->player_vel_y);
        state->on_ground = (coll & NG_COLL_BOTTOM) ? 1 : 0;

        if (state->player_x < PLAYER_HALF_W) {
            state->player_x = PLAYER_HALF_W;
            state->player_vel_x = 0;
        }
        if (state->player_x > FIX(state->level_width) - PLAYER_HALF_W) {
            state->player_x = FIX(state->level_width) - PLAYER_HALF_W;
            state->player_vel_x = 0;
        }

        if (state->player_y > FIX(250)) {
            state->player_x = FIX(80);
            state->player_y = FIX(100);
            state->player_vel_y = 0;
        }

        NGActorSetPos(state->player, state->player_x - FIX(16), state->player_y - FIX(16));

        update_bullets();
        update_walkers();
        resolve_hits();
    }

    return state->switch_target;
}

void TilemapDemoCleanup(void) {
    NGSongStop();

    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 27, 40, 1);

    NGCameraStopTracking();

    NGBackdropRemoveFromScene(state->clouds_near);
    NGBackdropDestroy(state->clouds_near);
    NGBackdropRemoveFromScene(state->clouds_mid);
    NGBackdropDestroy(state->clouds_mid);
    NGBackdropRemoveFromScene(state->clouds_far);
    NGBackdropDestroy(state->clouds_far);

    NGActorRemoveFromScene(state->player);
    NGActorDestroy(state->player);

    // Clear the scene's terrain
    NGSceneClearTerrain();

    NGMenuDestroy(state->menu);

    NGPalSetBackdrop(NG_COLOR_BLACK);

    NGCameraSetPos(0, 0);
    NGCameraSetZoom(NG_CAM_ZOOM_100);
}
