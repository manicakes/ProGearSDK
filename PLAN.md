# ProGearSDK Implementation Plan

This plan addresses all findings from the SDK review, organized into phases with precise file locations, code changes, and testing strategies.

---

## Phase 1: Low-Level Performance (Core + HAL)

These changes are foundational -- they improve performance for every module that depends on them.

### 1.1 Optimize memcpy/memset/memmove for word/long alignment

**File:** `core/src/ng_string.c`

**Current state:** Byte-at-a-time loops. The 68000 can move longs (32-bit) 4x faster than bytes for aligned data.

**Implementation:**

```c
void *memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    /* If both pointers share the same alignment, use fast path */
    if (n >= 16 && (((u32)d ^ (u32)s) & 1) == 0) {
        /* Align to word boundary */
        if ((u32)d & 1) {
            *d++ = *s++;
            n--;
        }

        /* Align to long boundary */
        if ((u32)d & 2) {
            *(u16 *)d = *(const u16 *)s;
            d += 2; s += 2; n -= 2;
        }

        /* Copy longs (m68k MOVE.L is 4x faster than 4x MOVE.B) */
        u32 longs = n >> 2;
        u32 *dl = (u32 *)d;
        const u32 *sl = (const u32 *)s;
        while (longs--) {
            *dl++ = *sl++;
        }
        d = (u8 *)dl;
        s = (const u8 *)sl;
        n &= 3;
    }

    /* Copy remaining bytes */
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
```

Apply similar optimization to `memset` (fill with longs when aligned and n >= 16) and `memmove` (long-word backward copy).

**Impact:** Lighting palette backup/restore copies up to 1024 bytes per frame. This change makes those copies ~4x faster. GCC-generated struct copies also benefit.

**Testing:** Write a host-compiled test that verifies correctness for:
- All alignment combinations (src%4 x dest%4)
- n = 0, 1, 2, 3, 4, 15, 16, 17, 1024
- Overlapping regions (memmove)

### 1.2 Fix NGSin(ANGLE_90) precision

**File:** `core/src/ng_math.c`

**Current state:** `sin_table[64] = 32767`. After `<< 1`, this gives 65534 instead of FIX_ONE (65536).

**Change:** Set `sin_table[64] = 32768`.

The table is `const fixed` (s32), so 32768 fits. After `<< 1` it produces 65536 = FIX_ONE exactly. Verify that the corresponding cosine entry at index 0 (cos = sin[angle+64]) is unaffected (it correctly wraps to sin[0] = 0 for cos(90)).

Also verify `sin_table[192]` (sin(270) = -1). It should be -32768, which after `<< 1` gives -65536 = -FIX_ONE. If it's currently -32767, fix that too.

**Testing:** Host test asserting `NGSin(ANGLE_90) == FIX_ONE` and `NGSin(ANGLE_270) == -FIX_ONE`.

### 1.3 Add FIX_DIV documentation and fast alternatives

**Files:** `core/include/ng_math.h`, `core/src/ng_math.c`

**Changes:**

1. Add a warning comment to `FIX_DIV`:
   ```c
   /**
    * Fixed-point division. WARNING: Very slow on 68000 (~500+ cycles).
    * Uses 64-bit soft division. Avoid in per-frame hot paths.
    * Consider FIX_DIV_APPROX or reciprocal lookup for performance.
    */
   ```

2. Add `FIX_DIV_FAST` using Newton-Raphson reciprocal approximation:
   ```c
   /**
    * Fast approximate fixed-point division using reciprocal estimation.
    * Less precise than FIX_DIV but ~10x faster. Error < 0.5%.
    * Suitable for non-critical visual calculations (camera, effects).
    */
   static inline fixed FIX_DIV_FAST(fixed a, fixed b) {
       /* Uses initial estimate + one Newton-Raphson refinement */
       ...
   }
   ```

3. Optimize `NGVec2Normalize` to avoid two `FIX_DIV` calls. Replace with a single reciprocal computation:
   ```c
   NGVec2 NGVec2Normalize(NGVec2 v) {
       fixed len = NGVec2Length(v);
       if (len == 0) return (NGVec2){0, 0};
       fixed inv_len = FIX_DIV(FIX_ONE, len); /* One division instead of two */
       return (NGVec2){FIX_MUL(v.x, inv_len), FIX_MUL(v.y, inv_len)};
   }
   ```

**Testing:** Host test comparing `FIX_DIV` vs `FIX_DIV_FAST` across a range of inputs, verifying error stays < 1%.

### 1.4 Provide non-inline FIX_MUL variant

**Files:** `core/include/ng_math.h`, `core/src/ng_math.c`

Add a function-call variant alongside the inline version:

```c
/* In ng_math.h - inline version (default) */
static inline fixed FIX_MUL(fixed a, fixed b) { ... }

/* In ng_math.c - callable version for tight loops */
fixed FIX_MUL_FUNC(fixed a, fixed b) {
    /* Same implementation, but not inlined */
    ...
}
```

Document in `ng_math.h` when to prefer each:
- `FIX_MUL`: Use for isolated multiplies and small expressions
- `FIX_MUL_FUNC`: Use in tight loops (>8 iterations) where code size matters

---

## Phase 2: Terrain Runtime Modification

### 2.1 Implement NGTerrainSetTile and NGTerrainSetCollision

**Files:** `progear/src/terrain.c`, `progear/include/terrain.h`

**Current state:** Both functions are stubs. The terrain's `tile_data` and `collision_data` point to ROM (const data from the asset pipeline).

**Implementation strategy:** Copy tile_data and collision_data from ROM to RAM (using the state arena) at terrain creation time. Then set/modify the RAM copies.

**Changes to `terrain.c`:**

1. Add RAM copies to the `Terrain` struct:
   ```c
   typedef struct {
       const NGTerrainAsset *asset;
       u8 *ram_tile_data;      /* Mutable copy in state arena (NULL until modified) */
       u8 *ram_collision_data;  /* Mutable copy in state arena (NULL until modified) */
       // ... existing fields
   } Terrain;
   ```

2. Implement lazy-copy-on-write in `NGTerrainSetTile`:
   ```c
   void NGTerrainSetTile(NGTerrainHandle handle, u16 tile_x, u16 tile_y, u8 tile_index) {
       if (handle < 0 || handle >= NG_TERRAIN_MAX) return;
       Terrain *tm = &terrains[handle];
       if (!tm->active || !tm->asset) return;
       if (tile_x >= tm->asset->width_tiles || tile_y >= tm->asset->height_tiles) return;

       /* Lazy copy: allocate RAM copy on first modification */
       if (!tm->ram_tile_data) {
           u32 size = (u32)tm->asset->width_tiles * tm->asset->height_tiles;
           tm->ram_tile_data = (u8 *)NGArenaAlloc(&ng_arena_state, size);
           if (!tm->ram_tile_data) return; /* OOM */
           memcpy(tm->ram_tile_data, tm->asset->tile_data, size);
           /* Update graphic source to use RAM copy */
           NGGraphicSetSourceTilemap8(tm->graphic, tm->asset->base_tile,
               tm->ram_tile_data, tm->asset->width_tiles, tm->asset->height_tiles,
               tm->asset->tile_to_palette, tm->asset->default_palette);
       }

       u16 idx = (u16)(tile_y * tm->asset->width_tiles + tile_x);
       tm->ram_tile_data[idx] = tile_index;

       /* Invalidate graphic to force tile redraw */
       if (tm->graphic) {
           NGGraphicInvalidateSource(tm->graphic);
       }
   }
   ```

3. Similar implementation for `NGTerrainSetCollision` (lazy copy of collision_data).

4. Update `NGTerrainGetCollision` and `NGTerrainGetTileAt` to prefer RAM copies when available:
   ```c
   const u8 *tile_data = tm->ram_tile_data ? tm->ram_tile_data : tm->asset->tile_data;
   ```

5. Update `NGTerrainResolveAABB` similarly to use RAM copies.

**Memory cost:** A 40x30 tile terrain = 1200 bytes for tile data + 1200 bytes for collision = 2400 bytes from the 24KB state arena. Acceptable.

**Testing:** Modify the tilemap demo to break/remove a tile when player jumps on it. Verify the tile disappears visually and collision stops.

---

## Phase 3: Sprite Budget Visibility

### 3.1 Add sprite count queries

**Files:** `progear/include/graphic.h`, `progear/src/graphic.c`

**Add to `graphic.h`:**
```c
/** @name Diagnostics */
/** @{ */

/**
 * Get the number of entity sprites currently in use.
 * @return Sprite count (0 to UI_SPRITE_FIRST-1)
 */
u16 NGGraphicGetEntitySpriteCount(void);

/**
 * Get the number of UI sprites currently in use.
 * @return Sprite count (0 to UI_SPRITE_POOL_SIZE)
 */
u16 NGGraphicGetUISpriteCount(void);

/**
 * Get total sprites available for entities.
 * @return Maximum entity sprites (constant)
 */
u16 NGGraphicGetEntitySpriteMax(void);

/** @} */
```

**Implementation in `graphic.c`:**

Add static variables that are updated during `NGGraphicSystemDraw`:
```c
static u16 last_entity_sprites_used = 0;
static u16 last_ui_sprites_used = 0;

void NGGraphicSystemDraw(void) {
    // ... existing code ...
    // At end:
    last_entity_sprites_used = entity_idx - HW_SPRITE_FIRST;
    last_ui_sprites_used = ui_idx - UI_SPRITE_FIRST;
}

u16 NGGraphicGetEntitySpriteCount(void) { return last_entity_sprites_used; }
u16 NGGraphicGetUISpriteCount(void) { return last_ui_sprites_used; }
u16 NGGraphicGetEntitySpriteMax(void) { return UI_SPRITE_FIRST - HW_SPRITE_FIRST; }
```

**Demo integration:** Add a debug display in the showcase that prints sprite counts on the fix layer:
```c
NGTextPrintf(1, 1, 0, "SPR: %d/%d", NGGraphicGetEntitySpriteCount(), NGGraphicGetEntitySpriteMax());
```

---

## Phase 4: Object Pooling

### 4.1 Add lightweight actor pool

**New files:** `progear/include/pool.h`, `progear/src/pool.c`

This is a thin layer over the actor system for pre-allocating and recycling actors.

**API design:**
```c
#define NG_POOL_MAX 8       /* Maximum pools */
#define NG_POOL_INVALID (-1)

typedef s8 NGPoolHandle;

/**
 * Create a pool of pre-allocated actors.
 * All actors share the same visual asset and are initially hidden.
 * @param asset Visual asset for all pool members
 * @param count Number of actors to pre-allocate (max 32)
 * @return Pool handle, or NG_POOL_INVALID if failed
 */
NGPoolHandle NGPoolCreate(const NGVisualAsset *asset, u8 count);

/**
 * Acquire an actor from the pool (activates and shows it).
 * @param pool Pool handle
 * @param x Scene X position (fixed-point)
 * @param y Scene Y position (fixed-point)
 * @param z Z-index
 * @return Actor handle, or NG_ACTOR_INVALID if pool is exhausted
 */
NGActorHandle NGPoolAcquire(NGPoolHandle pool, fixed x, fixed y, u8 z);

/**
 * Release an actor back to the pool (hides it).
 * @param pool Pool handle
 * @param actor Actor handle
 */
void NGPoolRelease(NGPoolHandle pool, NGActorHandle actor);

/**
 * Release all actors in the pool.
 * @param pool Pool handle
 */
void NGPoolReleaseAll(NGPoolHandle pool);

/**
 * Destroy the pool and all its actors.
 * @param pool Pool handle
 */
void NGPoolDestroy(NGPoolHandle pool);

/**
 * Get count of currently active (acquired) actors.
 * @param pool Pool handle
 * @return Active count
 */
u8 NGPoolActiveCount(NGPoolHandle pool);
```

**Implementation:**
```c
typedef struct {
    NGActorHandle actors[32];
    u8 active[32];  /* 1 = acquired, 0 = available */
    u8 count;
    u8 active_count;
    u8 used;        /* Pool slot in use */
} Pool;

static Pool pools[NG_POOL_MAX];
```

- `NGPoolCreate`: Calls `NGActorCreate` for each slot, stores handles, sets all actors invisible.
- `NGPoolAcquire`: Finds first non-active slot, calls `NGActorAddToScene`, marks active.
- `NGPoolRelease`: Calls `NGActorRemoveFromScene`, marks inactive.
- No VRAM/graphic creation/destruction overhead on acquire/release -- just show/hide.

**Makefile changes:** Add `pool.c` to the source list in `progear/Makefile`.

---

## Phase 5: Build System Improvements

### 5.1 Automatic dependency generation

**Files:** All Makefiles (`core/Makefile`, `hal/Makefile`, `progear/Makefile`, `demos/*/Makefile`)

**Change:** Replace the blanket header dependency rule with GCC-generated `.d` files.

In each Makefile, add:
```makefile
CFLAGS += -MMD -MP

# Include generated dependency files (silently ignore if missing)
-include $(C_OBJECTS:.o=.d)
```

Remove the existing blanket rule:
```makefile
# Remove this line:
$(C_OBJECTS): $(H_SOURCES)
```

The `-MMD` flag generates `.d` files alongside `.o` files, listing the exact headers each source file includes. The `-MP` flag adds phony targets for headers, preventing errors if a header is deleted.

**Clean target:** Add `*.d` to clean:
```makefile
clean:
    rm -rf build/ *.d
```

### 5.2 Add host-compiled unit test infrastructure

**New directory:** `tests/`

**Files:**
- `tests/Makefile` -- Compiles with host GCC (not m68k cross-compiler)
- `tests/test_math.c` -- Fixed-point math tests
- `tests/test_arena.c` -- Arena allocator tests
- `tests/test_main.c` -- Test runner

**Test framework:** Use a minimal assertion macro (no external dependencies):
```c
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        test_failures++; \
    } else { \
        test_passes++; \
    } \
} while(0)
```

**test_math.c test cases:**
- `FIX_MUL(FIX(3), FIX(4)) == FIX(12)`
- `FIX_MUL(FIX(-3), FIX(4)) == FIX(-12)`
- `FIX_MUL(FIX_HALF, FIX_HALF) == FIX_ONE/4`
- `FIX_DIV(FIX(10), FIX(2)) == FIX(5)`
- `NGSin(ANGLE_0) == 0`
- `NGSin(ANGLE_90) == FIX_ONE`
- `NGSin(ANGLE_180) == 0`
- `NGSin(ANGLE_270) == -FIX_ONE`
- `NGCos(ANGLE_0) == FIX_ONE`
- `NGVec2Length of (3,4) == FIX(5)`
- `NGVec2Normalize produces unit vector`

**test_arena.c test cases:**
- Allocation returns aligned pointer
- Sequential allocations don't overlap
- Reset restores all memory
- OOM returns NULL
- Save/restore preserves state

**Top-level Makefile:** Add `test` target:
```makefile
test:
    $(MAKE) -C tests
```

**Compile strategy:** The test Makefile compiles `core/src/ng_math.c` and `core/src/ng_arena.c` with the **host** compiler, using `-I core/include`. This works because these modules have no hardware dependencies (they use only `ng_types.h` which is just `typedef`s over `stdint.h`).

### 5.3 Add `check` target to top-level Makefile

The top-level `make check` should run format-check, lint, and tests:
```makefile
check: format-check lint test
```

---

## Phase 6: Documentation & Robustness

### 6.1 Document sdk_internal.h call graph

**File:** `progear/src/sdk_internal.h`

Add a comment block at the top documenting the system call flow:

```c
/*
 * Internal SDK subsystem communication.
 *
 * Call graph during frame:
 *
 *   NGEngineFrameStart()
 *     -> NGWaitVBlank()
 *     -> NGMenuDraw() [if menu active, fix layer writes]
 *     -> NGArenaReset(&ng_arena_frame)
 *     -> NGInputUpdate()
 *
 *   [Game logic runs here]
 *
 *   NGEngineFrameEnd()
 *     -> NGLightingUpdate()
 *         -> _NGActorCollectPalettes()
 *         -> _NGBackdropCollectPalettes()
 *         -> _NGTerrainCollectPalettes()
 *     -> NGSceneUpdate()
 *         -> NGCameraUpdate()
 *         -> _NGActorSystemUpdate() [animation tick]
 *         -> _NGBackdropSystemUpdate()
 *     -> NGSceneDraw()
 *         -> _NGBackdropSyncGraphics()  [parallax + camera -> screen pos]
 *         -> _NGTerrainSyncGraphics()   [camera viewport -> tile offset]
 *         -> _NGActorSyncGraphics()     [world pos -> screen pos via camera]
 *         -> NGGraphicSystemDraw()      [sort, allocate sprites, flush VRAM]
 *
 * Initialization order (NGEngineInit):
 *   NGArenaSystemInit -> NGPalInitDefault -> NGFixClearAll ->
 *   NGSceneInit (-> NGGraphicSystemInit, _NGActorSystemInit,
 *                   _NGBackdropSystemInit, _NGTerrainSystemInit) ->
 *   NGCameraInit -> NGInputInit -> NGAudioInit -> NGLightingInit ->
 *   NGPalInitAssets [weak] -> NGPalSetBackdrop
 */
```

### 6.2 Add arena OOM debug hook

**Files:** `core/include/ng_arena.h`, `core/src/ng_arena.c`

Add an optional debug callback:

```c
/* In ng_arena.h */
#ifdef NG_DEBUG
/** Set callback invoked on out-of-memory. Useful during development. */
void NGArenaSetOOMHandler(void (*handler)(NGArena *arena, u32 requested_size));
#endif

/* In ng_arena.c */
#ifdef NG_DEBUG
static void (*g_oom_handler)(NGArena *, u32) = NULL;

void NGArenaSetOOMHandler(void (*handler)(NGArena *arena, u32 requested_size)) {
    g_oom_handler = handler;
}
#endif

void *NGArenaAlloc(NGArena *arena, u32 size) {
    u8 *aligned = (u8 *)(((u32)arena->current + 3) & ~3);
    u8 *next = aligned + size;
    if (next > arena->end) {
#ifdef NG_DEBUG
        if (g_oom_handler) g_oom_handler(arena, size);
#endif
        return 0;
    }
    arena->current = next;
    return aligned;
}
```

The `NG_DEBUG` flag can be set in game Makefiles for development builds. Release builds have zero overhead.

### 6.3 Document single-scene/single-camera constraint

**File:** `CLAUDE.md`

Add to the Architecture section:

```
### Design Constraints (Intentional)

- **Single scene, single camera**: The SDK uses module-level singletons for
  Camera, Scene, Lighting, and Input. This is a deliberate trade-off for
  simplicity -- NeoGeo games rarely need split-screen or multiple concurrent
  scenes. Screen-space actors (NGActorSetScreenSpace) can overlay UI without
  a separate scene. If split-screen is needed, the HAL layer can be used
  directly.
```

### 6.4 Document error handling conventions

**File:** `CLAUDE.md`

Add:
```
### Error Handling Conventions

- **Creation functions** return a sentinel value on failure:
  - `NG_ACTOR_INVALID` (-1), `NG_TERRAIN_INVALID` (-1), `NG_BACKDROP_INVALID` (-1)
  - `NULL` for pointer-returning functions (NGGraphicCreate, NGPhysWorldCreate)
- **Property setters** silently ignore invalid handles (bounds-checked, then active-checked)
- **Arena allocation** returns NULL on OOM (use NG_DEBUG hook to catch during dev)
```

---

## Phase 7: Additional m68k Optimizations

### 7.1 Add comment to FIX_MUL explaining sign handling

**File:** `core/include/ng_math.h`

Add a comment block above `FIX_MUL`:
```c
/**
 * Fixed-point 16.16 multiply optimized for 68000.
 *
 * Decomposes the 32x32 multiply into four 16x16 multiplies that the
 * 68000 can execute natively (MULS/MULU). The low halves (a_lo, b_lo)
 * are treated as unsigned because they represent the magnitude of the
 * fractional/low portion of a two's complement number.
 *
 * Correctness requires 32-bit int (true for m68k-elf-gcc).
 */
```

### 7.2 Widen VBlank register save

**File:** `hal/startup/crt0.s`

Change:
```asm
_vblank:
    movem.l %d0-%d1/%a0-%a1, -(%sp)
```
To:
```asm
_vblank:
    movem.l %d0-%d7/%a0-%a6, -(%sp)
```

And the corresponding restore at the end of the handler. Cost: ~30 extra cycles per VBlank (negligible at 60Hz). Benefit: Correct behavior if anyone writes an assembly VBlank handler or uses `__attribute__((interrupt))`.

---

## Implementation Order

The phases should be implemented in this order, with each phase being a separate commit:

| Phase | Description | Estimated Scope | Dependencies |
|-------|-------------|-----------------|--------------|
| 1.2 | Sin table fix | 1 line | None |
| 1.1 | Optimized memcpy/memset | ~80 lines | None |
| 7.2 | VBlank register save | 2 lines | None |
| 7.1 | FIX_MUL documentation | Comment only | None |
| 1.3 | FIX_DIV docs + fast alternative | ~40 lines | None |
| 1.4 | Non-inline FIX_MUL variant | ~20 lines | None |
| 6.1 | sdk_internal.h call graph doc | Comment only | None |
| 6.3-6.4 | CLAUDE.md documentation | Docs only | None |
| 3.1 | Sprite count queries | ~30 lines | None |
| 2.1 | Terrain runtime modification | ~60 lines | None |
| 4.1 | Object pooling | ~150 lines (new) | None |
| 5.1 | Makefile dependency gen | Build system | None |
| 5.2 | Host unit tests | ~200 lines (new) | Phase 1.2 |
| 6.2 | Arena OOM debug hook | ~15 lines | None |

**Total estimated new/modified code:** ~600 lines across ~15 files, plus ~200 lines of new test code.

---

## Files Modified (Summary)

### Modified:
- `core/src/ng_string.c` -- Optimized memcpy/memset/memmove
- `core/src/ng_math.c` -- Sin table fix, FIX_MUL_FUNC, optimized NGVec2Normalize
- `core/include/ng_math.h` -- FIX_DIV docs, FIX_DIV_FAST, FIX_MUL_FUNC declaration, FIX_MUL comment
- `core/include/ng_arena.h` -- Debug OOM hook
- `core/src/ng_arena.c` -- Debug OOM hook implementation
- `hal/startup/crt0.s` -- Wider register save in VBlank
- `progear/src/terrain.c` -- SetTile/SetCollision implementation
- `progear/src/graphic.c` -- Sprite count tracking
- `progear/include/graphic.h` -- Sprite count query API
- `progear/src/sdk_internal.h` -- Call graph documentation
- `progear/Makefile` -- Add pool.c
- `core/Makefile`, `hal/Makefile`, `progear/Makefile`, `demos/*/Makefile` -- -MMD -MP
- `Makefile` -- Add test target
- `CLAUDE.md` -- Documentation additions

### New:
- `progear/include/pool.h` -- Actor pool API
- `progear/src/pool.c` -- Actor pool implementation
- `tests/Makefile` -- Host test build
- `tests/test_main.c` -- Test runner
- `tests/test_math.c` -- Math tests
- `tests/test_arena.c` -- Arena tests
