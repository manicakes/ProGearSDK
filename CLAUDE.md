# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ProGearSDK is a game engine SDK for the NeoGeo AES/MVS console (Motorola 68000 @ 12MHz, no FPU). It provides abstractions for building games using Scenes, Actors, and Cameras.

## Build Commands

```bash
# Build everything (Core + HAL + SDK library + all demos)
make

# Build only the Core library (outputs core/build/libneogeo_core.a)
make core

# Build Core and HAL library (outputs hal/build/libneogeo.a)
make hal

# Build Core, HAL, and SDK library (outputs sdk/build/libprogearsdk.a)
make sdk

# Build and run a demo in MAME
cd demos/showcase && make mame

# Code quality
make format       # Auto-format all source files
make format-check # Check formatting (CI)
make lint         # Static analysis with cppcheck
make check        # Run all checks
```

## Architecture

### Three-Layer Design

The codebase is split into three libraries:

1. **Core (Foundation Library)** - `core/` directory
   - Foundational types, math, and memory utilities
   - No hardware dependencies
   - Output: `core/build/libneogeo_core.a`
   - Headers use `ng_` prefix (e.g., `ng_types.h`, `ng_math.h`)

2. **HAL (Hardware Abstraction Layer)** - `hal/` directory
   - Low-level NeoGeo hardware access
   - Depends on Core
   - Output: `hal/build/libneogeo.a`
   - Headers use `ng_` prefix (e.g., `ng_sprite.h`, `ng_palette.h`)

3. **SDK (Game Engine)** - `sdk/` directory
   - High-level game abstractions (scenes, actors, cameras)
   - Depends on Core and HAL
   - Output: `sdk/build/libprogearsdk.a`

### Core Modules (`core/include/`)

- `ng_types.h` - Base types (u8, u16, u32, s8, s16, s32)
- `ng_math.h` - Fixed-point math, trig tables, vectors
- `ng_arena.h` - Bump-pointer arena allocator
- `neogeo_core.h` - Master header (includes all Core modules)

### HAL Modules (`hal/include/`)

- `ng_hardware.h` - Hardware registers, VRAM access, BIOS
- `ng_color.h` - 16-bit color format manipulation
- `ng_palette.h` - Palette RAM management
- `ng_sprite.h` - Sprite Control Block operations
- `ng_fix.h` - Fix layer (text) rendering
- `ng_input.h` - Controller input with edge detection
- `ng_audio.h` - ADPCM-A/B audio playback
- `neogeo_hal.h` - Master header (includes Core + all HAL modules)

### SDK Modules (`sdk/include/`)

- `actor.h` - Scene actors (sprites with animation)
- `scene.h` - World management and rendering
- `camera.h` - Viewport into the scene
- `backdrop.h` - Parallax background layers
- `terrain.h` - Tile-based levels with collision
- `physics.h` - Rigid body physics
- `lighting.h` - Palette effects (day/night, flash)
- `graphic.h` - Low-level sprite rendering
- `visual.h` - Visual asset structures
- `spring.h` - Animation easing
- `ui.h` - Menu system
- `engine.h` - Game loop lifecycle
- `progearsdk.h` - Master header (includes HAL + all SDK modules)

### Core Abstractions

- **Scene**: Infinite canvas with X (right+), Y (down+, max 512px), Z (render order) coordinates
- **Actor**: Visual objects created from assets, positioned with fixed-point coordinates
- **Camera**: 320x224 viewport into the scene

### Fixed-Point Math

The 68000 has no FPU. All positions and physics use fixed-point:
- `fixed` (16.16): 32-bit, use `FIX(n)` to convert integers
- `fixed16` (8.8): 16-bit, use `FIX16(n)`
- `FIX_MUL(a, b)` for multiplication (optimized for m68k)
- Angles: 0-255 represents 0-360 degrees

### Asset Pipeline

Assets are defined in `assets.yaml` and processed by `tools/progear_assets.py`:
- Visual sprites generate C-ROM data and `progear_assets.h`
- Audio samples generate V-ROM data
- Terrain (from `tilemaps:` section, Tiled TMX format) generates collision and tile data
- Lighting presets pre-bake palette variants

Generated header is included as `<progear_assets.h>` and contains `NGVisualAsset_*` structs.

### Directory Structure

```
ProGearSDK/
├── core/                 # Foundation Library (types, math, memory)
│   ├── include/          # Core public headers (ng_types.h, ng_math.h, ng_arena.h)
│   ├── src/              # Core implementation
│   └── build/            # Output: libneogeo_core.a
├── hal/                  # Hardware Abstraction Layer
│   ├── include/          # HAL public headers (ng_*.h)
│   ├── src/              # HAL implementation
│   └── build/            # Output: libneogeo.a
├── sdk/                  # Game Engine SDK
│   ├── include/          # SDK public headers
│   ├── src/              # SDK implementation
│   └── build/            # Output: libprogearsdk.a
├── demos/
│   ├── showcase/         # Feature demo
│   └── template/         # Starter template
└── tools/                # Asset pipeline tools
```

### Demo Structure

```
demos/showcase/
├── assets.yaml       # Asset definitions
├── src/main.c        # Entry point
└── build/gen/        # Generated: progear_assets.h, ROM data
```

## Hardware Constraints

- 320x224 resolution, 381 sprites (96 per scanline)
- 256 palettes of 16 colors each
- Fix layer: 40x32 tiles for text/HUD (always on top)
- Scene Y-axis limited to 512 pixels due to hardware

## Type Conventions

- `u8`, `u16`, `u32`: unsigned integers
- `s8`, `s16`, `s32`: signed integers
- `fixed`, `fixed16`: fixed-point types
- `NGActorHandle`, `NGTerrainHandle`: opaque handles to SDK objects
