/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

// parallax.c - Parallax effect implementation

#include <parallax.h>
#include <camera.h>
#include <neogeo.h>

// === Internal Constants ===

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  224
#define TILE_SIZE      16

// SCB base addresses
#define SCB1_BASE  0x0000
#define SCB2_BASE  0x8000
#define SCB3_BASE  0x8200
#define SCB4_BASE  0x8400

// Maximum hardware sprites per parallax layer
#define MAX_COLUMNS_PER_PARALLAX  42

// === Internal Types ===

typedef struct {
    const NGVisualAsset *asset;
    u16 width, height;       // Display dimensions (0 = asset, 0xFFFF = infinite)
    fixed parallax_x;        // Horizontal movement rate (FIX_ONE = 1:1 with camera)
    fixed parallax_y;        // Vertical movement rate
    s16 viewport_x;          // X offset from camera viewport
    s16 viewport_y;          // Y offset from camera viewport
    fixed anchor_cam_x;      // Camera position when added (for parallax calc)
    fixed anchor_cam_y;
    u8 z;                    // Z-index for render order
    u8 palette;
    u8 visible;
    u8 in_scene;
    u8 active;

    // Hardware sprite allocation
    u16 hw_sprite_first;
    u8 hw_sprite_count;
    u8 tiles_loaded;         // Tiles loaded to VRAM?

    // Dirty tracking for optimization
    u8 last_zoom;            // Last zoom value (to detect changes)
    u16 last_scb3;           // Last SCB3 value written

    // X-position cycling state (ngdevkit approach)
    u16 leftmost;            // Absolute sprite index of leftmost visible sprite
    s16 scroll_offset;       // Fixed-point scroll accumulator (4.4 format)
    s16 last_scroll_px;      // Last scroll position in pixels for delta calculation

    // Finite width X-position caching
    s16 last_base_x;         // Last base X position for finite width parallax
} Parallax;

// Fixed-point for sub-pixel scrolling (4 fractional bits like ngdevkit uses 3)
#define SCROLL_FRAC_BITS  4
#define SCROLL_FIX(x)     ((x) << SCROLL_FRAC_BITS)
#define SCROLL_INT(x)     ((x) >> SCROLL_FRAC_BITS)

// === Private State ===

static Parallax parallax_layers[NG_PARALLAX_MAX];

// === Forward declaration for scene integration ===
extern void _NGSceneMarkRenderQueueDirty(void);

// === Internal Functions (called by scene.c) ===

void _NGParallaxSystemInit(void) {
    for (u8 i = 0; i < NG_PARALLAX_MAX; i++) {
        parallax_layers[i].active = 0;
        parallax_layers[i].in_scene = 0;
    }
}

void _NGParallaxSystemUpdate(void) {
    // Parallax effects don't need per-frame updates
    // (no animation support in this version)
}

u8 _NGParallaxIsInScene(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return 0;
    return parallax_layers[handle].active && parallax_layers[handle].in_scene;
}

u8 _NGParallaxGetZ(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return 0;
    return parallax_layers[handle].z;
}

// Draw a parallax layer
static void draw_parallax(Parallax *plx, u16 first_sprite) {
    if (!plx->visible || !plx->asset) return;

    const NGVisualAsset *asset = plx->asset;

    // Get current camera position
    fixed cam_x = NGCameraGetX();
    fixed cam_y = NGCameraGetY();

    // Calculate how much camera has moved since anchor
    fixed delta_x = cam_x - plx->anchor_cam_x;
    fixed delta_y = cam_y - plx->anchor_cam_y;

    // Apply parallax rate to camera movement
    fixed parallax_offset_x = FIX_MUL(delta_x, plx->parallax_x);
    fixed parallax_offset_y = FIX_MUL(delta_y, plx->parallax_y);

    // Final position: viewport offset minus parallax movement
    s16 base_y = plx->viewport_y - FIX_INT(parallax_offset_y);

    // Get display dimensions
    u16 disp_w = plx->width;
    if (disp_w == 0) disp_w = asset->width_pixels;

    // Check if infinite width
    u8 infinite_width = (plx->width == NG_PARALLAX_WIDTH_INFINITE);

    // Calculate tiles/columns needed
    u8 asset_cols = asset->width_tiles;
    u8 asset_rows = asset->height_tiles;

    u8 num_cols;
    if (infinite_width) {
        // For infinite scroll, need enough columns to cover screen + buffer for wrapping
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;  // +2 for wrap buffer
        num_cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (num_cols > MAX_COLUMNS_PER_PARALLAX) num_cols = MAX_COLUMNS_PER_PARALLAX;
    } else {
        // For finite width, calculate columns needed and tile if asset is smaller
        num_cols = (disp_w + TILE_SIZE - 1) / TILE_SIZE;
        if (num_cols > MAX_COLUMNS_PER_PARALLAX) num_cols = MAX_COLUMNS_PER_PARALLAX;
    }

    // For finite width with tiling, calculate rows to cover display height
    u16 disp_h = plx->height;
    if (disp_h == 0) disp_h = asset->height_pixels;

    u8 num_rows;
    if (!infinite_width && disp_h > asset->height_pixels) {
        // Finite mode with height larger than asset - need to tile vertically
        num_rows = (disp_h + TILE_SIZE - 1) / TILE_SIZE;
    } else {
        // Use asset height (infinite width or finite with matching height)
        num_rows = asset_rows;
    }
    if (num_rows > 32) num_rows = 32;

    // Get zoom and check if changed
    u8 zoom = NGCameraGetZoom();
    u8 zoom_changed = (zoom != plx->last_zoom);

    // === DETECT SPRITE REALLOCATION ===
    // When menu opens/closes, render queue changes and sprite indices shift
    // If our assigned sprites moved, we need to reinitialize
    if (plx->tiles_loaded && plx->hw_sprite_first != first_sprite) {
        plx->tiles_loaded = 0;  // Force reinit with new sprite range
    }

    // === INITIAL SETUP (ONCE) ===
    if (!plx->tiles_loaded) {
        // Load tiles for all sprites (SCB1)
        for (u8 col = 0; col < num_cols; col++) {
            u16 spr = first_sprite + col;

            NG_REG_VRAMADDR = SCB1_BASE + (spr * 64);
            NG_REG_VRAMMOD = 1;

            // Use modulo to tile smaller assets across wider display
            u8 asset_col = col % asset_cols;

            for (u8 row = 0; row < num_rows; row++) {
                // Also modulo the row for vertical tiling
                u8 asset_row = row % asset_rows;
                u16 tile_idx = asset->base_tile + (asset_col * asset_rows) + asset_row;
                NG_REG_VRAMDATA = tile_idx & 0xFFFF;
                u16 attr = ((u16)plx->palette << 8) | 0x01;
                NG_REG_VRAMDATA = attr;
            }

            // Clear remaining slots
            for (u8 row = num_rows; row < 32; row++) {
                NG_REG_VRAMDATA = 0;
                NG_REG_VRAMDATA = 0;
            }
        }

        // Initialize SCB2 shrink values
        u16 shrink = NGCameraGetShrink();
        NG_REG_VRAMADDR = SCB2_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = shrink;
        }

        // Initialize SCB4 X positions with zoom-adjusted spacing
        // Start one tile to the LEFT of screen edge to provide buffer for bidirectional scrolling
        s16 tile_w = (TILE_SIZE * zoom) >> 4;
        NG_REG_VRAMADDR = SCB4_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            s16 x = (col * tile_w) - tile_w;  // Offset by -1 tile
            NG_REG_VRAMDATA = (x & 0x1FF) << 7;
        }

        plx->hw_sprite_first = first_sprite;
        plx->hw_sprite_count = num_cols;
        plx->tiles_loaded = 1;
        plx->last_zoom = zoom;
        plx->last_scb3 = 0xFFFF;  // Force SCB3 write

        // Initialize scroll state for bidirectional scrolling
        plx->leftmost = first_sprite;
        s16 tile_width_zoomed = (TILE_SIZE * zoom) >> 4;
        // Start scroll_offset in the middle of valid range [0, tile_width * 2]
        // This allows immediate scrolling in either direction
        plx->scroll_offset = SCROLL_FIX(tile_width_zoomed);
        // IMPORTANT: Use current parallax offset, not 0!
        // Otherwise reopening menu causes huge position jump
        plx->last_scroll_px = FIX_INT(parallax_offset_x);
    }

    // === UPDATE SCB2 ONLY WHEN ZOOM CHANGES ===
    if (zoom_changed) {
        u16 shrink = NGCameraGetShrink();
        NG_REG_VRAMADDR = SCB2_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = shrink;
        }

        // For infinite-width parallax, reinitialize X positions with new zoom spacing
        if (infinite_width) {
            s16 tile_w = (TILE_SIZE * zoom) >> 4;
            NG_REG_VRAMADDR = SCB4_BASE + first_sprite;
            NG_REG_VRAMMOD = 1;
            for (u8 col = 0; col < num_cols; col++) {
                s16 x = (col * tile_w) - tile_w;  // Offset by -1 tile
                NG_REG_VRAMDATA = (x & 0x1FF) << 7;
            }
            // Reset scroll state
            plx->leftmost = first_sprite;
            plx->scroll_offset = SCROLL_FIX((TILE_SIZE * zoom) >> 4);
            plx->last_scroll_px = FIX_INT(parallax_offset_x);
        }

        plx->last_zoom = zoom;
    }

    // === CALCULATE SCB3 VALUE ===
    // Adjust height_bits for zoom level
    // At reduced zoom, shrunk graphics are shorter than the display window
    // which causes garbage (repeated last line) at the bottom.
    // Reduce height_bits proportionally to match the shrunk output height.
    u16 shrink = NGCameraGetShrink();
    u8 v_shrink = shrink & 0xFF;
    u16 adjusted_rows = ((u16)num_rows * v_shrink + 254) / 255;  // Ceiling division
    if (adjusted_rows < 1) adjusted_rows = 1;
    if (adjusted_rows > 32) adjusted_rows = 32;
    u8 height_bits = (u8)adjusted_rows;

    // NeoGeo Y: 496 at top of screen, decreasing goes down
    s16 y_val = 496 - base_y;
    if (y_val < 0) y_val += 512;
    y_val &= 0x1FF;

    u16 scb3_val = ((u16)y_val << 7) | height_bits;

    // === UPDATE SCB3 ONLY WHEN VALUE CHANGES ===
    if (scb3_val != plx->last_scb3) {
        NG_REG_VRAMADDR = SCB3_BASE + first_sprite;
        NG_REG_VRAMMOD = 1;
        // Don't use chain/sticky bit (0x40) - each sprite needs independent X positioning
        // for the wrap-around scrolling logic to work correctly
        for (u8 col = 0; col < num_cols; col++) {
            NG_REG_VRAMDATA = scb3_val;
        }
        plx->last_scb3 = scb3_val;
    }

    // === UPDATE SCB4 (X POSITIONS) ===
    if (infinite_width) {
        // Bidirectional X-position cycling for infinite scroll
        // Calculate current scroll position from parallax
        s16 scroll_px = FIX_INT(parallax_offset_x);

        // Calculate how many pixels we scrolled since last frame
        s16 pixel_diff = scroll_px - plx->last_scroll_px;
        plx->last_scroll_px = scroll_px;

        // === OPTIMIZATION: Skip entire update if no scrolling occurred ===
        if (pixel_diff != 0) {
            // Zoom-adjusted tile width and total width
            s16 tile_width_zoomed = (TILE_SIZE * zoom) >> 4;
            s16 total_width = num_cols * tile_width_zoomed;
            s16 tile_width_fixed = SCROLL_FIX(tile_width_zoomed);

            // Update scroll offset accumulator
            plx->scroll_offset -= (pixel_diff << SCROLL_FRAC_BITS);

            // Handle wrap-around for BOTH directions
            // scroll_offset represents distance until leftmost needs to wrap right
            // Range should stay within [0, tile_width_fixed * 2)

            // Scrolling RIGHT: leftmost goes off left edge, wrap to right
            while (plx->scroll_offset <= 0) {
                // Read leftmost sprite's X
                NG_REG_VRAMADDR = SCB4_BASE + plx->leftmost;
                s16 x = (s16)(NG_REG_VRAMDATA >> 7);
                // Wrap to right side
                x += total_width;
                NG_REG_VRAMADDR = SCB4_BASE + plx->leftmost;
                NG_REG_VRAMDATA = ((x & 0x1FF) << 7);
                // Advance leftmost
                plx->leftmost++;
                if (plx->leftmost >= first_sprite + num_cols) {
                    plx->leftmost = first_sprite;
                }
                plx->scroll_offset += tile_width_fixed;
            }

            // Scrolling LEFT: rightmost goes off right edge, wrap to left
            while (plx->scroll_offset > tile_width_fixed * 2) {
                // Retreat leftmost (rightmost becomes new leftmost)
                if (plx->leftmost <= first_sprite) {
                    plx->leftmost = first_sprite + num_cols;
                }
                plx->leftmost--;
                // Read new leftmost sprite's X
                NG_REG_VRAMADDR = SCB4_BASE + plx->leftmost;
                s16 x = (s16)(NG_REG_VRAMDATA >> 7);
                // Wrap to left side
                x -= total_width;
                NG_REG_VRAMADDR = SCB4_BASE + plx->leftmost;
                NG_REG_VRAMDATA = ((x & 0x1FF) << 7);
                plx->scroll_offset -= tile_width_fixed;
            }

            // Now update all sprite X positions by pixel_diff
            NG_REG_VRAMMOD = 1;
            for (u8 col = 0; col < num_cols; col++) {
                u16 spr = first_sprite + col;
                NG_REG_VRAMADDR = SCB4_BASE + spr;
                s16 x = (s16)(NG_REG_VRAMDATA >> 7);
                x -= pixel_diff;
                NG_REG_VRAMADDR = SCB4_BASE + spr;
                NG_REG_VRAMDATA = ((x & 0x1FF) << 7);
            }
        }
    } else {
        // Simple linear X positions for finite width
        s16 base_x = plx->viewport_x - FIX_INT(parallax_offset_x);

        // === OPTIMIZATION: Only update if base_x OR zoom changed ===
        // Zoom affects sprite spacing: at 50% zoom, tiles are half-width
        // so they need to be positioned closer together
        if (base_x != plx->last_base_x || zoom_changed) {
            NG_REG_VRAMADDR = SCB4_BASE + first_sprite;
            NG_REG_VRAMMOD = 1;
            for (u8 col = 0; col < num_cols; col++) {
                // Apply zoom to column spacing
                s16 col_offset = (col * TILE_SIZE * zoom) >> 4;
                s16 x_pos = base_x + col_offset;
                NG_REG_VRAMDATA = (x_pos & 0x1FF) << 7;
            }
            plx->last_base_x = base_x;
        }
    }
}

void _NGParallaxSystemDraw(u16 first_sprite) {
    // Placeholder - individual parallax effects are drawn via NGParallaxDraw()
    (void)first_sprite;
}

// === Public API ===

NGParallaxHandle NGParallaxCreate(
    const NGVisualAsset *asset,
    u16 width,
    u16 height,
    fixed parallax_x,
    fixed parallax_y
) {
    if (!asset) return NG_PARALLAX_INVALID;

    // Find free slot
    NGParallaxHandle handle = NG_PARALLAX_INVALID;
    for (u8 i = 0; i < NG_PARALLAX_MAX; i++) {
        if (!parallax_layers[i].active) {
            handle = i;
            break;
        }
    }
    if (handle == NG_PARALLAX_INVALID) return NG_PARALLAX_INVALID;

    // Initialize
    Parallax *plx = &parallax_layers[handle];
    plx->asset = asset;
    plx->width = width;
    plx->height = height;
    plx->parallax_x = parallax_x;
    plx->parallax_y = parallax_y;
    plx->viewport_x = 0;
    plx->viewport_y = 0;
    plx->anchor_cam_x = 0;
    plx->anchor_cam_y = 0;
    plx->z = 0;
    plx->palette = asset->palette;
    plx->visible = 1;
    plx->in_scene = 0;
    plx->active = 1;
    plx->hw_sprite_first = 0;
    plx->hw_sprite_count = 0;
    plx->tiles_loaded = 0;
    plx->last_zoom = 0;
    plx->last_scb3 = 0xFFFF;  // Invalid to force initial write
    plx->leftmost = 0;
    plx->scroll_offset = 0;
    plx->last_scroll_px = 0;
    plx->last_base_x = 0x7FFF;  // Invalid to force initial write

    return handle;
}

void NGParallaxAddToScene(NGParallaxHandle handle, s16 viewport_x, s16 viewport_y, u8 z) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;

    plx->viewport_x = viewport_x;
    plx->viewport_y = viewport_y;
    plx->z = z;

    // Record current camera position as anchor
    plx->anchor_cam_x = NGCameraGetX();
    plx->anchor_cam_y = NGCameraGetY();

    plx->in_scene = 1;
    plx->tiles_loaded = 0;  // Force tile reload

    // Mark render queue for rebuild
    _NGSceneMarkRenderQueueDirty();
}

void NGParallaxRemoveFromScene(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;

    u8 was_in_scene = plx->in_scene;
    plx->in_scene = 0;

    // Clear hardware sprites
    if (plx->hw_sprite_count > 0) {
        for (u8 i = 0; i < plx->hw_sprite_count; i++) {
            NG_REG_VRAMADDR = SCB3_BASE + plx->hw_sprite_first + i;
            NG_REG_VRAMDATA = 0;
        }
    }

    // Mark render queue for rebuild if we were in scene
    if (was_in_scene) {
        _NGSceneMarkRenderQueueDirty();
    }
}

void NGParallaxDestroy(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    NGParallaxRemoveFromScene(handle);
    parallax_layers[handle].active = 0;
}

void NGParallaxSetViewportPos(NGParallaxHandle handle, s16 viewport_x, s16 viewport_y) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;

    plx->viewport_x = viewport_x;
    plx->viewport_y = viewport_y;

    // Re-anchor to current camera position
    plx->anchor_cam_x = NGCameraGetX();
    plx->anchor_cam_y = NGCameraGetY();
}

void NGParallaxSetZ(NGParallaxHandle handle, u8 z) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;
    if (plx->z != z) {
        plx->z = z;
        // Mark render queue for rebuild if Z changed while in scene
        if (plx->in_scene) {
            _NGSceneMarkRenderQueueDirty();
        }
    }
}

void NGParallaxSetVisible(NGParallaxHandle handle, u8 visible) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;
    plx->visible = visible ? 1 : 0;
}

void NGParallaxSetPalette(NGParallaxHandle handle, u8 palette) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active) return;
    plx->palette = palette;
    plx->tiles_loaded = 0;  // Force tile reload with new palette
}

// === Scene Integration ===

void NGParallaxDraw(NGParallaxHandle handle, u16 first_sprite) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active || !plx->in_scene) return;

    draw_parallax(plx, first_sprite);
}

u8 NGParallaxGetSpriteCount(NGParallaxHandle handle) {
    if (handle < 0 || handle >= NG_PARALLAX_MAX) return 0;
    Parallax *plx = &parallax_layers[handle];
    if (!plx->active || !plx->asset) return 0;

    u16 disp_w = plx->width;
    if (disp_w == 0) disp_w = plx->asset->width_pixels;

    if (plx->width == NG_PARALLAX_WIDTH_INFINITE) {
        // Match the allocation in draw_parallax
        u8 asset_cols = plx->asset->width_tiles;
        u8 screen_cols = (SCREEN_WIDTH / TILE_SIZE) + 2;
        u8 cols = (asset_cols > screen_cols) ? asset_cols : screen_cols;
        if (cols > MAX_COLUMNS_PER_PARALLAX) cols = MAX_COLUMNS_PER_PARALLAX;
        return cols;
    }

    u8 cols = (disp_w + TILE_SIZE - 1) / TILE_SIZE;
    if (cols > MAX_COLUMNS_PER_PARALLAX) cols = MAX_COLUMNS_PER_PARALLAX;
    return cols;
}
