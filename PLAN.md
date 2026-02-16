# ProGearSDK Elegance Refactoring Proposal

After a thorough review of every module across all three layers (Core, HAL, ProGear),
here are the proposed changes, organized by impact. Each change maintains or improves
performance characteristics on the 68000 target.

---

## 1. Delete Dead Code: `_NGSceneMarkRenderQueueDirty()`

**Files:** `progear/src/scene.c`, `progear/src/actor.c`, `progear/src/sdk_internal.h`

The function `_NGSceneMarkRenderQueueDirty()` in `scene.c:24-26` is a no-op stub:

```c
void _NGSceneMarkRenderQueueDirty(void) {
    (void)0;
}
```

It is called from `actor.c` in three places (lines 217, 236, 288) but does nothing.
The graphic system now manages render ordering internally via its own
`render_order_dirty` flag in `graphic.c:147`. This function and all its call sites
are dead code left over from a previous design.

**Change:** Remove the function body from `scene.c`, remove all call sites in
`actor.c`, and remove the declaration from `sdk_internal.h`.

**Impact:** Eliminates 3 function calls per affected code path (actor add/remove/z-change).
Small but measurable on a 12MHz CPU. More importantly, removes misleading code that
suggests scene-level render ordering exists when it doesn't.

---

## 2. Extract Zoom-to-Scale Conversion (Duplicated in 3 Modules)

**Files:** `progear/src/actor.c:102-103`, `progear/src/backdrop.c:80-81`,
`progear/src/terrain.c:62`

The conversion from camera zoom (8-16) to graphic scale (128-256) appears in three
places with slightly different formulations:

```c
// actor.c:103
scale = (u16)((zoom * NG_GRAPHIC_SCALE_ONE) >> 4);

// backdrop.c:81 (equivalent but different form)
scale = (u16)(zoom * 16);  /* zoom * 256 / 16 */

// terrain.c:62
scale = (u16)((zoom * NG_GRAPHIC_SCALE_ONE) >> 4);
```

**Change:** Add an inline function to `camera.h`:

```c
static inline u16 NGCameraZoomToScale(u8 zoom) {
    return (u16)((zoom * NG_GRAPHIC_SCALE_ONE) >> 4);
}
```

Replace all three call sites. This also makes the backdrop formulation consistent
with the other two (currently using `zoom * 16` which is algebraically equivalent
but obscures the intent).

**Impact:** Zero runtime cost (inline). Eliminates a subtle inconsistency between
modules and creates a single authoritative conversion.

---

## 3. Extract Palette Bitmask Helper (Duplicated in 3 Modules)

**Files:** `progear/src/actor.c:477`, `progear/src/backdrop.c:300`,
`progear/src/terrain.c:442`

The palette collection bitmask operation is copy-pasted identically:

```c
palette_mask[pal >> 3] |= (u8)(1 << (pal & 7));
```

**Change:** Add an inline helper to `sdk_internal.h`:

```c
static inline void _NGPaletteMaskSet(u8 *mask, u8 palette) {
    mask[palette >> 3] |= (u8)(1 << (palette & 7));
}
```

Replace all 4 occurrences (actor, backdrop, terrain default, terrain tile_to_palette).

**Impact:** Zero runtime cost. Makes the bitmask encoding self-documenting and
eliminates the risk of a typo in any one copy.

---

## 4. Extract Tile-Bounds Clamping Helper in Terrain

**File:** `progear/src/terrain.c`

The same 4-line tile bounds clamping sequence appears 4 times:

```c
if (left_tile < 0) left_tile = 0;
if (right_tile >= tm->asset->width_tiles) right_tile = (s16)(tm->asset->width_tiles - 1);
if (top_tile < 0) top_tile = 0;
if (bottom_tile >= tm->asset->height_tiles) bottom_tile = (s16)(tm->asset->height_tiles - 1);
```

Found at: lines 274-281, lines 316-323, lines 361-373 (horizontal pass), and the
same pattern again in `NGTerrainResolveAABB` vertical pass.

**Change:** Extract to a static inline helper:

```c
static inline void clamp_tile_bounds(const NGTerrainAsset *asset,
                                     s16 *left, s16 *right,
                                     s16 *top, s16 *bottom) {
    if (*left < 0) *left = 0;
    if (*right >= asset->width_tiles) *right = (s16)(asset->width_tiles - 1);
    if (*top < 0) *top = 0;
    if (*bottom >= asset->height_tiles) *bottom = (s16)(asset->height_tiles - 1);
}
```

**Impact:** ~16 lines removed. The compiler will inline this, so no function call
overhead on the 68000. Makes each collision function shorter and focused on its
actual logic rather than boilerplate bounds checking.

---

## 5. Move `str_equal()` from Actor to Core

**Files:** `progear/src/actor.c:35-41` -> `core/include/ng_string.h`

`actor.c` defines a private `str_equal()` for animation name lookup. This is a
general-purpose utility that belongs in the string module and could be useful
elsewhere (e.g., if terrain or lighting ever needs name-based lookup).

**Change:** Move to `core/include/ng_string.h` as a public inline function
`ng_str_equal()`, following the existing naming conventions. Remove the static
copy from `actor.c`.

```c
static inline u8 ng_str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}
```

**Impact:** Zero runtime cost (inline). Increases code reuse potential and follows
the pattern where Core provides foundational utilities.

---

## 6. Extract "Any Layers Active" Check in Lighting

**File:** `progear/src/lighting.c`

The exact same loop appears in two places (`NGLightingPop` at line 174 and
`NGLightingUpdate` at line 447):

```c
u8 any_active = 0;
for (u8 i = 0; i < NG_LIGHTING_MAX_LAYERS; i++) {
    if (g_lighting.layers[i].active) {
        any_active = 1;
        break;
    }
}
```

Additionally, `NGLightingIsActive()` at line 482 does the same thing.

**Change:** Extract a single `static u8 any_layer_active(void)` helper and use it
in all three places. This also makes `NGLightingIsActive()` a trivial delegation.

**Impact:** The compiler may or may not inline this across call sites, but even if
it doesn't, the loop is tiny (max 8 iterations). The readability improvement is
significant -- the post-layer-removal cleanup logic in both `Pop` and `Update`
becomes much clearer.

---

## 7. Use `NGCameraGetRenderX/Y()` in `NGCameraWorldToScreen()`

**File:** `progear/src/camera.c`

`NGCameraWorldToScreen()` (line 215) manually computes
`camera.x + FIX(camera.shake.offset_x)` while `NGCameraGetRenderX()` (line 278)
does exactly the same thing:

```c
// WorldToScreen (line 215-216):
fixed cam_render_x = camera.x + FIX(camera.shake.offset_x);
fixed cam_render_y = camera.y + FIX(camera.shake.offset_y);

// GetRenderX (line 278-280):
fixed NGCameraGetRenderX(void) {
    return camera.x + FIX(camera.shake.offset_x);
}
```

**Change:** Have `WorldToScreen` call `NGCameraGetRenderX()`/`NGCameraGetRenderY()`
instead of duplicating the computation.

**Impact:** The functions are in the same translation unit so the compiler can
inline them. This eliminates a maintenance hazard where one could be updated
without the other.

---

## 8. Integrate `NGLightingUpdatePrebakedFade()` into `NGLightingUpdate()`

**Files:** `progear/src/lighting.c`, `progear/src/engine.c`

`NGLightingUpdate()` is called every frame by `engine.c:54`, but
`NGLightingUpdatePrebakedFade()` (line 906) is *never* called by the engine.
Pre-baked preset fade animations (push/pop with fade_frames > 0) silently fail to
animate unless the user manually calls this function in their game loop.

This is almost certainly a bug. The engine should drive all lighting animations
automatically.

**Change:** Add a call to `NGLightingUpdatePrebakedFade()` at the end of
`NGLightingUpdate()`, after the layer animation processing. This is where it
logically belongs -- it's a lighting animation like any other.

```c
void NGLightingUpdate(void) {
    // ... existing layer update code ...

    /* Update pre-baked preset fade animation */
    NGLightingUpdatePrebakedFade();
}
```

**Impact:** Fixes a likely bug. Pre-baked lighting presets will now fade
automatically without requiring manual intervention. Zero overhead when no preset
fade is active (the function returns immediately on `!prebaked_fading`).

---

## 9. Remove Unused `friction` Field from Physics Bodies

**Files:** `progear/include/physics.h`, `progear/src/physics.c`

`NGBody` contains a `friction` field and `NGPhysBodySetFriction()` stores a value,
but friction is never applied during collision resolution. The field wastes 4 bytes
per body (128 bytes total across 32 bodies) on a memory-constrained platform.

**Change:** Remove the `friction` field from `NGBody`, remove
`NGPhysBodySetFriction()`, and remove the initialization in `alloc_body()`.

**Impact:** Saves 128 bytes of RAM. Eliminates a misleading API that suggests
friction works when it doesn't. If friction is ever needed, it should be
implemented properly rather than carrying dead weight.

---

## 10. Simplify Physics World to Static Singleton

**File:** `progear/src/physics.c`

The physics module uses a pool-based allocation pattern:

```c
#define MAX_WORLDS 1
static NGPhysWorld world_pool[MAX_WORLDS];
```

But `MAX_WORLDS` is 1 and there's no indication this will ever change (the NeoGeo
has a single game context). The pool pattern adds needless indirection:
`NGPhysWorldCreate()` searches a 1-element array, every function takes a world
pointer, etc.

**Change:** Replace with a simple static struct:

```c
static NGPhysWorld g_world;
```

Keep the existing API signature (`NGPhysWorldHandle`) for forwards compatibility,
but simplify the internals. `NGPhysWorldCreate()` just sets `g_world.active = 1`
and returns `&g_world`. Remove the pool search loop.

**Impact:** Saves a tiny amount of code size and eliminates one level of pointer
indirection in every physics call. The API surface doesn't change.

---

## 11. Add `NGSceneGetTerrain()` Accessor

**File:** `progear/src/scene.c`, `progear/include/scene.h`

Lines 83-185 of scene.c consist of 10 thin proxy methods that do nothing except
null-check `scene_terrain` and forward to `NGTerrain*()`:

```c
void NGSceneSetTerrainPos(fixed x, fixed y) {
    if (scene_terrain != NG_TERRAIN_INVALID) {
        NGTerrainSetPos(scene_terrain, x, y);
    }
}
```

This is ~100 lines of pure delegation with zero added logic.

**Change:** Rather than deleting these (which would break the public API), make
the terrain handle accessible so users *can* call the terrain API directly if they
prefer. Add:

```c
NGTerrainHandle NGSceneGetTerrain(void);
```

This lets advanced users bypass the proxy layer while keeping the convenience
methods for simple cases. Document that `NGSceneSetTerrain()` and
`NGSceneGetTerrain()` + direct terrain calls are equivalent.

**Impact:** No code removed (API stability), but users gain the option to
eliminate the double-dispatch overhead. The new function is 3 lines.

---

## 12. Remove Empty `_NGBackdropSystemUpdate()` Stub

**File:** `progear/src/backdrop.c:44`, `progear/src/scene.c:47`,
`progear/src/sdk_internal.h`

```c
void _NGBackdropSystemUpdate(void) {}
```

This empty function is called every frame by `NGSceneUpdate()`. Since backdrops
have no per-frame logic (they sync during draw), this is a wasted function call.

**Change:** Remove `_NGBackdropSystemUpdate()` from `backdrop.c`, remove its
declaration from `sdk_internal.h`, and remove the call from `scene.c:47`.

**Impact:** Removes one unnecessary function call per frame. Small but meaningful
on a 12MHz CPU where every cycle counts.

---

## Summary Table

| # | Change | Lines Removed | Lines Added | Risk |
|---|--------|:------------:|:-----------:|:----:|
| 1 | Delete `_NGSceneMarkRenderQueueDirty` | ~10 | 0 | None |
| 2 | Extract zoom-to-scale conversion | ~3 | ~5 | None |
| 3 | Extract palette bitmask helper | ~4 | ~5 | None |
| 4 | Extract tile-bounds clamping | ~16 | ~8 | None |
| 5 | Move `str_equal` to Core | ~7 | ~7 | None |
| 6 | Extract lighting active-check | ~12 | ~6 | None |
| 7 | Reuse `GetRenderX/Y` in camera | ~2 | ~2 | None |
| 8 | Integrate prebaked fade into Update | ~0 | ~3 | Low |
| 9 | Remove unused `friction` field | ~8 | 0 | Low |
| 10 | Simplify physics world to singleton | ~10 | ~5 | Low |
| 11 | Add `NGSceneGetTerrain()` accessor | 0 | ~6 | None |
| 12 | Remove empty `BackdropSystemUpdate` | ~4 | 0 | None |

**Total: ~76 lines removed, ~47 lines added (net -29 lines)**

All changes are safe for the 68000 target:
- No new divisions, multiplications, or memory allocations
- Inline helpers compile to identical machine code as the originals
- No behavioral changes except #8 (bug fix) and #9 (dead field removal)

## What I Explicitly Chose NOT to Change

- **Subsystem init/create/destroy patterns in actor/backdrop/terrain:** While
  structurally similar, they have enough domain-specific differences (parallax
  anchoring, collision data, animation state) that abstracting them into a shared
  "entity" base would add complexity without meaningfully reducing code. The C
  language also lacks good mechanisms for this without function pointers (which add
  overhead on the 68000).

- **Unrolled palette operations in `ng_palette.c`:** The 16-entry unrolling is
  intentional for the 68000 and correctly trades code size for speed.

- **Makefile duplication across demos:** While ROM generation rules are repeated,
  this is a build system concern separate from SDK code quality. The `.mk` include
  files already handle the important shared state.

- **`NGGraphic` struct complexity:** The graphic system is the
  performance-critical backbone of the engine. Its complexity (dirty tracking,
  cache, scroll state) is justified by the hardware constraints it manages.
