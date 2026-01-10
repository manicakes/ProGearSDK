# ProGearSDK

A modern game engine SDK for the NeoGeo AES/MVS.

> **Warning: Alpha Software**
>
> This project is incomplete and under active development. Expect bugs, missing features, and breaking changes without notice. Suitable only for hobbyist experimentation and tinkering.
>
> **No support is provided.** Issues and questions will not be answered. Use at your own risk.

## Core Concepts

ProGearSDK organizes your game around three core abstractions: **Scenes**, **Actors**, and **Cameras**. Understanding these concepts is key to building games with this engine.

### Scene

The **Scene** is the stage where your game takes place. Think of it as an infinite canvas with a coordinate system:

- **X axis**: 0 at the left edge, increases going right (no limit)
- **Y axis**: 0 at the top edge, increases going down (max 512 pixels due to hardware)
- **Z axis**: Render order. Objects with higher Z values are drawn in front of objects with lower Z values.

The scene manages all your game objects and handles rendering them in the correct order. You don't interact with the scene directly after initialization—instead, you populate it with actors.

### Actor

An **Actor** is any visual object in your game: the player, enemies, projectiles, platforms, background elements. Actors are created from visual assets (sprite images defined in `assets.yaml`) and placed into the scene.

Each actor has:
- **Position** (x, y): Where it exists in scene coordinates (fixed-point)
- **Z-index**: Its depth in the render order
- **Animation**: Which frames to display and how fast
- **Appearance**: Palette, visibility, flipping

### Camera

The **Camera** is your viewport into the scene. It determines which portion of the scene is visible on the 320x224 pixel screen. The camera position represents the top-left corner of what's visible. Actors in the scene are automatically transformed based on the camera's position and zoom level.

### How They Work Together

```
┌─────────────────────────────────────────────────────────────┐
│                         SCENE                               │
│   (infinite canvas with X, Y, Z coordinates)                │
│                                                             │
│     ┌─────────────────────┐                                 │
│     │       CAMERA        │  ← Viewport (320x224)           │
│     │   ┌───┐             │                                 │
│     │   │ A │  Actor      │                                 │
│     │   └───┘             │                                 │
│     │         ┌───┐       │                                 │
│     │         │ A │       │                                 │
│     │         └───┘       │                                 │
│     └─────────────────────┘                                 │
│                                                             │
│               ┌───┐                                         │
│               │ A │  ← Actor outside camera (not visible)   │
│               └───┘                                         │
└─────────────────────────────────────────────────────────────┘
```

## A Simple Application

Here's a complete example that displays a sprite and lets you move it with the joystick:

### assets.yaml

```yaml
visual_assets:
  - name: player
    source: assets/player.png
```

### src/main.c

```c
#include <progearsdk.h>             // All SDK headers
#include <ngres_generated_assets.h> // Generated asset definitions

int main(void) {
    // Initialize all engine subsystems
    NGEngineInit();

    // Create an actor from our sprite asset
    NGActorHandle player = NGActorCreate(&NGVisualAsset_player, 0, 0);

    // Add to scene at center of screen, Z-index 0
    NGActorAddToScene(player, FIX(160), FIX(112), 0);
    NGActorSetVisible(player, 1);

    // Main loop
    for (;;) {
        NGEngineFrameStart();

        // Move with joystick
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT))  NGActorMove(player, FIX(-2), 0);
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) NGActorMove(player, FIX(2), 0);
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP))    NGActorMove(player, 0, FIX(-2));
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN))  NGActorMove(player, 0, FIX(2));

        NGEngineFrameEnd();
    }
}
```

### What's happening

1. `NGEngineInit()` initializes all subsystems (scene, camera, input, audio, etc.)
2. `NGActorCreate()` creates an actor from a visual asset defined in `assets.yaml`
3. `NGActorAddToScene()` places the actor in the scene at position (160, 112)—the center of the 320x224 screen
4. Each frame, `NGEngineFrameStart()` handles timing, input polling, and housekeeping
5. We check input and move the actor accordingly
6. `NGEngineFrameEnd()` updates animations and renders everything

## Adding Camera Movement

To make the camera follow the player:

```c
for (;;) {
    NGEngineFrameStart();

    // Move player
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT))  NGActorMove(player, FIX(-2), 0);
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) NGActorMove(player, FIX(2), 0);
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP))    NGActorMove(player, 0, FIX(-2));
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN))  NGActorMove(player, 0, FIX(2));

    // Center camera on player
    NGCameraSetPos(
        NGActorGetX(player) - FIX(160),  // Center horizontally
        NGActorGetY(player) - FIX(112)   // Center vertically
    );

    NGEngineFrameEnd();
}
```

## Adding Animation

Define animations in `assets.yaml`:

```yaml
visual_assets:
  - name: player
    source: assets/player.png
    frame_size: [32, 32]
    animations:
      idle: { frames: [0], speed: 1, loop: true }
      walk: { frames: [0, 1, 2, 3], speed: 4, loop: true }
```

Then switch animations based on movement:

```c
static u8 is_moving = 0;

for (;;) {
    NGEngineFrameStart();

    u8 moving = 0;
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT))  { NGActorMove(player, FIX(-2), 0); moving = 1; }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) { NGActorMove(player, FIX(2), 0); moving = 1; }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP))    { NGActorMove(player, 0, FIX(-2)); moving = 1; }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN))  { NGActorMove(player, 0, FIX(2)); moving = 1; }

    // Change animation when movement state changes
    if (moving != is_moving) {
        is_moving = moving;
        NGActorSetAnimByName(player, moving ? "walk" : "idle");
    }

    NGEngineFrameEnd();
}
```

## Building

### From Project Root

The top-level Makefile orchestrates building the SDK library and demo projects:

```bash
make              # Build SDK library + all demos
make sdk          # Build only the SDK library (sdk/build/libprogearsdk.a)
make showcase     # Build SDK + showcase demo
make template     # Build SDK + template demo
make clean        # Clean all build artifacts
```

### Building a Demo

From within a demo directory:

```bash
cd demos/showcase
make          # Build ROM files (automatically builds SDK first)
make mame     # Build and run in MAME emulator
make clean    # Clean demo build artifacts
```

### Code Quality

```bash
# From project root - check everything
make format       # Auto-format all source files (SDK + demos)
make format-check # Check formatting (fails if changes needed)
make lint         # Run static analysis with cppcheck
make check        # Run all checks (format-check + lint)

# From sdk/ directory - check SDK only
cd sdk
make format       # Format SDK source files
make check        # Run SDK checks only

# From a demo directory - check demo + SDK
cd demos/showcase
make check        # Check demo code only
make check-all    # Check demo + SDK code
```

## Requirements

- **m68k-elf-gcc** - 68000 cross-compiler (GCC 10+)
- **m68k-elf-binutils** - Binary utilities
- **SDCC** - Z80 assembler (sdasz80)
- **Python 3.8+** - Asset pipeline
- **Pillow, PyYAML** - Python libraries
- **clang-format** - Code formatting (optional, for `make format`)
- **cppcheck** - Static analysis (optional, for `make lint`)

### macOS (Homebrew)

```bash
brew install m68k-elf-gcc m68k-elf-binutils sdcc clang-format cppcheck
pip install -r requirements.txt
```

### Linux (Debian/Ubuntu)

```bash
# Install SDCC and code quality tools from package manager
sudo apt install sdcc clang-format cppcheck

# Build m68k toolchain from source or use a PPA
# See: https://wiki.neogeodev.org/index.php?title=Development_tools

pip install -r requirements.txt
```

## Other SDK Features

Beyond scenes, actors, and cameras, ProGearSDK provides:

| Module | Purpose |
|--------|---------|
| `tilemap.h` | Tile-based world rendering with collision |
| `parallax.h` | Scrolling background layers with depth |
| `physics.h` | 2D rigid body simulation and collision |
| `input.h` | Controller input with edge detection |
| `fix.h` | Text rendering on the 40x32 tile overlay |
| `ui.h` | Sprite-based menus with spring animations |
| `palette.h` | Color management and fade effects |
| `audio.h` | Sound effects (ADPCM-A) and music (ADPCM-B) |
| `ngmath.h` | Fixed-point math and trigonometry |
| `arena.h` | Fast zero-fragmentation memory allocation |

## Tilemaps

ProGearSDK supports tile-based world rendering for platformers, RPGs, and other games with large scrolling levels. The tilemap system efficiently renders only visible tiles and provides built-in collision detection.

### Creating a Tilemap

Design levels in [Tiled](https://www.mapeditor.org/) (the industry-standard tilemap editor), then reference them in `assets.yaml`:

```yaml
tilemaps:
  - name: level1
    source: assets/level1.tmx
    layer: "Ground"
    tileset: tiles_simple
```

### Using Tilemaps in Code

```c
#include <tilemap.h>
#include <ngres_generated_assets.h>

// Create tilemap from asset
NGTilemapHandle level = NGTilemapCreate(&NGTilemapAsset_level1);

// Add to scene at world origin
NGTilemapAddToScene(level, FIX(0), FIX(0), 0);

// In game loop - collision detection
fixed player_x, player_y, vel_x, vel_y;

// Resolve collision and get collision flags
u8 hit = NGTilemapResolveAABB(level,
    &player_x, &player_y,    // Position (modified on collision)
    FIX(8), FIX(8),          // Half-size of player hitbox
    &vel_x, &vel_y);         // Velocity (zeroed on collision)

// Check what we hit
if (hit & NG_COLL_BOTTOM) {
    // Landed on ground
    on_ground = 1;
}
```

### Collision Flags

Set tile properties in Tiled using Custom Properties:
- `solid` - Blocks movement (walls, floors)
- `platform` - One-way platform (pass through from below)
- `hazard` - Damages player on contact
- `ladder` - Climbable tile

## Hardware Reference

| Component | Specification |
|-----------|---------------|
| CPU | Motorola 68000 @ 12 MHz |
| Sound CPU | Zilog Z80 @ 4 MHz |
| Resolution | 320x224 pixels |
| Sprites | 381 hardware sprites, 96 per scanline |
| Fix Layer | 40x32 text/UI tiles (always on top) |
| Palettes | 256 palettes, 16 colors each |

## Resources

- [NeoGeo Development Wiki](https://wiki.neogeodev.org)
- [68000 Programmer's Reference](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf)

## License

MIT License
