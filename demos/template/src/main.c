/*
 * ProGearSDK Template
 * A minimal starting point for NeoGeo game development.
 */

#include <engine.h>
#include <scene.h>
#include <actor.h>
#include <input.h>
#include <fix.h>
#include <palette.h>
#include <ngres_generated_assets.h>

int main(void) {
    NGEngineInit();

    NGTextPrint(NGFixLayoutAlign(NG_ALIGN_CENTER, NG_ALIGN_TOP), 0, "PROGEAR SDK TEMPLATE");

    NGActorHandle sprite = NGActorCreate(&NGVisualAsset_checkerboard, 0, 0);
    NGActorAddToScene(sprite, FIX(0), FIX(0), 0);
    NGActorSetVisible(sprite, 1);

    for (;;) {
        NGEngineFrameStart();

        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT))  NGActorMove(sprite, -FIX(2), 0);
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) NGActorMove(sprite, FIX(2), 0);
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP))    NGActorMove(sprite, 0, -FIX(2));
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN))  NGActorMove(sprite, 0, FIX(2));

        NGEngineFrameEnd();
    }
}
