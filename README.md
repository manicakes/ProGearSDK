# ProGearSDK

ProGearSDK is a game engine for the NeoGeo AES/MVS. The NeoGeo has been around
for over thirty years now, and people are still building games for it, and this
SDK is how you do that. You write your game code in C, and ProGearSDK handles
the sprites and the scenes and the input and the sound, and it compiles down to
a ROM that runs on the actual hardware — a Motorola 68000 at 12 MHz with no
floating-point unit.

> **This is alpha software.** It is incomplete, and there are bugs, and things
> will break, and the API will change without notice. It is strictly for
> hobbyist experimentation. No support is provided. Issues and questions will
> not be answered. Use it at your own risk.

## How It Is Built

The SDK is three libraries, and each one builds on the one below it.

```
┌───────────────────────────────────────────────────────┐
│                      Your Game                        │
├───────────────────────────────────────────────────────┤
│               SDK (libprogearsdk.a)                   │
│   Scenes, Actors, Cameras, Physics, UI, Lighting      │
├───────────────────────────────────────────────────────┤
│                 HAL (libneogeo.a)                     │
│   Sprites, Palettes, Input, Audio, Fix Layer, Color   │
├───────────────────────────────────────────────────────┤
│              Core (libneogeo_core.a)                  │
│   Types, Fixed-Point Math, Vectors, Arena Allocator   │
├───────────────────────────────────────────────────────┤
│                   NeoGeo Hardware                     │
│   68000 CPU, VRAM, Palette RAM, Z80 Sound CPU         │
└───────────────────────────────────────────────────────┘
```

**[Core](core/)** is the bottom layer. It has no hardware dependencies at all —
it is just types and fixed-point math and an arena allocator. Every other part
of the SDK depends on Core, but Core depends on nothing.

**[HAL](hal/)** is the hardware layer. It sits on top of Core, and it is the
part that actually talks to the NeoGeo — the sprites, the palettes, the
controllers, the audio hardware. HAL also carries the startup code and the
linker script and the Z80 audio driver, so it has everything you need to get a
program running on the machine.

**[ProGear](progear/)** is the engine itself, and it sits on top of both Core
and HAL. This is where the high-level abstractions live. You get Scenes, which
are the world your game takes place in. You get Actors, which are the objects
in that world — they have position and animation and they move around. You get
Cameras, which define the 320x224 viewport the player actually sees. And you
get physics and lighting and terrain and backdrop layers and a menu system.

## Getting Started

Here is what a minimal program looks like. You include the headers, and you
initialize the engine, and you create an actor from an asset, and you place it
in the scene. Then you enter the main loop, and every frame you check the
input and move the actor accordingly:

```c
#include <progear.h>
#include <progear_assets.h>

int main(void) {
    NGEngineInit();

    NGActorHandle player = NGActorCreate(&NGVisualAsset_player, 0, 0);
    NGActorAddToScene(player, FIX(160), FIX(112), 0);

    for (;;) {
        NGEngineFrameStart();

        if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT))  NGActorMove(player, FIX(-2), 0);
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) NGActorMove(player, FIX(2), 0);

        NGEngineFrameEnd();
    }
}
```

That is the whole structure. You create things, you put them in the scene, and
then you run the loop — read input, update state, render — and the engine does
the rest. Every frame, sixty times a second.

## Building

You build with `make`. The default target builds everything — Core and HAL and
ProGear and all the demos:

```bash
make              # Build everything
make core         # Build Core only (outputs core/build/libneogeo_core.a)
make hal          # Build Core + HAL (outputs hal/build/libneogeo.a)
make progear      # Build all three libraries (outputs progear/build/libprogearsdk.a)
make showcase     # Build and run the showcase demo
make docs         # Generate API docs (requires Doxygen)
```

To run a demo in MAME:

```bash
cd demos/showcase
make mame
```

## Project Structure

```
ProGearSDK/
├── core/             # Core library — types, math, memory
│   ├── include/      # Headers: ng_types.h, ng_math.h, ng_arena.h
│   └── src/
├── hal/              # HAL library — hardware access
│   ├── include/      # Headers: ng_sprite.h, ng_palette.h, ng_input.h, etc.
│   ├── src/
│   ├── startup/      # 68000 startup code (crt0.s)
│   ├── z80/          # Z80 audio driver
│   └── rom/          # Linker script and system fix tiles
├── progear/          # ProGear engine library
│   ├── include/      # Headers: actor.h, scene.h, camera.h, etc.
│   └── src/
├── demos/
│   ├── showcase/     # Feature demonstration
│   └── template/     # Starter template for new games
└── tools/            # Asset pipeline (Python)
```

## Requirements

You need three tools to build the SDK:

- **m68k-elf-gcc** — the cross-compiler that targets the 68000
- **SDCC** — specifically the sdasz80 assembler, because the Z80 sound CPU
  needs its own code
- **Python 3.8+** — with Pillow and PyYAML, because the asset pipeline uses
  them to process sprites and audio and terrain data

On macOS:

```bash
brew install m68k-elf-gcc m68k-elf-binutils sdcc
pip install -r requirements.txt
```

## Hardware Constraints

The NeoGeo has hard limits, and you need to know them, because the hardware
does not give you any flexibility here:

| Component | Specification |
|-----------|---------------|
| CPU | Motorola 68000 @ 12 MHz — no FPU, so all math is fixed-point |
| Resolution | 320x224 pixels |
| Sprites | 381 total, 96 per scanline — if you put too many on one line, they drop out |
| Palettes | 256 palettes of 16 colors each |
| Fix layer | 40x32 character tiles, always on top — this is where your HUD and text go |
| Scene Y-axis | 512 pixels maximum, due to hardware |

Every NeoGeo game ever made worked within these same constraints. The SDK does
not try to hide them from you — it gives you tools to work within them.

## Resources

- [NeoGeo Development Wiki](https://wiki.neogeodev.org) — community-maintained
  documentation on the hardware, the BIOS, the memory map, all of it
- [68000 Programmer's Reference](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf) —
  the CPU reference manual from Motorola

## License

MIT License
