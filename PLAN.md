# Plan: Simplify ProGearSDK Library Interfaces

## Guiding Principles

1. Reduce surface area by removing redundant/duplicate definitions
2. Improve consistency across naming, patterns, and conventions
3. Generalize where two specialized functions can become one
4. Never introduce performance regressions on the 68000 target
5. Never break compile compatibility in one atomic step; provide migration path where needed

---

## Phase 1: Core Library (`core/include/`)

### 1A. Remove `FIX_FROM_FLOAT` macro (ng_math.h)
- **Problem**: `FIX_FROM_FLOAT(x)` and `FIX(x)` have identical implementations `((fixed)((x) * FIX_ONE))`. Two names for the same operation adds cognitive load.
- **Change**: Remove `FIX_FROM_FLOAT`. Add a doc comment to `FIX()` noting it works for compile-time float literals (e.g., `FIX(2)` and `FIX(0.5)` both work).
- **Risk**: Low. Grep for usage, update any call sites.
- **Performance**: None.

### 1B. Standardize header guards (all core headers)
- **Problem**: Core headers use `_NG_*_H_` with leading underscore, which is technically reserved by the C standard (identifiers starting with `_` followed by uppercase).
- **Change**: Rename to `NG_*_H` pattern (e.g., `_NG_MATH_H_` → `NG_MATH_H`). Apply same convention to HAL and ProGear headers for consistency.
- **Risk**: None. Header guards are internal.
- **Performance**: None.

---

## Phase 2: HAL Library (`hal/include/`)

### 2A. Remove `NG_RGB5` alias (ng_color.h)
- **Problem**: `NG_RGB5(r,g,b)` is byte-for-byte identical to `NG_RGB(r,g,b)`. Having both creates ambiguity about which to use.
- **Change**: Remove the `NG_RGB5` macro body. Replace with `#define NG_RGB5(r,g,b) NG_RGB(r,g,b)` as a deprecated redirect. In a follow-up, remove entirely.
- **Risk**: Low. Grep for `NG_RGB5` usage and verify callers work with `NG_RGB`.
- **Performance**: None (compile-time only).

### 2B. Consolidate color brightness functions (ng_color.h)
- **Problem**: Three overlapping functions:
  - `NGColorDarken(c, amount)` — darken toward black (0-31)
  - `NGColorLighten(c, amount)` — lighten toward white (0-31)
  - `NGColorAdjustBrightness(c, amount)` — adjust brightness (-31 to +31)

  `NGColorAdjustBrightness` already subsumes both Darken (negative) and Lighten (positive). Three functions for one operation is unnecessary surface area.
- **Change**: Remove `NGColorDarken` and `NGColorLighten`. Document `NGColorAdjustBrightness` as the single brightness adjustment function with signed amount.
- **Risk**: Low-medium. Update any internal callers.
- **Performance**: None. Same underlying computation.

### 2C. Consolidate palette fade functions (ng_palette.h)
- **Problem**: Three fade functions:
  - `NGPalFadeToBlack(palette, amount)` — special case of fade to color
  - `NGPalFadeToWhite(palette, amount)` — special case of fade to color
  - `NGPalFadeToColor(palette, target, amount)` — general form

  The first two are just `NGPalFadeToColor(pal, NG_COLOR_BLACK, amt)` and `NGPalFadeToColor(pal, NG_COLOR_WHITE, amt)`.
- **Change**: Remove `NGPalFadeToBlack` and `NGPalFadeToWhite`. Callers use `NGPalFadeToColor` directly with `NG_COLOR_BLACK` or `NG_COLOR_WHITE`. This is equally readable and reduces surface area.
- **Risk**: Low. The generalized function already exists.
- **Performance**: None. Same underlying call.

### 2D. Consolidate palette gradient functions (ng_palette.h)
- **Problem**: Three gradient functions:
  - `NGPalGradient(palette, start_idx, end_idx, start_color, end_color)` — general form
  - `NGPalGradientToBlack(palette, start_idx, end_idx, color)` — special case
  - `NGPalGradientToWhite(palette, start_idx, end_idx, color)` — special case

  Same pattern as fade: specialized versions are just the general form with a fixed endpoint.
- **Change**: Remove `NGPalGradientToBlack` and `NGPalGradientToWhite`. Callers use `NGPalGradient(pal, start, end, color, NG_COLOR_BLACK)`.
- **Risk**: Low.
- **Performance**: None.

---

## Phase 3: ProGear Library (`progear/include/`)

### 3A. Extract shared collision constants (scene.h, terrain.h)
- **Problem**: `NG_COLL_NONE/LEFT/RIGHT/TOP/BOTTOM` and `NG_TILE_SOLID/PLATFORM/SLOPE_L/SLOPE_R/HAZARD/TRIGGER/LADDER` are defined identically in both `scene.h` and `terrain.h`. This is a maintenance hazard — if one changes, the other must be updated manually.
- **Change**: Create `collision.h` in `progear/include/` containing these shared definitions. Both `scene.h` and `terrain.h` include it instead of defining their own copies.
- **Risk**: None. Pure refactor, no behavioral change.
- **Performance**: None.

### 3B. Unify actor position query (actor.h)
- **Problem**: Actor position requires two separate calls: `NGActorGetX(actor)` and `NGActorGetY(actor)`. The physics system already has `NGPhysBodyGetPos()` returning `NGVec2`. Inconsistent convenience level between subsystems.
- **Change**: Add `NGVec2 NGActorGetPos(NGActorHandle actor)` that returns both coordinates in one call. Keep `GetX`/`GetY` as convenience for cases where only one axis is needed.
- **Risk**: None. Additive API.
- **Performance**: Neutral. Returns struct by value (two 32-bit fields, fits in two registers on 68000).

### 3C. Standardize header guards across ProGear (all progear headers)
- **Problem**: Inconsistent header guard styles:
  - `_CAMERA_H_`, `_PHYSICS_H_`, `_TERRAIN_H_`, `_SCENE_H_` (leading underscore)
  - `LIGHTING_H`, `SPRING_H`, `UI_H`, `ENGINE_H` (no underscore)
  - End comments mix `//` and `/* */` styles
- **Change**: Standardize all to `NG_[MODULE]_H` pattern (e.g., `NG_CAMERA_H`, `NG_LIGHTING_H`). Use `/* */` end comments consistently.
- **Risk**: None. Internal convention change.
- **Performance**: None.

### 3D. Simplify camera zoom getters (camera.h)
- **Problem**: Two zoom getters that return the same information in different types:
  - `NGCameraGetZoom()` → `u8`
  - `NGCameraGetZoomFixed()` → `fixed`
  - Plus `NGCameraGetTargetZoom()` → `u8`

  The u8 variant truncates during transitions, which is exactly when you'd want the fixed-point value. Having both creates ambiguity about which to use.
- **Change**: Remove `NGCameraGetZoomFixed()`. Rename the current behavior so `NGCameraGetZoom()` returns `u8` (the discrete zoom level, which is what the constants like `NG_CAM_ZOOM_100` are). If needed, internal code that wants the interpolated value uses internal state directly.
- **Risk**: Low. Check demo usage — demos only use `NGCameraGetTargetZoom()` for comparison against constants.
- **Performance**: None.

### 3E. Simplify spring initialization (spring.h)
- **Problem**: Two initialization functions per spring type:
  - `NGSpringInit(spring, initial)` — uses hardcoded defaults for stiffness/damping
  - `NGSpringInitEx(spring, initial, stiffness, damping)` — full control
  - Same for 2D: `NGSpring2DInit` and `NGSpring2DInitEx`

  The `Init`/`InitEx` split is a common anti-pattern. The "simple" version just hides two parameters.
- **Change**: Remove `NGSpringInit` and `NGSpring2DInit`. Rename `NGSpringInitEx` → `NGSpringInit` and `NGSpring2DInitEx` → `NGSpring2DInit`. Callers that used the old simple form pass `NG_SPRING_SMOOTH_STIFFNESS, NG_SPRING_SMOOTH_DAMPING` (or whichever preset was the default) explicitly. This makes the stiffness/damping choice visible and intentional.
- **Risk**: Low-medium. Must update all callers.
- **Performance**: None.

### 3F. Unify lighting layer handle type (lighting.h)
- **Problem**: Lighting uses bare `u8` with `NG_LIGHTING_INVALID_HANDLE (0xFF)`, while every other system uses a dedicated typedef (e.g., `NGActorHandle`, `NGBackdropHandle`, `NGTerrainHandle`). Inconsistent handle type conventions.
- **Change**: The typedef `NGLightingLayerHandle` already exists but is defined as bare `u8`. This is acceptable; the real issue is the inconsistent invalid sentinel naming. Change from `NG_LIGHTING_INVALID_HANDLE` to `NG_LIGHTING_INVALID` to match the pattern used by `NG_ACTOR_INVALID`, `NG_BACKDROP_INVALID`, `NG_TERRAIN_INVALID`.
- **Risk**: Low. Rename + grep.
- **Performance**: None.

---

## Phase 4: Cross-Cutting Consistency

### 4A. Standardize "System Init" internal function visibility (graphic.h, others)
- **Problem**: `NGGraphicSystemInit()`, `NGGraphicSystemDraw()`, `NGGraphicSystemReset()` are in the public header but documented as "Internal — called by NGEngineInit()/NGSceneDraw()". Same for `NGArenaSystemInit()`. These pollute the public API with functions users should never call.
- **Change**: Move these declarations to internal headers (e.g., `progear/src/graphic_internal.h`) so they are not visible to SDK users. They remain callable by engine.c and scene.c internally.
- **Risk**: Low. Only affects internal callers which we control.
- **Performance**: None.

### 4B. Remove `NGCameraGetShrink()` from public API (camera.h)
- **Problem**: `NGCameraGetShrink()` returns a raw SCB2 hardware value. This is an implementation detail of NeoGeo sprite hardware that leaks through the ProGear abstraction layer. No demo uses it. Anyone writing at the ProGear level should never need raw SCB2 values.
- **Change**: Move to internal header. HAL-level code that needs it can access it there.
- **Risk**: Low. No demo usage found.
- **Performance**: None.

### 4C. Remove `NGCameraGetRenderX/Y()` from public API (camera.h)
- **Problem**: These "render position" getters include shake offsets. They exist for internal rendering code, not for game logic. Game code should use `NGCameraGetX/Y()` for the logical position, and the renderer handles shake internally.
- **Change**: Move to internal header.
- **Risk**: Low. Verify no demo usage.
- **Performance**: None.

---

## Summary of Surface Area Reduction

| Change | Functions/Macros Removed | Functions Added | Net |
|--------|--------------------------|-----------------|-----|
| 1A. Remove FIX_FROM_FLOAT | -1 macro | 0 | -1 |
| 2A. Remove NG_RGB5 | -1 macro | 0 | -1 |
| 2B. Consolidate brightness | -2 functions | 0 | -2 |
| 2C. Consolidate palette fade | -2 functions | 0 | -2 |
| 2D. Consolidate palette gradient | -2 functions | 0 | -2 |
| 3A. Extract collision constants | 0 (moved) | 0 | 0 |
| 3B. Unify actor position | 0 | +1 function | +1 |
| 3D. Simplify zoom getters | -1 function | 0 | -1 |
| 3E. Simplify spring init | -2 functions | 0 | -2 |
| 3F. Unify handle naming | -1 macro | +1 macro | 0 |
| 4A. Hide system internals | -6 functions (from public) | 0 public | -6 |
| 4B. Hide GetShrink | -1 function (from public) | 0 | -1 |
| 4C. Hide render positions | -2 functions (from public) | 0 | -2 |
| **Total** | **-21** | **+2** | **-19** |

Net reduction of 19 public API symbols while adding 1 convenience function.

---

## Execution Order

1. **Phase 1** (Core) — smallest scope, establishes patterns
2. **Phase 3A** (collision extraction) — prerequisite for clean terrain/scene
3. **Phase 3C** (header guards) — mechanical, no logic changes
4. **Phase 2A-2D** (HAL consolidations) — function removals with clear replacements
5. **Phase 3B, 3D-3F** (ProGear refinements) — API shape changes
6. **Phase 4A-4C** (internalization) — move internal symbols out of public headers
7. Update demos to use simplified APIs
8. `make check` to verify everything compiles and passes

Each phase is a separate commit for clean bisectability.
