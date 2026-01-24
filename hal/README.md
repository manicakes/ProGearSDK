# NeoGeo HAL (Hardware Abstraction Layer)

Low-level hardware access library for NeoGeo development.

The HAL provides direct access to NeoGeo hardware through a clean C API. It handles the details of VRAM layout, sprite control blocks, palette RAM, and hardware registers so you can focus on your game logic.

## Building

```bash
make              # Build libneogeo.a
make clean        # Remove build artifacts
make format       # Auto-format source files
make lint         # Run static analysis
```

Output: `build/libneogeo.a`

## Usage

Include the master header to access all HAL functionality:

```c
#include <neogeo_hal.h>
```

Or include individual modules as needed:

```c
#include <ng_sprite.h>
#include <ng_palette.h>
```

## Modules

### ng_types.h - Base Types

Foundation types used throughout the HAL and SDK.

```c
u8, u16, u32     // Unsigned integers
s8, s16, s32     // Signed integers
vu8, vu16, vu32  // Volatile unsigned (for hardware registers)
```

### ng_hardware.h - Hardware Access

Direct access to NeoGeo hardware registers and memory.

```c
// Hardware registers
REG_VRAMADDR     // VRAM address register
REG_VRAMRW       // VRAM read/write
REG_SWPROM       // Switch to ROM bank

// VRAM access macros
VRAM_WRITE(addr, val)
VRAM_READ(addr)

// Wait for vertical blank
NGWaitVBlank()
```

### ng_math.h - Fixed-Point Math

The 68000 has no FPU, so all math uses fixed-point representation.

```c
// Types
fixed            // 16.16 fixed-point (32-bit)
fixed16          // 8.8 fixed-point (16-bit)
angle_t          // 0-255 = 0-360 degrees

// Conversion
FIX(n)           // Integer to fixed (compile-time)
FIX_INT(f)       // Fixed to integer (truncate)
FIX_FROM_FLOAT(f) // Float to fixed (compile-time only)

// Arithmetic
FIX_MUL(a, b)    // Multiply two fixed values
FIX_DIV(a, b)    // Divide two fixed values

// Trigonometry (angle_t input, fixed output)
NGSin(angle)
NGCos(angle)

// Vectors
NGVec2           // 2D vector (fixed x, y)
NGVec2Add(a, b)
NGVec2Sub(a, b)
NGVec2Scale(v, s)
NGVec2Dot(a, b)
NGVec2Length(v)
NGVec2Normalize(v)
```

### ng_color.h - Color Manipulation

NeoGeo uses 16-bit colors in a specific format (Dark bit + 5-bit RGB).

```c
// Create colors
NGColor color = NG_RGB(31, 16, 0);  // Orange (5-bit components)

// Predefined colors
NG_COLOR_BLACK, NG_COLOR_WHITE
NG_COLOR_RED, NG_COLOR_GREEN, NG_COLOR_BLUE
NG_COLOR_DARK_BLUE, NG_COLOR_DARK_RED, ...

// Color manipulation
NGColorLerp(a, b, t)       // Interpolate between colors
NGColorBrighten(c, amount)
NGColorDarken(c, amount)
```

### ng_palette.h - Palette Management

The NeoGeo has 256 palettes of 16 colors each in Palette RAM.

```c
// Set individual color
NGPalSetColor(palette, index, color)

// Load entire palette (16 colors)
NGPalSet(palette, colors)

// Set backdrop color (color 0 of palette 0)
NGPalSetBackdrop(color)

// Palette effects
NGPalFadeToBlack(palette, amount)  // 0=normal, 16=black
NGPalFadeToWhite(palette, amount)
```

### ng_sprite.h - Sprite Hardware

Direct control over NeoGeo's sprite hardware via Sprite Control Blocks (SCB).

```c
// Sprite attributes
NGSprSetTile(sprite, tile)      // Set tile number
NGSprSetPalette(sprite, pal)    // Set palette
NGSprSetPos(sprite, x, y)       // Set screen position
NGSprSetSize(sprite, h, sticky) // Set height and chain mode
NGSprSetZoom(sprite, x, y)      // Set zoom (0-16)
NGSprSetFlip(sprite, h, v)      // Set flip flags

// Batch operations
NGSprHide(sprite)               // Make sprite invisible
NGSprShow(sprite)               // Make sprite visible
```

### ng_fix.h - Fix Layer (Text)

The fix layer is a 40x32 tile layer that renders above all sprites, perfect for UI and text.

```c
// Print text
NGFixPrint(x, y, palette, "Hello World")
NGFixPrintf(x, y, palette, "Score: %d", score)

// Clear regions
NGFixClear(x, y, width, height)
NGFixClearAll()

// Individual tiles
NGFixSetTile(x, y, tile, palette)
```

### ng_input.h - Controller Input

Read controller state with edge detection for button presses.

```c
// Update input state (call once per frame)
NGInputUpdate()

// Check button state
NGInputHeld(NG_PLAYER_1, NG_BTN_A)     // Button currently down
NGInputPressed(NG_PLAYER_1, NG_BTN_A)  // Button just pressed
NGInputReleased(NG_PLAYER_1, NG_BTN_A) // Button just released

// Button constants
NG_BTN_UP, NG_BTN_DOWN, NG_BTN_LEFT, NG_BTN_RIGHT
NG_BTN_A, NG_BTN_B, NG_BTN_C, NG_BTN_D
NG_BTN_START, NG_BTN_SELECT

// Player constants
NG_PLAYER_1, NG_PLAYER_2
```

### ng_audio.h - Sound System

ADPCM audio playback for sound effects and music.

```c
// Sound effects (ADPCM-A, 6 channels)
NGSfxPlay(sfx_index)
NGSfxPlayPan(sfx_index, NG_PAN_LEFT)  // With stereo pan
NGSfxStopChannel(channel)
NGSfxStopAll()

// Music (ADPCM-B, 1 channel)
NGMusicPlay(music_index)
NGMusicStop()
NGMusicPause()
NGMusicResume()
NGMusicIsPaused()
NGMusicIsPlaying()

// Volume control
NGSfxSetVolume(volume)    // 0-31
NGMusicSetVolume(volume)  // 0-63

// Pan constants
NG_PAN_LEFT, NG_PAN_CENTER, NG_PAN_RIGHT
```

### ng_arena.h - Memory Allocation

Fast bump-pointer arena allocator with zero fragmentation.

```c
// Pre-defined arenas
ng_arena_persistent  // Lives for entire game
ng_arena_state       // Reset on scene change
ng_arena_frame       // Reset every frame

// Allocation
void *ptr = NGArenaAlloc(&ng_arena_state, size)
MyStruct *s = NG_ARENA_ALLOC(&ng_arena_state, MyStruct)
MyStruct *arr = NG_ARENA_ALLOC_ARRAY(&ng_arena_state, MyStruct, 10)

// Reset (free all allocations)
NGArenaReset(&ng_arena_state)

// Save/restore points
NGArenaMark mark = NGArenaSave(&ng_arena_state)
// ... allocate temporary data ...
NGArenaRestore(&ng_arena_state, mark)

// Query
NGArenaUsed(&ng_arena_state)
NGArenaRemaining(&ng_arena_state)
```

## Memory Map

| Address | Size | Description |
|---------|------|-------------|
| 0x000000 | 1MB | P-ROM (program code) |
| 0x100000 | 64KB | Work RAM |
| 0x400000 | 64KB | Palette RAM |
| 0x3C0000 | 64KB | VRAM (via ports) |

## See Also

- [SDK Documentation](../sdk/) - High-level game engine
- [NeoGeo Dev Wiki](https://wiki.neogeodev.org) - Hardware details
