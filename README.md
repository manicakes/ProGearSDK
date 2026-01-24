# ProGearSDK

A modern game engine SDK for the NeoGeo AES/MVS.

> **Warning: Alpha Software**
>
> This project is incomplete and under active development. Expect bugs, missing features, and breaking changes without notice. Suitable only for hobbyist experimentation.
>
> **No support is provided.** Issues and questions will not be answered. Use at your own risk.

## Architecture

ProGearSDK uses a two-layer architecture:

```
┌─────────────────────────────────────────────────────┐
│                    Your Game                         │
├─────────────────────────────────────────────────────┤
│              SDK (libprogearsdk.a)                   │
│   Scenes, Actors, Cameras, Physics, UI, Lighting    │
├─────────────────────────────────────────────────────┤
│              HAL (libneogeo.a)                       │
│   Sprites, Palettes, Input, Audio, Fix Layer, Math  │
├─────────────────────────────────────────────────────┤
│              NeoGeo Hardware                         │
│   68000 CPU, VRAM, Palette RAM, Z80 Sound CPU       │
└─────────────────────────────────────────────────────┘
```

- **[HAL](hal/)** - Hardware Abstraction Layer providing direct access to NeoGeo hardware
- **[SDK](sdk/)** - High-level game engine with scenes, actors, cameras, and more

## Quick Start

```c
#include <progearsdk.h>
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

## Building

```bash
make              # Build everything (HAL + SDK + demos)
make hal          # Build HAL only (hal/build/libneogeo.a)
make sdk          # Build SDK only (sdk/build/libprogearsdk.a)
make showcase     # Build and run showcase demo
```

### Running Demos

```bash
cd demos/showcase
make mame         # Build and run in MAME emulator
```

## Project Structure

```
ProGearSDK/
├── hal/              # Hardware Abstraction Layer
│   ├── include/      # HAL headers (ng_*.h)
│   └── src/          # HAL implementation
├── sdk/              # Game Engine SDK
│   ├── include/      # SDK headers
│   └── src/          # SDK implementation
├── demos/
│   ├── showcase/     # Feature demonstration
│   └── template/     # Starter template
└── tools/            # Asset pipeline
```

## Requirements

- **m68k-elf-gcc** - 68000 cross-compiler
- **SDCC** - Z80 assembler (sdasz80)
- **Python 3.8+** with Pillow, PyYAML

### macOS

```bash
brew install m68k-elf-gcc m68k-elf-binutils sdcc
pip install -r requirements.txt
```

## Hardware Reference

| Component | Specification |
|-----------|---------------|
| CPU | Motorola 68000 @ 12 MHz (no FPU) |
| Resolution | 320x224 pixels |
| Sprites | 381 total, 96 per scanline |
| Palettes | 256 palettes × 16 colors |

## Resources

- [NeoGeo Development Wiki](https://wiki.neogeodev.org)
- [68000 Programmer's Reference](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf)

## License

MIT License
