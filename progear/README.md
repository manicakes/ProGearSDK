# ProGearSDK Game Engine

High-level game engine for NeoGeo development.

The SDK provides abstractions for building games using **Scenes**, **Actors**, and **Cameras**. It handles sprite management, animation, physics, and rendering so you can focus on gameplay.

The SDK builds on top of the HAL. Runtime components (startup code, linker script, Z80 driver, ROM data) are provided by the HAL - see [HAL documentation](../hal/).

## Building

```bash
make              # Build libprogearsdk.a (requires HAL)
make clean        # Remove build artifacts
make format       # Auto-format source files
make lint         # Run static analysis
make docs         # Generate API documentation
```

Output: `build/libprogearsdk.a`

## Usage

Include the master header to access all SDK (and HAL) functionality:

```c
#include <progear.h>
```

## Core Concepts

### Scene

The **Scene** is the stage where your game takes place—an infinite canvas with a coordinate system:

- **X axis**: 0 at left, increases going right (no limit)
- **Y axis**: 0 at top, increases going down (max 512 pixels)
- **Z axis**: Render order (higher Z = drawn in front)

The scene manages all objects and renders them in correct order.

### Actor

An **Actor** is any visual object: player, enemies, projectiles, platforms. Actors are created from visual assets and placed into the scene.

Each actor has:
- **Position** (x, y): Scene coordinates (fixed-point)
- **Z-index**: Depth in render order
- **Animation**: Current frames and playback
- **Appearance**: Palette, visibility, flipping

### Camera

The **Camera** is your viewport into the scene—a 320x224 window that determines what's visible on screen. The camera can track actors, shake, and zoom.

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

## Getting Started

### Define Assets (assets.yaml)

```yaml
visual_assets:
  - name: player
    source: assets/player.png
```

### Write Your Game (src/main.c)

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
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_UP))    NGActorMove(player, 0, FIX(-2));
        if (NGInputHeld(NG_PLAYER_1, NG_BTN_DOWN))  NGActorMove(player, 0, FIX(2));

        NGEngineFrameEnd();
    }
}
```

## Modules

### engine.h - Game Loop

```c
NGEngineInit()        // Initialize all subsystems
NGEngineFrameStart()  // Begin frame (timing, input)
NGEngineFrameEnd()    // End frame (render, vsync)
```

### actor.h - Game Objects

```c
// Lifecycle
NGActorHandle actor = NGActorCreate(&NGVisualAsset_player, width, height);
NGActorAddToScene(actor, x, y, z);
NGActorRemoveFromScene(actor);
NGActorDestroy(actor);

// Position
NGActorSetPos(actor, x, y);
NGActorMove(actor, dx, dy);
NGActorGetX(actor), NGActorGetY(actor);

// Animation
NGActorSetAnim(actor, index);
NGActorSetAnimByName(actor, "walk");
NGActorSetFrame(actor, frame);
NGActorAnimDone(actor);

// Appearance
NGActorSetVisible(actor, 1);
NGActorSetPalette(actor, palette);
NGActorSetHFlip(actor, 1);
NGActorSetVFlip(actor, 1);
NGActorSetScreenSpace(actor, 1);  // UI mode

// Audio
NGActorPlaySfx(actor, sfx_index);  // Spatial audio
```

### camera.h - Viewport

```c
// Position
NGCameraSetPos(x, y);
NGCameraGetX(), NGCameraGetY();

// Zoom (affects visible area)
NGCameraSetZoom(NG_CAM_ZOOM_100);   // 1x (320x224)
NGCameraSetZoom(NG_CAM_ZOOM_75);    // 0.75x (426x298 visible)
NGCameraSetZoom(NG_CAM_ZOOM_50);    // 0.5x (640x448 visible)
NGCameraSetTargetZoom(zoom);        // Smooth zoom

// Actor tracking
NGCameraTrackActor(actor);
NGCameraStopTracking();
NGCameraSetDeadzone(width, height);
NGCameraSetFollowSpeed(speed);
NGCameraSetBounds(width, height);

// Effects
NGCameraShake(intensity, duration);
NGCameraWorldToScreen(wx, wy, &sx, &sy);
```

### scene.h - World Management

```c
// Terrain (tile-based levels)
NGSceneSetTerrain(&NGTerrainAsset_level);
NGSceneClearTerrain();
NGSceneGetTerrainBounds(&width, &height);

// Collision detection
u8 hit = NGSceneResolveCollision(&x, &y, half_w, half_h, &vx, &vy);
if (hit & NG_COLL_BOTTOM) { /* landed */ }
if (hit & NG_COLL_LEFT)   { /* hit wall */ }
```

### backdrop.h - Parallax Layers

```c
// Create scrolling background
NGBackdropHandle bg = NGBackdropCreate(
    &NGVisualAsset_mountains,
    NG_BACKDROP_WIDTH_INFINITE,  // Tile horizontally
    0,                           // Fixed height
    FIX(0.5),        // X parallax (slower = further)
    FIX(0.5)         // Y parallax
);
NGBackdropAddToScene(bg, x, y, z);
NGBackdropRemoveFromScene(bg);
NGBackdropDestroy(bg);
```

### terrain.h - Tile-Based Levels

```c
// Load from Tiled TMX (via assets.yaml)
NGTerrainHandle level = NGTerrainCreate(&NGTerrainAsset_level1);
NGTerrainAddToScene(level, x, y, z);

// Collision
u8 hit = NGTerrainResolveAABB(level, &x, &y, half_w, half_h, &vx, &vy);

// Collision flags
NG_COLL_TOP, NG_COLL_BOTTOM, NG_COLL_LEFT, NG_COLL_RIGHT

// Cleanup
NGTerrainRemoveFromScene(level);
NGTerrainDestroy(level);
```

### physics.h - Rigid Body Simulation

```c
// Create physics world
NGPhysWorldHandle world = NGPhysWorldCreate();
NGPhysWorldSetGravity(world, 0, FIX(1));
NGPhysWorldSetBounds(world, min_x, max_x, min_y, max_y);

// Create bodies
NGBodyHandle body = NGPhysBodyCreateAABB(world, x, y, half_w, half_h);
NGPhysBodySetVel(body, vx, vy);
NGPhysBodySetRestitution(body, FIX(0.8));

// Update (call each frame)
NGPhysWorldUpdate(world, collision_callback, user_data);

// Query
NGVec2 pos = NGPhysBodyGetPos(body);
NGVec2 vel = NGPhysBodyGetVel(body);

// Cleanup
NGPhysBodyDestroy(body);
NGPhysWorldDestroy(world);
```

### lighting.h - Visual Effects

```c
// Instant effects
NGLightingFlash(r, g, b, frames);  // Screen flash

// Pre-baked presets (zero CPU cost)
NGLightingLayerHandle night = NGLightingPushPreset(
    NG_LIGHTING_PREBAKED_NIGHT, fade_frames);
NGLightingPopPreset(night, fade_frames);
NGLightingUpdatePrebakedFade();  // Call each frame
NGLightingIsPrebakedFading();
```

### ui.h - Menu System

```c
// Create menu
NGMenuHandle menu = NGMenuCreateDefault(&ng_arena_state, max_items);
NGMenuSetTitle(menu, "OPTIONS");
NGMenuAddItem(menu, "Resume");
NGMenuAddItem(menu, "Settings");
NGMenuAddSeparator(menu, "---");
NGMenuAddItem(menu, "Quit");

// Update (call each frame)
NGMenuUpdate(menu);

// State
NGMenuShow(menu);
NGMenuHide(menu);
if (NGMenuConfirmed(menu)) {
    u8 sel = NGMenuGetSelection(menu);
}
if (NGMenuCancelled(menu)) { /* B pressed */ }

// Cleanup
NGMenuDestroy(menu);
```

### visual.h - Asset Structures

Assets are defined in `assets.yaml` and generate `progear_assets.h`:

```yaml
visual_assets:
  - name: player
    source: assets/player.png
    frame_size: [32, 32]
    animations:
      idle: { frames: [0], speed: 1, loop: true }
      walk: { frames: [0, 1, 2, 3], speed: 4, loop: true }
```

Generated code:

```c
extern const NGVisualAsset NGVisualAsset_player;
#define NGPAL_PLAYER 3  // Auto-assigned palette
```

## Animation Example

```c
static u8 is_moving = 0;

for (;;) {
    NGEngineFrameStart();

    u8 moving = 0;
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_LEFT)) {
        NGActorMove(player, FIX(-2), 0);
        NGActorSetHFlip(player, 1);
        moving = 1;
    }
    if (NGInputHeld(NG_PLAYER_1, NG_BTN_RIGHT)) {
        NGActorMove(player, FIX(2), 0);
        NGActorSetHFlip(player, 0);
        moving = 1;
    }

    if (moving != is_moving) {
        is_moving = moving;
        NGActorSetAnimByName(player, moving ? "walk" : "idle");
    }

    NGEngineFrameEnd();
}
```

## Camera Tracking Example

```c
// Setup
NGCameraTrackActor(player);
NGCameraSetDeadzone(80, 40);        // Don't move until player near edge
NGCameraSetFollowSpeed(FIX(0.12));  // Smooth follow
NGCameraSetBounds(level_width, level_height);  // Don't show outside level

// The camera now automatically follows the player each frame
```

## See Also

- [HAL Documentation](../hal/) - Low-level hardware access
- [Demo Projects](../demos/) - Working examples
- [Asset Pipeline](../tools/) - How to define assets
