/**
 * HAL-Only Template
 *
 * Minimal NeoGeo application using only the Hardware Abstraction Layer.
 * Demonstrates that complete applications can be built without the SDK.
 */

#include <neogeo_hal.h>

int main(void) {
    /* Clear the fix layer */
    NGFixClearAll();

    /* Set backdrop color to dark blue */
    NGPalSetBackdrop(NG_COLOR_DARK_BLUE);

    /* Set up fix palette 0: glyph color 1 on opaque background color 2. */
    NGPalSetColor(NG_PAL_FIX, 1, NG_COLOR_WHITE);
    NGPalSetColor(NG_PAL_FIX, 2, NG_COLOR_DARK_BLUE);

    /* Print text on the fix layer (40x32 tile text overlay) */
    /* NGFixLayoutXY takes x,y tile coordinates */
    NGTextPrint(NGFixLayoutXY(15, 14), 0, "Hello HAL!");

    /* Main loop - just wait for vblank forever */
    for (;;) {
        NGWaitVBlank();
    }
}
