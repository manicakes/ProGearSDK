# NeoGeo Core Foundation Library

Foundational types, fixed-point math, and memory utilities for NeoGeo development.

The Core library provides platform-independent foundations used throughout the SDK. These modules have no hardware dependencies and can be used by both the HAL and SDK layers.

## Building

```bash
make              # Build libneogeo_core.a
make clean        # Remove build artifacts
make format       # Auto-format source files
make lint         # Run static analysis
```

Output: `build/libneogeo_core.a`

## Usage

Include the master header to access all Core functionality:

```c
#include <neogeo_core.h>
```

Or include individual modules as needed:

```c
#include <ng_types.h>
#include <ng_math.h>
```

## Modules

### ng_types.h - Base Types

Foundation types used throughout the codebase.

```c
u8, u16, u32     // Unsigned integers
s8, s16, s32     // Signed integers
vu8, vu16, vu32  // Volatile unsigned (for hardware registers)
```

### ng_math.h - Fixed-Point Math

The 68000 has no FPU, so all math uses fixed-point representation.

```c
// Types
fixed            // 16.16 fixed-point (32-bit)
fixed16          // 8.8 fixed-point (16-bit)
angle_t          // 0-255 = 0-360 degrees

// Conversion
FIX(n)           // Integer to fixed (compile-time)
FIX_INT(f)       // Fixed to integer (truncate)
FIX_FROM_FLOAT(f) // Float to fixed (compile-time only)

// Arithmetic
FIX_MUL(a, b)    // Multiply two fixed values
FIX_DIV(a, b)    // Divide two fixed values

// Trigonometry (angle_t input, fixed output)
NGSin(angle)
NGCos(angle)

// Vectors
NGVec2           // 2D vector (fixed x, y)
NGVec2Add(a, b)
NGVec2Sub(a, b)
NGVec2Scale(v, s)
NGVec2Dot(a, b)
NGVec2Length(v)
NGVec2Normalize(v)
```

### ng_arena.h - Memory Allocation

Fast bump-pointer arena allocator with zero fragmentation.

```c
// Pre-defined arenas
ng_arena_persistent  // Lives for entire game
ng_arena_state       // Reset on scene change
ng_arena_frame       // Reset every frame

// Allocation
void *ptr = NGArenaAlloc(&ng_arena_state, size)
MyStruct *s = NG_ARENA_ALLOC(&ng_arena_state, MyStruct)
MyStruct *arr = NG_ARENA_ALLOC_ARRAY(&ng_arena_state, MyStruct, 10)

// Reset (free all allocations)
NGArenaReset(&ng_arena_state)

// Save/restore points
NGArenaMark mark = NGArenaSave(&ng_arena_state)
// ... allocate temporary data ...
NGArenaRestore(&ng_arena_state, mark)

// Query
NGArenaUsed(&ng_arena_state)
NGArenaRemaining(&ng_arena_state)
```

## See Also

- [HAL Documentation](../hal/) - Hardware abstraction layer
- [ProGear Documentation](../progear/) - High-level game engine
