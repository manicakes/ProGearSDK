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

int main(void) {
    NGEngineInit();

    NGStateRegister(DEMO_ID_BALL, BallDemoInit, BallDemoUpdate, BallDemoCleanup);
    NGStateRegister(DEMO_ID_SCROLL, ScrollDemoInit, ScrollDemoUpdate, ScrollDemoCleanup);
    NGStateRegister(DEMO_ID_BLANK_SCENE, BlankSceneInit, BlankSceneUpdate, BlankSceneCleanup);
    NGStateRegister(DEMO_ID_TILEMAP, TilemapDemoInit, TilemapDemoUpdate, TilemapDemoCleanup);
    NGStateRegister(DEMO_ID_MVS, MVSDemoInit, MVSDemoUpdate, MVSDemoCleanup);
    NGStateRegister(DEMO_ID_RASTER, RasterDemoInit, RasterDemoUpdate, RasterDemoCleanup);

    NGStateStart(DEMO_ID_BALL);

    for (;;) {
        NGEngineFrameStart();
        NGStateUpdate();
        NGEngineFrameEnd();
    }
}
