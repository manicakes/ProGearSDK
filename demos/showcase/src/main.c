/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <engine.h>
#include <ng_arena.h>
#include <progear_assets.h>

#include "demo_ids.h"
#include "ball/ball_demo.h"
#include "scroll/scroll_demo.h"
#include "blank_scene/blank_scene.h"
#include "tilemap_demo/tilemap_demo.h"
#include "mvs_demo/mvs_demo.h"
#include "raster_demo/raster_demo.h"

typedef enum {
    DEMO_BALL,
    DEMO_SCROLL,
    DEMO_BLANK_SCENE,
    DEMO_TILEMAP,
    DEMO_MVS,
    DEMO_RASTER
} DemoMode;

int main(void) {
    NGEngineInit();

    DemoMode current_demo = DEMO_BALL;
    BallDemoInit();

    for (;;) {
        NGEngineFrameStart();

        u8 switch_to = 0;
        switch (current_demo) {
            case DEMO_BALL:
                switch_to = BallDemoUpdate();
                break;
            case DEMO_SCROLL:
                switch_to = ScrollDemoUpdate();
                break;
            case DEMO_BLANK_SCENE:
                switch_to = BlankSceneUpdate();
                break;
            case DEMO_TILEMAP:
                switch_to = TilemapDemoUpdate();
                break;
            case DEMO_MVS:
                switch_to = MVSDemoUpdate();
                break;
            case DEMO_RASTER:
                switch_to = RasterDemoUpdate();
                break;
        }

        if (switch_to) {
            switch (current_demo) {
                case DEMO_BALL:
                    BallDemoCleanup();
                    break;
                case DEMO_SCROLL:
                    ScrollDemoCleanup();
                    break;
                case DEMO_BLANK_SCENE:
                    BlankSceneCleanup();
                    break;
                case DEMO_TILEMAP:
                    TilemapDemoCleanup();
                    break;
                case DEMO_MVS:
                    MVSDemoCleanup();
                    break;
                case DEMO_RASTER:
                    RasterDemoCleanup();
                    break;
            }

            NGArenaReset(&ng_arena_state);

            switch (switch_to) {
                case DEMO_ID_BALL:
                    current_demo = DEMO_BALL;
                    BallDemoInit();
                    break;
                case DEMO_ID_SCROLL:
                    current_demo = DEMO_SCROLL;
                    ScrollDemoInit();
                    break;
                case DEMO_ID_BLANK_SCENE:
                    current_demo = DEMO_BLANK_SCENE;
                    BlankSceneInit();
                    break;
                case DEMO_ID_TILEMAP:
                    current_demo = DEMO_TILEMAP;
                    TilemapDemoInit();
                    break;
                case DEMO_ID_MVS:
                    current_demo = DEMO_MVS;
                    MVSDemoInit();
                    break;
                case DEMO_ID_RASTER:
                    current_demo = DEMO_RASTER;
                    RasterDemoInit();
                    break;
            }
        }

        NGEngineFrameEnd();
    }
}
