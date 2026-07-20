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
#include <debug.h>
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
/* Patrol half-width. The walkers sit on the x 192-367 ground run and are
 * 32px wide, so 20 keeps even the outermost one (330) fully on it:
 * 330 + 20 + 16 = 366. They have no terrain collision, so this is what
 * stops them walking out over the edges. */
#define WALKER_RANGE FIX(20)
/* Feel: a stomp launches you higher than a normal jump, and higher still if
 * you are holding the jump button as you land - the Mario rule. */
#define STOMP_BOUNCE      FIX(-6.0)
#define STOMP_BOUNCE_HELD FIX(-8.5)

#define WALKER_HP        3 /* bullets needed to finish one off */
#define HIT_KNOCKBACK    FIX(7)
#define HIT_FLASH_FRAMES 5  /* white frames on impact */
#define DEATH_FRAMES     30 /* blink-out on defeat */

/* Body contact shoves the player away. The timer overrides walk input for a
 * few frames so the shove reads as a shove rather than being cancelled by the
 * stick on the very next frame. */
#define PUSHBACK_SPEED  FIX(3.0)
#define PUSHBACK_LIFT   FIX(-1.75)
#define PUSHBACK_FRAMES 10

/* Spawn: column 1 (x 16-31) is one of only four columns clear from the top of
 * the map to the ground, and it keeps the camera clamped at x=0. */
#define SPAWN_X 24
#define SPAWN_Y 120

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
    u8 alive; /* slot in use (includes the death animation) */
    u8 hp;
    u8 flash; /* frames left of the white impact flash */
    u8 dying; /* death animation running; no longer collidable */
    u8 die_timer;
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
    u8 pushback; /* frames of shove remaining */
    s8 pushback_dir;
    Bullet bullets[MAX_BULLETS];
    Walker walkers[MAX_WALKERS];
    u8 defeated;
} TilemapDemoState;

static TilemapDemoState *state;

#define MENU_RESUME      0
#define MENU_BALL_DEMO   1
#define MENU_SCROLL_DEMO 2
#define MENU_BRAWLER     3

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

/* Impact flash: swap to a white palette for a few frames, then back. Cheap
 * feedback that needs no extra animation frames. */
static void walker_flash(Walker *w) {
    w->flash = HIT_FLASH_FRAMES;
    NGActorSetPalette(w->actor, NGPAL_BALL_WHITE);
}

/* Death: blink out over half a second, accelerating, then leave the scene.
 * The walker stops moving and stops colliding the moment it starts dying. */
static void walker_update_death(Walker *w) {
    w->die_timer--;
    u8 period = (w->die_timer > DEATH_FRAMES / 2) ? 4 : 2;
    NGActorSetVisible(w->actor, (u8)((w->die_timer / period) & 1));
    if (w->die_timer == 0) {
        NGActorRemoveFromScene(w->actor);
        w->alive = 0;
    }
}

static void update_walkers(void) {
    for (u8 i = 0; i < MAX_WALKERS; i++) {
        Walker *w = &state->walkers[i];
        if (!w->alive)
            continue;

        if (w->dying) {
            walker_update_death(w);
            continue;
        }

        if (w->flash && --w->flash == 0) {
            NGActorSetPalette(w->actor, NGPAL_WALKER);
        }

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
    w->dying = 1;
    w->die_timer = DEATH_FRAMES;
    NGActorSetPalette(w->actor, NGPAL_BALL_WHITE);
    state->defeated++;
    NGSfxPlay(NGSFX_BALL_HIT);
    draw_hud();
}

/* One bullet's worth of damage: knock the walker back, flash it, and finish
 * it on the third hit. The knockback is clamped to the patrol span so a hit
 * cannot shove it off the ledge it lives on. */
static void walker_take_hit(Walker *w, s8 dir) {
    w->x += dir > 0 ? HIT_KNOCKBACK : -HIT_KNOCKBACK;
    if (w->x > w->home_x + WALKER_RANGE)
        w->x = w->home_x + WALKER_RANGE;
    else if (w->x < w->home_x - WALKER_RANGE)
        w->x = w->home_x - WALKER_RANGE;
    NGActorSetPos(w->actor, w->x, w->y);

    if (--w->hp == 0) {
        defeat_walker(w);
        return;
    }
    walker_flash(w);
    NGSfxPlay(NGSFX_BALL_HIT);
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
                walker_take_hit(w, b->dir);
                retire_bullet(b);
                break;
            }
        }
        if (w->dying)
            continue;

        /* Stomp, only while falling: rising into a walker from below must not
         * count, or a jump under a platform would kill it. A stomp always
         * finishes a walker regardless of remaining hit points. */
        NGRect stomp;
        if (state->player_vel_y > 0 && NGActorGetBox(w->actor, NG_BOX_USER1, 0, &stomp)) {
            NGRect feet;
            feet.x = (s16)(FIX_INT(state->player_x) - 6);
            feet.y = (s16)(FIX_INT(state->player_y) + 4);
            feet.w = 12;
            feet.h = 10;

            if (NGRectOverlap(&feet, &stomp, 0)) {
                defeat_walker(w);
                state->player_vel_y =
                    NGInputHeld(NG_PLAYER_1, NG_BTN_B) ? STOMP_BOUNCE_HELD : STOMP_BOUNCE;
                state->jumping = 1;
                continue;
            }
        }

        /* Body contact: shove the player away from the walker's centre.
         * Uses the walker's NG_BOX_BODY rather than its hurtbox, so brushing
         * the sprite's edges does not shove you. */
        NGRect body;
        if (!NGActorGetBox(w->actor, NG_BOX_BODY, 0, &body))
            continue;

        NGRect player;
        player.x = (s16)(FIX_INT(state->player_x) - 6);
        player.y = (s16)(FIX_INT(state->player_y) - 12);
        player.w = 12;
        player.h = 24;

        if (NGRectOverlap(&player, &body, 0)) {
            s16 body_centre = (s16)(body.x + (s16)(body.w >> 1));
            state->pushback_dir = (FIX_INT(state->player_x) < body_centre) ? -1 : 1;
            state->pushback = PUSHBACK_FRAMES;
            if (state->on_ground) {
                state->player_vel_y = PUSHBACK_LIFT;
            }
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

    /* Column 29 (x 464-479) is clear from the top of the map down to the
     * ground at row 12, so the player lands on the same plane the walkers
     * patrol on. Neighbouring columns carry ledges at rows 5 and 7; spawning
     * inside one leaves the player embedded in solid terrain. */
    state->player_x = FIX(SPAWN_X);
    state->player_y = FIX(SPAWN_Y);
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
    NGMenuAddItem(state->menu, "Brawler");
    NGMenuSetDefaultSounds(state->menu);
    NGEngineSetActiveMenu(state->menu);

    /* Bullets: anchored centre so a shot is positioned by where it *is*,
     * not by a corner. Parked off-screen until fired. */
    state->facing = 1;
    state->pushback = 0;
    state->pushback_dir = 1;
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
    static const s16 walker_x[MAX_WALKERS] = {230, 280, 330};
    state->defeated = 0;
    for (u8 i = 0; i < MAX_WALKERS; i++) {
        Walker *w = &state->walkers[i];
        w->actor = NGActorCreate(&NGVisualAsset_walker, 0, 0);
        NGActorSetAnchor(w->actor, NG_ANCHOR_BOTTOM);
        NGActorSetAnimByName(w->actor, "walk");
        w->home_x = FIX(walker_x[i]);
        w->x = w->home_x;
        w->y = FIX(192); /* ground line, row 12 */
        w->dir = (i & 1) ? -1 : 1;
        w->alive = 1;
        w->hp = WALKER_HP;
        w->flash = 0;
        w->dying = 0;
        w->die_timer = 0;
        NGActorAddToScene(w->actor, w->x, w->y, 9);
    }

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "TILEMAP DEMO");
    NGTextPrint(NGFixLayoutOffset(NG_ALIGN_LEFT, NG_ALIGN_BOTTOM, 1, -1), 0,
                "B:JUMP  A:SHOOT  STOMP FROM ABOVE");

    draw_hud();

    NGDebugSetEnabled(1);
    NGDebugResetPeaks();

    /* Distinct fix palettes so hitboxes and hurtboxes read apart at a glance */
    NGDebugSetBoxPalette(NG_BOX_HIT, NGPAL_BALL_RED);
    NGDebugSetBoxPalette(NG_BOX_HURT, NGPAL_BALL_GREEN);
    NGDebugSetBoxPalette(NG_BOX_USER1, NGPAL_BALL_YELLOW);

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
                case MENU_BRAWLER:
                    NGMenuHide(state->menu);
                    set_menu_open(0);
                    state->switch_target = DEMO_ID_BRAWLER;
                    break;
            }
        }

        if (NGMenuCancelled(state->menu)) {
            NGMenuHide(state->menu);
            set_menu_open(0);
        }
    } else {
        state->player_vel_x = 0;

        if (state->pushback > 0) {
            /* Being shoved: ignore the stick so the knock reads clearly */
            state->pushback--;
            state->player_vel_x = state->pushback_dir > 0 ? PUSHBACK_SPEED : -PUSHBACK_SPEED;
        } else {
            if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
                state->player_vel_x = -PLAYER_SPEED;
            }
            if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
                state->player_vel_x = PLAYER_SPEED;
            }
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
            /* Reuse the verified-clear spawn column; the old fallback of
             * x=80 sits under the row-5 ledge. */
            state->player_x = FIX(SPAWN_X);
            state->player_y = FIX(SPAWN_Y);
            state->player_vel_y = 0;
        }

        NGActorSetPos(state->player, state->player_x - FIX(16), state->player_y - FIX(16));

        /* Box overlay: clear last frame's cells, then redraw */
        NGDebugClearBoxes();
        for (u8 i = 0; i < MAX_WALKERS; i++) {
            if (state->walkers[i].alive && !state->walkers[i].dying) {
                NGDebugDrawBoxes(state->walkers[i].actor, NG_BOX_HURT | NG_BOX_USER1);
            }
        }
        for (u8 i = 0; i < MAX_BULLETS; i++) {
            if (state->bullets[i].alive) {
                NGDebugDrawBoxes(state->bullets[i].actor, NG_BOX_HIT);
            }
        }

        NGDebugDrawHUD(26);

        update_bullets();
        update_walkers();
        resolve_hits();
    }

    return state->switch_target;
}

void TilemapDemoCleanup(void) {
    NGSongStop();

    /* Anything this demo put on the fix layer has to go, or it bleeds through
     * into whatever comes next - the layer is not part of the scene and is not
     * cleared by resetting it. */
    NGDebugClearBoxes();
    NGFixClear(0, 3, 40, 1);
    NGFixClear(0, 4, 40, 1);
    NGFixClear(0, 26, 40, 1);
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
