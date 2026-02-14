# ProGearSDK Technical Review

## Executive Summary

ProGearSDK is a well-structured game engine SDK for the NeoGeo AES/MVS platform. The three-layer architecture (Core, HAL, ProGear) is sound, the API surface is clean, and the codebase demonstrates genuine understanding of the 68000 and NeoGeo hardware. The asset pipeline is a major strength. That said, there are real issues at both the low level (m68k code generation concerns, VRAM timing hazards, memory safety) and the high level (API friction, missing features, scalability limits). This review covers all of them.

---

## 1. Architecture

### Strengths

**Clean three-layer separation.** Core has zero hardware dependencies, HAL wraps the hardware, ProGear provides game-level abstractions. This is the right decomposition. A developer who only wants hardware access can link HAL alone (the `hal-template` demo proves this works). A game developer can use ProGear and never think about VRAM addresses.

**The Graphic abstraction is the architectural centerpiece.** Rather than having actors, backdrops, and terrain each manage sprites independently, they all funnel through `NGGraphic`, which handles sprite allocation, Z-sorting, tile writing, scaling, and visibility. This is a critical design decision that prevents the kind of sprite-index collision bugs that plague NeoGeo homebrew projects. The `graphic.c` file at ~1640 lines is the most complex module, but that complexity is well-contained.

**Handle-based object management.** Actors, terrains, backdrops, and lighting layers are all accessed via opaque handles into static arrays. This avoids heap allocation entirely, which is correct for a system with 64KB of RAM and no OS. The arena allocator for graphics is appropriate for the bump-and-reset allocation pattern games naturally use.

**Asset pipeline as a first-class concern.** The `progear_assets.py` tool handles the entire path from PNG/WAV/TMX to NeoGeo ROM format, including ADPCM encoding, C-ROM tile packing, palette extraction, tilemap collision data, and even pre-baked lighting palettes. This is the kind of tooling that makes or breaks a retro SDK -- without it, developers spend more time fighting ROM formats than making games.

### Concerns

**Single-camera, single-scene assumption baked into global state.** Camera, scene, lighting, and input are all module-level singletons accessed via global functions. There is no way to have two cameras (e.g., split-screen), two scenes (e.g., a pause-screen overlay with its own actor set), or two input contexts. This is a reasonable constraint for NeoGeo games, but it should be documented as a deliberate architectural decision rather than left implicit. If split-screen 2-player ever becomes a goal, significant refactoring would be needed.

**The `sdk_internal.h` coupling.** ProGear modules communicate through forward-declared internal functions like `_NGActorCollectPalettes`, `_NGBackdropSyncGraphics`, `_NGTerrainSyncGraphics`. These form an implicit dependency graph: Scene calls Backdrop/Terrain sync, Lighting calls Actor/Backdrop/Terrain palette collection. This works, but the coupling is invisible to someone reading any single module. A comment block in `sdk_internal.h` documenting the call graph would help.

**No explicit initialization ordering contract.** `NGEngineInit()` calls subsystem inits in a specific order (Arena, Palette, Fix, Scene, Camera, Input, Audio, Lighting, Assets, Backdrop). If a user calls `NGSceneInit()` before `NGArenaSystemInit()`, they'll get silent corruption. The SDK should either enforce init ordering (e.g., assert that arenas are initialized) or document it prominently.

---

## 2. Low-Level / m68k-Specific Issues

### 2.1 Fixed-Point Multiply: FIX_MUL

```c
static inline fixed FIX_MUL(fixed a, fixed b) {
    s16 a_hi = (s16)(a >> 16);
    u16 a_lo = (u16)a;
    s16 b_hi = (s16)(b >> 16);
    u16 b_lo = (u16)b;
    s32 result = ((s32)a_hi * b_hi) << 16;
    result += (s32)a_hi * b_lo;
    result += (s32)a_lo * b_hi;
    result += ((u32)a_lo * b_lo) >> 16;
    return result;
}
```

This is the correct decomposition for a 68000 (four 16x16 multiplies instead of emulated 32x32). However:

- **Sign handling in the `a_lo * b_hi` term is subtle.** `a_lo` is `u16` and `b_hi` is `s16`. The C standard promotes `u16` to `int` for the multiply, which works correctly on a 32-bit int compiler. But the comment should explain why `a_lo` is unsigned -- it's because the low 16 bits of a signed 32-bit value are always a positive magnitude contribution. The correctness depends on this being compiled with 32-bit int, which is true for m68k-elf-gcc but should be documented.

- **The `a_lo * b_lo` term uses `(u32)` cast.** This forces an unsigned 16x16 multiply (`MULU`), which is correct. But the `>> 16` discard of the low 16 bits means we're losing the carry contribution to the fractional part. This is standard for 16.16 fixed-point but introduces a consistent rounding-down bias. For physics accumulation over many frames this can drift. Consider a rounding variant for cases like `NGVec2Normalize`.

- **`static inline` in a header means this gets inlined everywhere.** That's 4 MULU instructions (38-70 cycles each) per call site. For tight loops (physics with many bodies, lighting palette resolution), this code bloat can push hot functions out of the CPU's prefetch. Consider providing both an inline and a non-inline (function call) variant, letting the developer choose.

### 2.2 FIX_DIV Uses 64-bit Division

```c
#define FIX_DIV(a, b) ((fixed)((((long long)(a)) << FIX_SHIFT) / (b)))
```

The 68000 has no 64-bit divide instruction. This will emit a call to a GCC soft-division routine (`__divdi3` or similar), which is extremely expensive -- easily 500+ cycles. Every call to `NGVec2Normalize` (which does two `FIX_DIV`s) and `NGAtan2` (one `FIX_DIV`) pays this cost.

Recommendations:
- Provide a lookup-table reciprocal for common denominators.
- For `NGVec2Normalize`, consider a fast inverse-square-root approximation instead of `sqrt` + two divides.
- At minimum, document that `FIX_DIV` is very slow and should be avoided in per-frame code.

### 2.3 Sin Table Precision

```c
static const fixed sin_table[256] = { 0, 804, 1607, ... };
// ...
fixed NGSin(angle_t angle) {
    return ((fixed)sin_table[angle]) << 1;
}
```

The table stores values in 0.15 format (max 32767), then shifts left by 1 to get 0.16 format. This means the effective range is -65534 to +65534, not the full -65536 to +65536. The peak value at index 64 is `32767 << 1 = 65534`, which is `FIX_ONE - 2`. This means `NGSin(ANGLE_90)` returns 0.99997 instead of 1.0.

For most game code this is fine, but it can cause subtle issues:
- A projectile launched at exactly 90 degrees will have a tiny X component instead of zero.
- Rotation matrix composition over many frames can accumulate error.

Fix: Change the table entry at index 64 to 32768 (which after `<< 1` gives exactly `FIX_ONE = 65536`). The signed overflow is not an issue because `s32` holds this value fine.

### 2.4 VRAM Timing Hazards

The SDK writes to VRAM (sprite tile data, SCB registers, fix layer) during active display. On the NeoGeo, VRAM writes during active display are legal but have timing constraints -- the LSPC may not process writes consistently if they collide with the display pipeline.

`NGEngineFrameEnd()` calls `NGSceneDraw()`, which calls `_NGGraphicSystemDraw()`. If the game's update logic is fast and `FrameEnd` executes well before VBlank, these VRAM writes happen during active display. The `FrameStart` function correctly waits for VBlank and does fix-layer writes immediately after, but sprite VRAM writes in `FrameEnd` have no such protection.

In practice, most NeoGeo games write SCB2/SCB3/SCB4 during active display without issues (these are latched at VBlank). SCB1 (tile data) writes during active display can cause single-frame glitches on real hardware. The graphic system writes SCB1 tiles as part of `_NGGraphicSystemDraw()`.

Recommendations:
- Double-buffer the SCB1 writes: prepare tile data during active display into a RAM buffer, then flush to VRAM in the VBlank handler.
- Alternatively, document that SCB1 writes are best done during VBlank and provide a mechanism to defer them.

### 2.5 VBlank Handler Register Preservation

```asm
_vblank:
    movem.l %d0-%d1/%a0-%a1, -(%sp)
```

The handler only saves d0-d1/a0-a1. If the custom VBlank handler (called via `ng_vblank_handler`) uses d2-d7 or a2-a4, those registers will be corrupted in the interrupted code. The C calling convention for m68k-elf-gcc treats d2-d7/a2-a4 as callee-saved, so a C function called from the VBlank handler will save/restore them. But an assembly handler or a C handler that the compiler doesn't emit save/restore code for (e.g., with `__attribute__((interrupt))`) could corrupt them.

This is technically correct if all VBlank handlers are regular C functions (callee-saved convention handles it), but it's fragile. Saving d0-d7/a0-a6 would be safer at the cost of ~16 extra cycles per VBlank (negligible at 60Hz).

### 2.6 Memory Functions Are Byte-at-a-Time

```c
void *memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) { *d++ = *s++; }
    return dest;
}
```

The 68000 can do word (16-bit) and long (32-bit) moves, which are 2x and 4x faster respectively for aligned data. A `memcpy` that copies longs for the bulk and bytes for the remainder would significantly improve palette backup/restore, arena operations, and GCC-generated struct copies. This is particularly important because the lighting system copies up to 32 palettes (32 * 32 = 1024 bytes) per frame via `NGPalBackup`/`NGPalRestore`.

### 2.7 NG_VRAM_DECLARE_BASE() Repeated in Every Function

Every function in `ng_sprite.c` calls `NG_VRAM_DECLARE_BASE()`, which loads `0x3C0000` into register a5. Functions like `NGSpriteTileWrite` that are called in sequence (after `NGSpriteTileBegin`) redundantly reload a5. The compiler may optimize this away when inlined, but as standalone functions, each call wastes 12 cycles loading the constant.

Consider providing a "batch" API that takes the base pointer as a parameter, or making these functions `static inline` in the header so the compiler can fold redundant loads.

---

## 3. High-Level API Issues

### 3.1 Limited Sprite Count Management

The NeoGeo has 381 sprites. The graphic system allocates them from a pool, but there's no visibility into how many are available, no priority system for when sprites run out, and no mechanism to reclaim sprites from off-screen objects.

In a real game, running out of sprites is a constant concern. The SDK should provide:
- `u16 NGGraphicGetSpriteCount(void)` -- total sprites in use.
- `u16 NGGraphicGetAvailableSprites(void)` -- sprites remaining.
- Optionally, a priority-based eviction system where low-priority graphics are hidden when sprites are exhausted (common in commercial NeoGeo games).

### 3.4 No Sprite Linking / Chaining API at ProGear Level

The NeoGeo's "sticky bit" in SCB3 chains sprites vertically, allowing multi-tile-tall sprites. The HAL exposes this via `NGSpriteYSetChain`, and the Graphic system uses it internally. But there's no ProGear-level concept of "this actor is 3 tiles tall and 2 tiles wide." The visual asset system defines width/height in pixels, and the graphic system calculates tile counts internally.

This works for the common case, but doesn't expose the ability to:
- Dynamically change an actor's visible tile subset (e.g., damage revealing internal structure).
- Compose an actor from multiple visual assets (e.g., separate head/body/legs for a character with modular equipment).

### 3.5 Terrain Tile Modification Is Unimplemented

```c
void NGTerrainSetTile(NGTerrainHandle handle, u16 tile_x, u16 tile_y, u8 tile_index) {
    (void)handle; (void)tile_x; (void)tile_y; (void)tile_index;
}
```

Both `NGTerrainSetTile` and `NGTerrainSetCollision` are stubs. The header documents them as requiring "RAM copy support." This is a significant gap -- destructible terrain, collectibles that disappear, and doors that open are standard platformer features. The terrain data is in ROM (const pointers from the asset pipeline), so runtime modification requires copying to RAM first. The arena allocator could serve this purpose.

### 3.6 No Object Pooling or Entity System

Games typically have many instances of the same object type (bullets, particles, enemies). The SDK provides raw actor slots (up to `NG_ACTOR_MAX`) but no pooling mechanism. Each bullet requires a full `NGActorCreate` / `NGActorDestroy` cycle, which includes graphic allocation and VRAM setup.

A lightweight pool -- pre-allocate N actors of a given type, show/hide them as needed -- would dramatically reduce per-frame overhead for projectile-heavy games (shmups, Metal Slug-style games).

### 3.7 Camera Zoom Range Is Limited

```c
#define NG_CAM_ZOOM_100 16
#define NG_CAM_ZOOM_50  8
```

Zoom only goes from 100% down to 50%. No zoom-in is supported. The NeoGeo hardware can't scale sprites above 100%, so "zoom in" would mean rendering a viewport smaller than 320x224 and using hardware scaling to fill the screen. This is how some commercial games achieved close-up effects. The current implementation correctly prevents values above 16, but doesn't document why zoom-in is absent or whether it's architecturally possible.

---

## 4. Robustness & Safety

### 4.1 No Bounds Checking on Handle Arrays

Every subsystem uses the pattern:
```c
if (handle < 0 || handle >= NG_TERRAIN_MAX) return;
```

This is good, but `NGTerrainHandle` is `s8`, and `NG_TERRAIN_MAX` is 4. An attacker or a bug could pass handle values that are technically in range but correspond to inactive slots. The code does check `tm->active`, which mitigates this. However, the `NGActorHandle` is also `s8` with `NG_ACTOR_MAX` presumably being some value -- if it's larger than 127, the signed handle type silently wraps. Verify that all `*_MAX` constants fit in their handle type's positive range.

### 4.2 Arena Allocator Returns NULL on OOM With No Diagnostic

```c
void *NGArenaAlloc(NGArena *arena, u32 size) {
    u8 *aligned = (u8 *)(((u32)arena->current + 3) & ~3);
    u8 *next = aligned + size;
    if (next > arena->end) { return 0; }
    // ...
}
```

When an arena runs out of memory, it silently returns NULL. Callers in the graphic system check for NULL returns, but callers using `NG_ARENA_ALLOC` macros may not. On a system with 64KB RAM, OOM is a real possibility. A debug-build hook (e.g., a function pointer called on OOM) would help developers catch allocation failures during development.

### 4.3 Division by Zero in Camera Shake

```c
camera.shake.offset_x =
    (s8)((shake_random() % (current_intensity * 2 + 1)) - current_intensity);
```

If `current_intensity` is 0 (which happens when `camera.shake.timer` is 0 but the code enters the block anyway due to the `> 0` check), the modulo is `% 1`, which is always 0. This is safe but wasteful. More concerning: the `shake_random()` function uses a 16-bit LCG with the multiplier `1103515245`, which overflows `u16`. The truncation is intentional (it's a standard LCG), but the period is only 2^16, which means shake patterns repeat every ~18 minutes of continuous shaking. Acceptable for game use.

### 4.4 Lighting Palette Backup Limit

```c
#define LIGHTING_MAX_BACKUP_PALETTES 32
```

The system can only back up 32 of the 256 possible palettes. If a game uses more than 32 palettes with lighting effects active, palettes beyond the 32nd are silently not backed up and won't be restored when lighting layers are removed. The backup loop silently stops when full:

```c
if (g_lighting.backup_count < LIGHTING_MAX_BACKUP_PALETTES) {
    // backup
}
```

This should at minimum log a warning in debug builds. In practice, NeoGeo games rarely use more than 20-30 palettes for sprites, so 32 is usually sufficient, but a game with many unique enemy palettes could hit this limit.

---

## 5. Build System & Tooling

### Strengths

- **Recursive Make with dependency ordering** -- `make progear` automatically builds core and hal first.
- **Format and lint targets** at every level -- `clang-format` and `cppcheck` integration is professional.
- **Multiple output formats** -- MAME testing, NeoSD `.neo` files, ROM zip packages.
- **The `hal.mk` and `progear.mk` includes** let game projects reuse build configuration without copy-pasting.

### Concerns

- **No dependency tracking between source and headers.** The Makefiles use a blanket rule where all objects depend on all headers (`$(C_OBJECTS): $(H_SOURCES)`). This means changing any header rebuilds everything. For a small project this is fine, but as games grow, automatic dependency generation (`-MMD -MP` flags) would speed up incremental builds.
- **No unit test infrastructure.** The SDK has zero tests. Fixed-point math, arena allocation, collision detection, and palette manipulation are all testable on the host machine (they don't require hardware). A host-compiled test suite using a standard C test framework would catch regressions and validate the math.
- **The asset pipeline has no caching.** Running `progear_assets.py` regenerates all assets from scratch every time. For a large game with hundreds of sprites and audio samples, this becomes a bottleneck. Timestamp-based or content-hash-based caching would help.

---

## 6. Documentation & Developer Experience

### Strengths

- **Doxygen comments are thorough and consistent.** Every public function has parameter docs, return value docs, and usage context. The `@section` blocks in headers like `camera.h` and `terrain.h` provide conceptual overviews.
- **The `demos/template` project is a genuine quick-start.** A developer can copy it, change `assets.yaml`, and have a running game in minutes.
- **CLAUDE.md is comprehensive.** The project overview, build commands, and architectural summary are accurate and well-organized.

### Concerns

- **No API migration guide or changelog.** As the SDK evolves, breaking API changes will be frustrating without documentation of what changed and why.
- **The `hal-template` demo is under-documented.** It demonstrates HAL-only usage but doesn't explain when or why a developer would choose HAL over ProGear.
- **Error handling conventions are undocumented.** Some functions return sentinel values (NG_ACTOR_INVALID), some silently return, some write to output parameters. A "conventions" section in the documentation would help.

---

## 7. Recommendations (Priority-Ordered)

### Critical (Blocks Real Game Development)

1. **Implement `NGTerrainSetTile` / `NGTerrainSetCollision`** -- Copy terrain data to RAM on creation (from state arena), then allow modification. Destructible terrain, collectibles, and doors are standard platformer features.
2. **Add sprite count queries** -- `NGGraphicGetAvailableSprites()` so developers can manage their sprite budget. Running out of the 381 hardware sprites is a constant concern.

### Important (Performance & Correctness)

3. **Optimize `memcpy`/`memset` for word/long alignment** -- Lighting palette operations (up to 1KB per frame) will benefit significantly.
4. **Document and mitigate `FIX_DIV` cost** -- Provide reciprocal lookup table or fast approximation alternatives.
5. **Fix `NGSin(ANGLE_90)` precision** -- Table entry at index 64 should produce exactly `FIX_ONE`.
6. **Evaluate SCB1 write timing** -- Profile on real hardware to determine if VBlank-only writes are necessary.

### Valuable (Quality of Life)

7. **Add host-compilable unit tests** -- Start with math, arena, and collision modules.
8. **Provide an object pooling utility** -- Pre-allocated actor pools for bullets/particles.
9. **Add automatic dependency generation** in Makefiles (`-MMD -MP`).
10. **Add debug-build OOM hook** for arena allocator.
11. **Document the internal call graph** in `sdk_internal.h`.
12. **Add asset pipeline caching** based on content hashes.

---

## 8. Conclusion

ProGearSDK is a genuinely useful SDK for NeoGeo development. The architecture is sound, the hardware abstraction is correct, and the asset pipeline bridges the gap between modern development tools and 1990s hardware. The game-logic layer already covers the most common needs well -- actor flipping, animation completion queries, physics collision callbacks, and terrain collision resolution are all present and functional. The remaining gaps are in runtime terrain modification, sprite budget visibility, and object pooling for bullet-heavy games.

The low-level m68k code shows real understanding of the hardware but has room for optimization in critical paths (memcpy, FIX_DIV, palette operations). The build system is solid for the project's current size but should grow toward automated testing and incremental asset building.

The SDK is at the stage where building one or two complete small games with it would surface the remaining friction points faster than any code review. The demo framework is a good start, but a complete game (even a simple one like Pong or Breakout with full game flow: title screen, gameplay, game over, high scores) would exercise the systems that the current demos skip.
