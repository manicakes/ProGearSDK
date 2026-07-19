/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hitbox.h>
#include <ng_math.h>

#include "sdk_internal.h"

/* One bit per actor slot: who this attacker has already connected with during
 * the current attack instance. NG_ACTOR_MAX slots at 8 actors per byte. */
#define HIT_SET_BYTES ((NG_ACTOR_MAX + 7) / 8)

static u8 g_hit_set[NG_ACTOR_MAX][HIT_SET_BYTES];

/* ============================================================
 * Box resolution
 * ============================================================ */

/**
 * Locate the box list for an actor's current frame.
 * Returns 0 if the actor has no box data at all.
 */
static u8 frame_boxes_for(NGActorHandle actor, const NGVisualAsset **out_asset,
                          const NGFrameBoxes **out_fb) {
    const NGVisualAsset *asset = _NGActorGetAsset(actor);
    if (!asset || !asset->boxes || !asset->frame_boxes)
        return 0;

    u16 frame = _NGActorGetAbsoluteFrame(actor);
    if (frame >= asset->frame_count)
        return 0;

    *out_asset = asset;
    *out_fb = &asset->frame_boxes[frame];
    return 1;
}

u8 NGActorHasBox(NGActorHandle actor, u8 kinds) {
    const NGVisualAsset *asset;
    const NGFrameBoxes *fb;
    if (!frame_boxes_for(actor, &asset, &fb))
        return 0;
    return (fb->kinds & kinds) ? 1 : 0;
}

u8 NGActorBoxCount(NGActorHandle actor, u8 kinds) {
    const NGVisualAsset *asset;
    const NGFrameBoxes *fb;
    if (!frame_boxes_for(actor, &asset, &fb))
        return 0;
    if (!(fb->kinds & kinds))
        return 0;

    u8 n = 0;
    for (u8 i = 0; i < fb->box_count; i++) {
        if (asset->boxes[fb->first_box + i].kind & kinds)
            n++;
    }
    return n;
}

/**
 * Resolve box @p b of @p asset into scene coordinates for @p actor.
 *
 * Boxes are stored in frame-local pixels from the frame's top-left, so the
 * transform is: mirror within the frame if flipped, then offset by the actor's
 * top-left corner (its position less its anchor).
 */
static void resolve_box(NGActorHandle actor, const NGVisualAsset *asset, const NGBox *b,
                        NGRect *out) {
    s16 local_x = b->x;
    if (_NGActorGetHFlip(actor)) {
        /* Mirror about the frame's vertical centre. Exact, and independent of
         * where the anchor happens to be. */
        local_x = (s16)((s16)asset->width_pixels - b->x - (s16)b->w);
    }

    NGVec2 anchor = NGActorGetAnchorPixels(actor);
    s16 origin_x = (s16)(FIX_INT(NGActorGetX(actor)) - anchor.x);
    s16 origin_y = (s16)(FIX_INT(NGActorGetY(actor)) - anchor.y);

    out->x = (s16)(origin_x + local_x);
    out->y = (s16)(origin_y + b->y);
    out->w = b->w;
    out->h = b->h;
}

u8 NGActorGetBox(NGActorHandle actor, u8 kinds, u8 index, NGRect *out) {
    const NGVisualAsset *asset;
    const NGFrameBoxes *fb;
    if (!out || !frame_boxes_for(actor, &asset, &fb))
        return 0;
    if (!(fb->kinds & kinds))
        return 0;

    u8 seen = 0;
    for (u8 i = 0; i < fb->box_count; i++) {
        u8 slot = (u8)(fb->first_box + i);
        if (!(asset->boxes[slot].kind & kinds))
            continue;
        if (seen == index) {
            resolve_box(actor, asset, &asset->boxes[slot], out);
            return 1;
        }
        seen++;
    }
    return 0;
}

u16 NGActorGetAnimFrame(NGActorHandle actor) {
    return _NGActorGetAnimFrame(actor);
}

/* ============================================================
 * Overlap
 * ============================================================ */

u8 NGRectOverlap(const NGRect *a, const NGRect *b, NGRect *out) {
    s16 ax2 = (s16)(a->x + (s16)a->w);
    s16 ay2 = (s16)(a->y + (s16)a->h);
    s16 bx2 = (s16)(b->x + (s16)b->w);
    s16 by2 = (s16)(b->y + (s16)b->h);

    if (a->x >= bx2 || b->x >= ax2 || a->y >= by2 || b->y >= ay2)
        return 0;

    if (out) {
        s16 x1 = a->x > b->x ? a->x : b->x;
        s16 y1 = a->y > b->y ? a->y : b->y;
        s16 x2 = ax2 < bx2 ? ax2 : bx2;
        s16 y2 = ay2 < by2 ? ay2 : by2;
        out->x = x1;
        out->y = y1;
        out->w = (u16)(x2 - x1);
        out->h = (u16)(y2 - y1);
    }
    return 1;
}

u8 NGActorOverlap(NGActorHandle a, u8 kinds_a, NGActorHandle b, u8 kinds_b, NGRect *out) {
    if (!NGActorHasBox(a, kinds_a) || !NGActorHasBox(b, kinds_b))
        return 0;

    NGRect ra, rb;
    for (u8 i = 0; NGActorGetBox(a, kinds_a, i, &ra); i++) {
        for (u8 j = 0; NGActorGetBox(b, kinds_b, j, &rb); j++) {
            if (NGRectOverlap(&ra, &rb, out))
                return 1;
        }
    }
    return 0;
}

/* ============================================================
 * Strikes
 * ============================================================ */

static u8 hit_set_test(NGActorHandle attacker, NGActorHandle defender) {
    return (g_hit_set[attacker][defender >> 3] >> (defender & 7)) & 1;
}

static void hit_set_mark(NGActorHandle attacker, NGActorHandle defender) {
    g_hit_set[attacker][defender >> 3] |= (u8)(1 << (defender & 7));
}

void NGCombatBeginAttack(NGActorHandle attacker) {
    if (attacker < 0 || attacker >= NG_ACTOR_MAX)
        return;
    for (u8 i = 0; i < HIT_SET_BYTES; i++) {
        g_hit_set[attacker][i] = 0;
    }
}

void NGCombatClearPair(NGActorHandle attacker, NGActorHandle defender) {
    if (attacker < 0 || attacker >= NG_ACTOR_MAX)
        return;
    if (defender < 0 || defender >= NG_ACTOR_MAX)
        return;
    g_hit_set[attacker][defender >> 3] &= (u8) ~(1 << (defender & 7));
}

/**
 * Test the attacker's hitboxes against one kind on the defender.
 * Returns 1 and fills @p hit on the first overlapping pair.
 */
static u8 strike_against(NGActorHandle attacker, NGActorHandle defender, u8 kind, NGHit *hit) {
    if (!NGActorHasBox(defender, kind))
        return 0;

    NGRect ra, rb, ov;
    for (u8 i = 0; NGActorGetBox(attacker, NG_BOX_HIT, i, &ra); i++) {
        for (u8 j = 0; NGActorGetBox(defender, kind, j, &rb); j++) {
            if (!NGRectOverlap(&ra, &rb, &ov))
                continue;
            if (hit) {
                hit->overlap = ov;
                hit->contact_x = (s16)(ov.x + (s16)(ov.w >> 1));
                hit->contact_y = (s16)(ov.y + (s16)(ov.h >> 1));
                hit->defender_kind = kind;
            }
            return 1;
        }
    }
    return 0;
}

NGStrikeResult NGCombatStrike(NGActorHandle attacker, NGActorHandle defender, NGHit *out) {
    if (attacker < 0 || attacker >= NG_ACTOR_MAX)
        return NG_STRIKE_NONE;
    if (defender < 0 || defender >= NG_ACTOR_MAX || attacker == defender)
        return NG_STRIKE_NONE;

    /* One resolution per attack instance per target */
    if (hit_set_test(attacker, defender))
        return NG_STRIKE_NONE;

    if (!NGActorHasBox(attacker, NG_BOX_HIT))
        return NG_STRIKE_NONE;

    /* Guard before hurt. When a frame carries both, the guard must win - this
     * ordering is what makes a parry land on the frames it is meant to. */
    if (strike_against(attacker, defender, NG_BOX_GUARD, out)) {
        hit_set_mark(attacker, defender);
        return NG_STRIKE_GUARD;
    }

    if (strike_against(attacker, defender, NG_BOX_HURT, out)) {
        hit_set_mark(attacker, defender);
        return NG_STRIKE_HIT;
    }

    return NG_STRIKE_NONE;
}
