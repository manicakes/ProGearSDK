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

    /* Print text on the fix layer (40x32 tile text overlay) */
    /* NGFixLayoutXY takes x,y tile coordinates */
    /* Palette 0 is the default fix layer palette from sfix.bin */
    NGTextPrint(NGFixLayoutXY(15, 14), 0, "Hello HAL!");

    /* Main loop - just wait for vblank forever */
    for (;;) {
        NGWaitVBlank();
    }
}
