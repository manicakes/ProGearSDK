# HAL Separation Proposal

This document proposes separating the ProGearSDK into two libraries:
1. **libneogeo** - Low-level NeoGeo Hardware Abstraction Layer (HAL)
2. **libprogearsdk** - High-level game engine (links to libneogeo)

## Rationale

- Clear separation of concerns between hardware access and game logic
- HAL can be reused by other projects that don't need the full SDK
- Easier to port high-level SDK to emulator/PC builds (swap HAL implementation)
- Better testability - mock the HAL for unit testing game logic
- Cleaner dependency graph

---

## Proposed Structure

```
ProGearSDK/
├── hal/                          # NEW: Hardware Abstraction Layer
│   ├── include/
│   │   ├── neogeo_hal.h          # Master include for HAL
│   │   ├── ng_types.h            # Base types (u8, u16, fixed, etc.)
│   │   ├── ng_hardware.h         # Register definitions, VRAM macros
│   │   ├── ng_math.h             # Fixed-point math, vectors, trig
│   │   ├── ng_color.h            # Color format, blending
│   │   ├── ng_palette.h          # Palette RAM operations
│   │   ├── ng_sprite.h           # Sprite SCB operations
│   │   ├── ng_fix.h              # Fix layer / text
│   │   ├── ng_input.h            # Controller input
│   │   ├── ng_audio.h            # Z80 audio driver communication
│   │   └── ng_arena.h            # Memory arena allocator
│   ├── src/
│   │   ├── ng_math.c
│   │   ├── ng_color.c
│   │   ├── ng_palette.c
│   │   ├── ng_sprite.c
│   │   ├── ng_input.c
│   │   ├── ng_audio.c
│   │   └── ng_arena.c
│   ├── build/
│   │   └── libneogeo.a           # Output library
│   └── Makefile
│
├── sdk/                          # HIGH-LEVEL SDK (existing, reorganized)
│   ├── include/
│   │   ├── progearsdk.h          # Master include (also includes neogeo_hal.h)
│   │   ├── visual.h              # Asset definitions
│   │   ├── graphic.h             # Platform-agnostic graphics API
│   │   ├── actor.h               # Game actors
│   │   ├── scene.h               # Scene/world management
│   │   ├── camera.h              # Viewport/camera
│   │   ├── terrain.h             # Tilemap rendering + collision
│   │   ├── backdrop.h            # Parallax backgrounds
│   │   ├── physics.h             # Rigid body physics
│   │   ├── lighting.h            # Global lighting/palette effects
│   │   ├── spring.h              # Spring animations
│   │   ├── ui.h                  # Menu system
│   │   └── engine.h              # Application lifecycle
│   ├── src/
│   │   ├── graphic.c
│   │   ├── actor.c
│   │   ├── scene.c
│   │   ├── camera.c
│   │   ├── terrain.c
│   │   ├── backdrop.c
│   │   ├── physics.c
│   │   ├── lighting.c
│   │   ├── spring.c
│   │   ├── ui.c
│   │   └── engine.c
│   ├── build/
│   │   └── libprogearsdk.a       # Output library
│   └── Makefile                  # Links to ../hal/build/libneogeo.a
│
├── demos/
│   └── showcase/
│       └── Makefile              # Links: -lprogearsdk -lneogeo
│
└── Makefile                      # Top-level: builds hal, then sdk, then demos
```

---

## Module Classification

### HAL (libneogeo) - Hardware Layer

| Module | Description |
|--------|-------------|
| `ng_types.h` | `u8`, `u16`, `u32`, `s8`, `s16`, `s32`, `fixed`, `fixed16`, volatile variants |
| `ng_hardware.h` | Register addresses, VRAM macros, BIOS variables, 68k helpers |
| `ng_math.h` | Fixed-point arithmetic, sin/cos tables, vectors, angles |
| `ng_color.h` | `NG_RGB`, color blending, HSV conversion |
| `ng_palette.h` | Palette RAM read/write, gradients, fade effects |
| `ng_sprite.h` | SCB1-4 operations, sprite positioning, hardware Y conversion |
| `ng_fix.h` | Fix layer tile operations, text rendering |
| `ng_input.h` | Controller state polling, edge detection |
| `ng_audio.h` | Z80 command protocol, ADPCM-A/B playback |
| `ng_arena.h` | Bump-pointer arena allocator |

### SDK (libprogearsdk) - Game Engine Layer

| Module | Description | HAL Dependencies |
|--------|-------------|------------------|
| `visual.h` | Asset struct definitions | ng_types |
| `graphic.h` | Graphics abstraction (sprite allocation, rendering) | ng_sprite, ng_types |
| `actor.h` | Game objects with animation | graphic, ng_math |
| `scene.h` | World management, collision dispatch | terrain, ng_math |
| `camera.h` | Viewport, zoom, tracking, shake | ng_math, ng_hardware |
| `terrain.h` | Tilemap rendering, tile collision | graphic, ng_math |
| `backdrop.h` | Parallax layers | graphic, ng_math |
| `physics.h` | Rigid body simulation | ng_math |
| `lighting.h` | Palette-based lighting effects | ng_palette, ng_color, ng_math |
| `spring.h` | Physics-based animation | ng_math |
| `ui.h` | Menu system | graphic, ng_fix, spring, ng_arena |
| `engine.h` | Main loop, subsystem init | ui, ng_input |

---

## Dependency Graph

```
                    ┌─────────────────────────────────────────┐
                    │           APPLICATION (demo)            │
                    └─────────────────────────────────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    │         libprogearsdk (SDK)           │
                    │                                       │
                    │  ┌─────────┐ ┌────────┐ ┌─────────┐  │
                    │  │  actor  │ │ scene  │ │ camera  │  │
                    │  └────┬────┘ └───┬────┘ └────┬────┘  │
                    │       │          │           │        │
                    │  ┌────┴──────────┴───────────┴────┐   │
                    │  │           graphic              │   │
                    │  └────────────────────────────────┘   │
                    │       │                               │
                    │  ┌────┴────┐ ┌─────────┐ ┌────────┐  │
                    │  │backdrop │ │ terrain │ │physics │  │
                    │  └─────────┘ └─────────┘ └────────┘  │
                    │                                       │
                    │  ┌─────────┐ ┌────────┐ ┌────────┐   │
                    │  │lighting │ │ spring │ │   ui   │   │
                    │  └─────────┘ └────────┘ └────────┘   │
                    │                                       │
                    │  ┌─────────┐                          │
                    │  │ engine  │                          │
                    │  └─────────┘                          │
                    └───────────────────┬───────────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    │          libneogeo (HAL)              │
                    │                                       │
                    │  ┌────────┐ ┌────────┐ ┌─────────┐   │
                    │  │ sprite │ │palette │ │  fix    │   │
                    │  └────────┘ └────────┘ └─────────┘   │
                    │                                       │
                    │  ┌────────┐ ┌────────┐ ┌─────────┐   │
                    │  │ input  │ │ audio  │ │  arena  │   │
                    │  └────────┘ └────────┘ └─────────┘   │
                    │                                       │
                    │  ┌────────┐ ┌────────┐ ┌─────────┐   │
                    │  │  math  │ │ color  │ │hardware │   │
                    │  └────────┘ └────────┘ └─────────┘   │
                    │                                       │
                    │  ┌────────┐                           │
                    │  │ types  │                           │
                    │  └────────┘                           │
                    └───────────────────────────────────────┘
```

---

## API Prefix Convention

To avoid naming collisions and clarify which layer a function belongs to:

| Layer | Prefix | Example |
|-------|--------|---------|
| HAL types | `NG` | `NGColor`, `NGAngle`, `fixed` |
| HAL functions | `NG` | `NGPaletteSet()`, `NGSpriteSetPosition()` |
| SDK types | `NG` | `NGActor`, `NGScene`, `NGCamera` |
| SDK functions | `NG` | `NGActorCreate()`, `NGSceneUpdate()` |

The prefix stays the same (both use `NG`) because:
- They're both part of the ProGear/NeoGeo ecosystem
- HAL is an implementation detail hidden from game code
- The distinction is architectural, not API-level

---

## Header Include Strategy

### HAL Master Header (`neogeo_hal.h`)

```c
#ifndef NEOGEO_HAL_H
#define NEOGEO_HAL_H

#include "ng_types.h"
#include "ng_hardware.h"
#include "ng_math.h"
#include "ng_color.h"
#include "ng_palette.h"
#include "ng_sprite.h"
#include "ng_fix.h"
#include "ng_input.h"
#include "ng_audio.h"
#include "ng_arena.h"

#endif
```

### SDK Master Header (`progearsdk.h`)

```c
#ifndef PROGEARSDK_H
#define PROGEARSDK_H

// HAL is a dependency - include it
#include <neogeo_hal.h>

// SDK modules
#include "visual.h"
#include "graphic.h"
#include "actor.h"
#include "scene.h"
#include "camera.h"
#include "terrain.h"
#include "backdrop.h"
#include "physics.h"
#include "lighting.h"
#include "spring.h"
#include "ui.h"
#include "engine.h"

#endif
```

### Application Code

```c
#include <progearsdk.h>      // Gets everything (HAL + SDK)
#include <progear_assets.h>  // Generated assets

// Or for HAL-only projects:
#include <neogeo_hal.h>
```

---

## Build System Changes

### Top-Level Makefile

```makefile
.PHONY: all hal sdk demos clean

all: hal sdk demos

hal:
	$(MAKE) -C hal

sdk: hal
	$(MAKE) -C sdk

demos: sdk
	$(MAKE) -C demos/showcase

clean:
	$(MAKE) -C hal clean
	$(MAKE) -C sdk clean
	$(MAKE) -C demos/showcase clean
```

### HAL Makefile (`hal/Makefile`)

```makefile
TARGET = build/libneogeo.a

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=build/%.o)

CFLAGS += -Iinclude

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build
```

### SDK Makefile (`sdk/Makefile`)

```makefile
TARGET = build/libprogearsdk.a

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=build/%.o)

CFLAGS += -Iinclude -I../hal/include

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
```

### Demo Makefile

```makefile
LIBS = -L../../sdk/build -L../../hal/build -lprogearsdk -lneogeo
CFLAGS += -I../../sdk/include -I../../hal/include
```

---

## Migration Steps

### Phase 1: Create HAL Structure
1. Create `hal/` directory with `include/` and `src/` subdirs
2. Copy and rename files:
   - `types.h` → `ng_types.h`
   - `neogeo.h` → `ng_hardware.h`
   - `ngmath.h/.c` → `ng_math.h/.c`
   - `color.h/.c` → `ng_color.h/.c`
   - `palette.h/.c` → `ng_palette.h/.c`
   - `sprite.h/.c` → `ng_sprite.h/.c`
   - `fix.h/.c` → `ng_fix.h/.c`
   - `input.h/.c` → `ng_input.h/.c`
   - `audio.h/.c` → `ng_audio.h/.c`
   - `arena.h/.c` → `ng_arena.h/.c`
3. Create `neogeo_hal.h` master header
4. Create HAL Makefile
5. Build and verify `libneogeo.a`

### Phase 2: Update SDK
1. Remove HAL files from `sdk/include/` and `sdk/src/`
2. Update SDK includes to use `<neogeo_hal.h>` or specific `<ng_*.h>`
3. Update `progearsdk.h` to include HAL
4. Update SDK Makefile to depend on HAL
5. Build and verify `libprogearsdk.a`

### Phase 3: Update Demos
1. Update demo Makefiles to link both libraries
2. Update include paths
3. Build and test demos

### Phase 4: Update Tooling
1. Update `CLAUDE.md` with new structure
2. Update any documentation
3. Update CI/CD if applicable

---

## Open Questions

1. **Naming**: Should the HAL library be `libneogeo` or `libnghal` or something else?

2. **Header prefix**: Should HAL headers use `ng_` prefix or keep original names with a subdirectory (`neogeo/types.h`)?

3. **Asset pipeline**: The `visual.h` types are generated by the asset pipeline. Should generated assets:
   - Stay in SDK (current proposal)
   - Move to HAL (if assets are hardware-specific)
   - Have their own tiny library

4. **Math module**: `ng_math` is mostly pure computation (fixed-point, trig). Could argue it belongs in either layer. Current proposal keeps it in HAL since it's foundational and doesn't depend on game concepts.

5. **Arena allocator**: Similar to math - pure utility. Keeping in HAL for now.

---

## Benefits Summary

| Benefit | Description |
|---------|-------------|
| **Reusability** | HAL usable for non-ProGear NeoGeo projects |
| **Portability** | Swap HAL for SDL/PC implementation |
| **Testability** | Mock HAL for unit testing SDK logic |
| **Clarity** | Clear boundary between "how hardware works" and "how games work" |
| **Compilation** | HAL rarely changes; faster incremental builds |
| **Documentation** | Separate docs for hardware vs game engine concepts |
