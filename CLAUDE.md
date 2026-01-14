# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ProGearSDK is a game engine SDK for the NeoGeo AES/MVS console (Motorola 68000 @ 12MHz, no FPU). It provides abstractions for building games using Scenes, Actors, and Cameras.

## Build Commands

```bash
# Build everything (SDK library + all demos)
make

# Build only the SDK library (outputs sdk/build/libprogearsdk.a)
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

### Core Abstractions

- **Scene**: Infinite canvas with X (right+), Y (down+, max 512px), Z (render order) coordinates
- **Actor**: Visual objects created from assets, positioned with fixed-point coordinates
- **Camera**: 320x224 viewport into the scene

### SDK Structure

```
sdk/
├── include/      # Public headers (progearsdk.h includes all)
├── src/          # Implementation
└── build/        # Output: libprogearsdk.a
```

Key modules: `actor.h` (sprites), `scene.h` (world management), `camera.h` (viewport), `tilemap.h` (tile-based levels with collision), `input.h` (controller), `physics.h` (rigid body), `ngmath.h` (fixed-point math), `lighting.h` (palette effects), `audio.h` (ADPCM sound/music)

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
- Tilemaps (Tiled TMX format) generate collision and tile data
- Lighting presets pre-bake palette variants

Generated header is included as `<progear_assets.h>` and contains `NGVisualAsset_*` structs.

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
- `NGActorHandle`, `NGTilemapHandle`: opaque handles to SDK objects
