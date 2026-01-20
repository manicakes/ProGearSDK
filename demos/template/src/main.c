/*
 * ProGearSDK Template
 * A minimal starting point for NeoGeo game development.
 */

#include <progearsdk.h>
#include <progear_assets.h>

int main(void) {
    EngineInit();

    TextPrint(FixLayoutAlign(ALIGN_CENTER, ALIGN_TOP), 0, "PROGEAR SDK TEMPLATE");

    Actor sprite = ActorCreate(&VisualAsset_checkerboard);
    ActorAddToScene(sprite, FIX(0), FIX(0), 0);
    ActorSetVisible(sprite, 1);

    for (;;) {
        EngineFrameStart();

        if (InputHeld(PLAYER_1, BUTTON_LEFT))  ActorMove(sprite, -FIX(2), 0);
        if (InputHeld(PLAYER_1, BUTTON_RIGHT)) ActorMove(sprite, FIX(2), 0);
        if (InputHeld(PLAYER_1, BUTTON_UP))    ActorMove(sprite, 0, -FIX(2));
        if (InputHeld(PLAYER_1, BUTTON_DOWN))  ActorMove(sprite, 0, FIX(2));

        EngineFrameEnd();
    }
}
