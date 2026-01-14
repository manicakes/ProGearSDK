# ProGearSDK Showcase

A comprehensive demonstration application showcasing ProGearSDK features for NeoGeo development.

## Building

```bash
make          # Build ROM files (automatically builds SDK library first)
make mame     # Build and run in MAME
make clean    # Clean build artifacts
```

### Code Quality

```bash
make check        # Check demo code formatting and lint
make check-all    # Check demo + SDK code
```

## Running in MAME

Before running with `make mame`, you must configure the path to your NeoGeo BIOS:

1. Open `Makefile`
2. Find the `NEOGEO_BIOS_PATH` variable near the top
3. Set it to the directory containing your `neogeo.zip` BIOS file

```makefile
# Example: BIOS located in ~/roms/
NEOGEO_BIOS_PATH = ~/roms
```

The `neogeo.zip` BIOS is required by MAME to run NeoGeo games and is not included with ProGearSDK.

## Controls

- **D-Pad**: Scene-specific controls
- **Start**: Open/close menu
- **A**: Confirm menu selection
- **B**: Cancel/close menu

## Demo Scenes

The demo includes four scenes that can be switched between using the in-game menu.

### Ball Demo

The default scene demonstrating multiple SDK features working together.

**Features demonstrated:**
- `actor.h` - Animated ball sprites with physics
- `backdrop.h` - Layered background with shadow effect
- `physics.h` - Ball collision and bouncing
- `camera.h` - Circular camera motion and zoom effects
- `audio.h` - Background music (ADPCM-B) and sound effects (ADPCM-A)
- `ui.h` - Spring-animated menu system
- `palette.h` - Multiple ball color palettes
- `lighting.h` - Day/night cycle with lightning flashes

**Day/Night Cycle:**
Every 10 seconds the scene transitions between day and night mode. During night mode:
- The scene takes on a blue tint with reduced brightness
- Random lightning flashes occur, with varying intensity patterns
- Lightning effects demonstrate the lighting system's layer stacking (flash on top of night mode)

**Menu options:**
- Add Ball - Spawn a new bouncing ball
- Clear Balls - Remove all balls
- Toggle Zoom - Switch between 100% and 75% zoom
- Pause/Resume Music - Control background music

### Scroll Demo

Demonstrates the backdrop scrolling system with multiple layers at different depths.

**Features demonstrated:**
- `backdrop.h` - Three-layer parallax scrolling (back, middle, front)
- `camera.h` - Auto-scrolling camera with vertical bobbing
- `scene.h` - Z-ordering of backdrop layers

**Menu options:**
- Toggle Zoom - Switch between zoom levels
- Reset Camera - Return camera to starting position

### Blank Scene

A minimal scene template showing the basic structure for a ProGearSDK application.

**Features demonstrated:**
- `engine.h` - Basic game loop with `NGEngineInit/FrameStart/FrameEnd`
- `ui.h` - Menu creation and handling
- `fix.h` - Text display on the fix layer

This scene serves as a starting point for new development.

### Terrain Demo

A platformer-style demo showcasing the terrain system with collision detection.

**Features demonstrated:**
- `terrain.h` - Tile-based terrain rendering with viewport windowing
- `terrain.h` - AABB collision resolution with solid tiles
- `camera.h` - Camera following the player with bounds clamping
- `input.h` - Platformer controls (move, jump)

**Controls:**
- **Left/Right** - Move player
- **A** - Jump (hold for higher jump)

**Gameplay features:**
- Variable jump height (tap for short hop, hold for full jump)
- Smooth collision resolution against terrain
- Camera tracks player position
- World bounds from terrain dimensions

## Project Structure

```
showcase/
├── src/
│   ├── main.c                 # Entry point and scene switching
│   ├── ball/
│   │   ├── ball.c/h           # Ball physics system
│   │   └── ball_demo.c/h      # Ball demo scene
│   ├── scroll/
│   │   └── scroll_demo.c/h    # Backdrop scroll demo scene
│   ├── blank_scene/
│   │   └── blank_scene.c/h    # Minimal blank scene
│   └── tilemap_demo/
│       └── tilemap_demo.c/h   # Terrain collision demo scene
├── assets/                    # Graphics and audio assets
├── assets.yaml                # Asset definitions for progear_assets.py
└── Makefile
```

## Credits

### Music

"Destruction Bringer (Free)" - Composed by One Man Symphony - https://onemansymphony.bandcamp.com

Licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

## License

This demo is part of ProGearSDK and is released under the MIT License.
