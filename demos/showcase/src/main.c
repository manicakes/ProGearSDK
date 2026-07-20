/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <engine.h>
#include <state.h>
#include <progear_assets.h>

#include "demo_ids.h"
#include "ball/ball_demo.h"
#include "scroll/scroll_demo.h"
#include "blank_scene/blank_scene.h"
#include "tilemap_demo/tilemap_demo.h"
#include "mvs_demo/mvs_demo.h"
#include "raster_demo/raster_demo.h"
#include "brawler/brawler.h"

int main(void) {
    NGEngineInit();

    NGStateRegister(DEMO_ID_BALL, "Ball Demo", BallDemoInit, BallDemoUpdate, BallDemoCleanup);
    NGStateRegister(DEMO_ID_SCROLL, "Scroll Demo", ScrollDemoInit, ScrollDemoUpdate,
                    ScrollDemoCleanup);
    NGStateRegister(DEMO_ID_BLANK_SCENE, "Blank Scene", BlankSceneInit, BlankSceneUpdate,
                    BlankSceneCleanup);
    NGStateRegister(DEMO_ID_TILEMAP, "Tilemap Demo", TilemapDemoInit, TilemapDemoUpdate,
                    TilemapDemoCleanup);
    NGStateRegister(DEMO_ID_MVS, "MVS Features", MVSDemoInit, MVSDemoUpdate, MVSDemoCleanup);
    NGStateRegister(DEMO_ID_RASTER, "Raster Effects", RasterDemoInit, RasterDemoUpdate,
                    RasterDemoCleanup);
    NGStateRegister(DEMO_ID_BRAWLER, "Brawler", BrawlerInit, BrawlerUpdate, BrawlerCleanup);

    NGStateStart(DEMO_ID_BALL);

    for (;;) {
        NGEngineFrameStart();
        NGStateUpdate();
        NGEngineFrameEnd();
    }
}
