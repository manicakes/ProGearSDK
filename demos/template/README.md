# ProGearSDK Template

A minimal starting point for NeoGeo game development with ProGearSDK.

## What It Does

- Displays a title on the fix layer
- Renders a 64x64 sprite in the center of the screen
- Allows the sprite to be moved with the joystick (D-Pad)

## Building

```bash
make          # Build ROM files (automatically builds SDK library first)
make mame     # Build and run in MAME
make clean    # Clean build artifacts
```

## Running in MAME

Before running with `make mame`, configure the path to your NeoGeo BIOS:

1. Open `Makefile`
2. Set `NEOGEO_BIOS_PATH` to the directory containing your `neogeo.zip` BIOS file

## Controls

- **D-Pad**: Move the sprite

## Using as a Starting Point

1. Copy this entire `template` directory
2. Rename it to your project name
3. Edit `assets.yaml` to add your graphics and audio
4. Edit `src/main.c` to implement your game logic
5. Update the `GAME_NAME` in `Makefile`
6. If moving outside the `demos/` directory, update `SDK_PATH` and `TOOLS_PATH` in the Makefile

## Project Structure

```
template/
├── src/
│   └── main.c           # Entry point and game loop
├── assets/
│   └── checkerboard.png # Sample sprite (64x64 red/blue checkerboard)
├── assets.yaml          # Asset definitions for ngres
├── Makefile
└── README.md
```

## License

This template is part of ProGearSDK and is released under the MIT License.
