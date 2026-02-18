# ProGearSDK

Listen — there is a machine, and the machine is the NeoGeo, and it has lived for
thirty years and more, and people still gather around it, and they still want to
make games for it. And so we built ProGearSDK.

ProGearSDK is an engine for that machine. You give it your sprites and your
sounds and your ideas, and it gives you back a game that runs on the real
hardware — the mighty Motorola 68000, running at twelve megahertz, with no
floating-point unit, no shortcuts, no safety net.

> **Hear this well — the SDK is young, and it is rough.**
>
> It is alpha software, and it is incomplete, and there are bugs in it, and
> things will break, and things will change without warning. It is for
> tinkerers and experimenters and the brave-hearted. No support is given.
> No questions will be answered. You use it, and you use it at your own risk.

## How It Is Built

Now, the SDK is not one thing but three things stacked together, and each one
rests on the one beneath it, and you should know them by name.

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

At the bottom sits **[Core](core/)**, the foundation, the bedrock. Core knows
nothing of the hardware — it knows only numbers and types and memory. It gives
you your fixed-point math, and it gives you your arena allocator, and it gives
you the types that every other piece depends on. Core is patient and Core is
steady and Core asks nothing of the machine.

Above Core sits **[HAL](hal/)**, the Hardware Abstraction Layer, and HAL is the
one who speaks to the NeoGeo itself. HAL knows the sprites and HAL knows the
palettes and HAL knows the input and HAL knows the sound. It carries the startup
code and the linker script and the Z80 audio driver — everything you need to
make the machine wake up and listen. HAL depends on Core, and HAL depends on
nothing else.

And above them both sits **[ProGear](progear/)**, the engine proper, the high
place where you do your real work. ProGear gives you Scenes — and Scenes are
the great canvas on which everything is drawn. ProGear gives you Actors — and
Actors are the living things that move and animate upon that canvas. ProGear
gives you Cameras — and Cameras are the window, the viewport, the eye through
which the player sees the world. And ProGear gives you physics and lighting and
terrain and backdrops and menus, and it builds upon Core and it builds upon HAL,
and together the three of them hold up your game.

## Come, Let Us Make Something

Here is how you begin. You include the headers, and you initialize the engine,
and you create an actor, and you place that actor in the scene, and then you
enter the great loop — the loop that never ends — and inside the loop you read
the player's input and you move the actor and the engine draws the frame, and
then it does it again, and again, and again, sixty times every second, forever:

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

You see it? You create the player, and you add the player to the scene, and then
you listen — left, right, left, right — and you move, and the engine takes care
of the rest.

## Building Your Work

When you are ready to build, you go to the command line, and you call upon
`make`, and `make` will do the work:

```bash
make              # Build everything — Core and HAL and ProGear and all the demos
make core         # Build Core alone (and you get core/build/libneogeo_core.a)
make hal          # Build Core and HAL together (and you get hal/build/libneogeo.a)
make progear      # Build all three (and you get progear/build/libprogearsdk.a)
make showcase     # Build the showcase demo and run it
make docs         # Generate the documentation (you will need Doxygen for this)
```

And when you want to see your demo running — when you want to see it alive and
moving on the screen — you go into the demo directory and you call upon MAME:

```bash
cd demos/showcase
make mame         # Build the demo and run it in the MAME emulator
```

And MAME will open, and your creation will appear, and you will see what you
have made.

## Where Everything Lives

Now, the files and folders — you must know where things are, so listen:

```
ProGearSDK/
├── core/             # Here is Core, the foundation
│   ├── include/      # Its headers: ng_types.h, ng_math.h, ng_arena.h
│   └── src/          # Its source, its implementation
├── hal/              # Here is HAL, who speaks to the hardware
│   ├── include/      # Its headers, all named ng_*.h
│   ├── src/          # Its source
│   ├── startup/      # The 68000 startup code, the crt0.s that wakes the machine
│   ├── z80/          # The Z80 audio driver, the faithful servant of sound
│   └── rom/          # ROM support — the linker script, the system fix tiles
├── progear/          # Here is ProGear, the engine itself
│   ├── include/      # Its headers
│   └── src/          # Its source
├── demos/
│   ├── showcase/     # A demonstration of what the engine can do
│   └── template/     # A bare starting point for your own game
└── tools/            # The asset pipeline, the tools that prepare your art and sound
```

## What You Need Before You Begin

Before you can build, you must gather your tools. You need three things:

- **m68k-elf-gcc** — the cross-compiler, the one that speaks the language of the
  68000
- **SDCC** — for the Z80 assembler, sdasz80, because the sound CPU has its own
  code and its own tongue
- **Python 3.8 or newer** — with the Pillow library and with PyYAML, because the
  asset pipeline is written in Python and it needs these to do its work

On macOS you gather them like this:

```bash
brew install m68k-elf-gcc m68k-elf-binutils sdcc
pip install -r requirements.txt
```

And then you are ready.

## Know Your Machine

The NeoGeo is a particular machine, and it has particular limits, and you must
know them and you must respect them, because the machine will not bend for you:

| What | How Much |
|------|----------|
| The CPU | A Motorola 68000, twelve megahertz, and no floating-point unit — every decimal is fixed-point, every multiplication is done by hand |
| The screen | 320 pixels across and 224 pixels down, and not one pixel more |
| The sprites | 381 in total, but only 96 on any single scanline — crowd them together and they will vanish |
| The palettes | 256 of them, and each one holds 16 colors — that is all the color you get |

These are the walls of the arena. Learn them. Work within them. The great games
were all made inside these same walls.

## Where to Learn More

- [The NeoGeo Development Wiki](https://wiki.neogeodev.org) — where the
  community has gathered its knowledge, and where you can learn the hardware
  from the ground up
- [The 68000 Programmer's Reference](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf) —
  the manual for the CPU itself, from Motorola, who made it

## License

MIT License — take it and use it and build with it and share what you make.
