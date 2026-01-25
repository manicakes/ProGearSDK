# NeoGeo HAL (Hardware Abstraction Layer)

Low-level hardware access library for NeoGeo development.

The HAL provides direct access to NeoGeo hardware through a clean C API. It handles the details of VRAM layout, sprite control blocks, palette RAM, and hardware registers so you can focus on your game logic.

The HAL depends on the [Core library](../core/) for foundational types, math, and memory utilities. The HAL includes everything needed to build a complete NeoGeo application without the SDK game engine.

## Building

```bash
make              # Build libneogeo.a and crt0.o
make clean        # Remove build artifacts
make format       # Auto-format source files
make lint         # Run static analysis
```

Output:
- `build/libneogeo.a` - HAL library
- `build/crt0.o` - Startup code (link with your application)

## Structure

```
hal/
├── include/      # Public headers (ng_*.h)
├── src/          # HAL implementation
├── startup/      # 68000 startup code (crt0.s)
├── z80/          # Z80 audio driver
├── rom/          # ROM support files
│   ├── link.ld   # Linker script
│   ├── sfix.bin  # System fix layer font
│   └── eyecatcher-*.bin  # Boot logo
└── hal.mk        # Makefile include for apps
```

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

## Building HAL-Only Applications

For applications that don't need the SDK game engine, include `hal.mk` in your Makefile:

```makefile
HAL_PATH = path/to/hal
include $(HAL_PATH)/hal.mk

# Your application
CFLAGS = $(HAL_CFLAGS) -Isrc
LDFLAGS = $(HAL_LDFLAGS)

my_app.elf: main.o $(HAL_LIB) $(HAL_CRT0)
	$(CC) $(CFLAGS) $(LDFLAGS) $(HAL_CRT0) main.o $(HAL_LIB) -lgcc -o $@
```

The `hal.mk` provides:
- `HAL_LIB` - Path to libneogeo.a
- `HAL_CRT0` - Path to crt0.o (startup code)
- `HAL_LINKER_SCRIPT` - Path to link.ld
- `HAL_SFIX` - Path to sfix.bin (S-ROM data)
- `HAL_Z80_DRIVER` - Path to Z80 audio driver source
- `HAL_CFLAGS` - Compiler flags for 68000
- `HAL_LDFLAGS` - Linker flags

## Modules

> **Note:** Foundation modules (`ng_types.h`, `ng_math.h`, `ng_arena.h`) have moved to the [Core library](../core/). They are still accessible via `neogeo_hal.h` which includes Core automatically.

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

## Memory Map

| Address | Size | Description |
|---------|------|-------------|
| 0x000000 | 1MB | P-ROM (program code) |
| 0x100000 | 64KB | Work RAM |
| 0x400000 | 64KB | Palette RAM |
| 0x3C0000 | 64KB | VRAM (via ports) |

## See Also

- [Core Documentation](../core/) - Foundation types, math, and memory
- [SDK Documentation](../sdk/) - High-level game engine
- [NeoGeo Dev Wiki](https://wiki.neogeodev.org) - Hardware details
