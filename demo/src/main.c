/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// main.c - ProGearSDK combined demo
// Switches between ball demo, scroll demo, and blank scene demo

#include <engine.h>

#include "demo_ids.h"
#include "ball/ball_demo.h"
#include "scroll/scroll_demo.h"
#include "blank_scene/blank_scene.h"

typedef enum {
    DEMO_BALL,
    DEMO_SCROLL,
    DEMO_BLANK_SCENE
} DemoMode;

int main(void) {
    NGEngineInit();

    // Start with ball demo
    DemoMode current_demo = DEMO_BALL;
    BallDemoInit();

    // Main loop
    for (;;) {
        NGEngineFrameStart();

        // Update current demo (returns target demo ID, or 0 for no switch)
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
        }

        // Handle demo switch
        if (switch_to) {
            // Cleanup current demo
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
            }

            // Initialize target demo
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
            }
        }

        NGEngineFrameEnd();
    }
}
