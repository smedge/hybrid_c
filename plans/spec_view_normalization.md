# Spec: View Normalization

## Overview

Decouple visible world area from screen resolution. A player on a 4K monitor shouldn't see 4x more world than someone on a laptop. The world projection scales based on a reference resolution so everyone sees roughly the same gameplay area at a given zoom level.

## The Problem

The world projection uses screen pixel dimensions directly:
```c
Mat4_ortho(0, screen.width, 0, screen.height, -1, 1)
```

At zoom scale 0.25, visible world = `screen_pixels / 0.25`:
- 800x600 window: 3,200 x 2,400 world units
- 1440x900 (reference): 5,760 x 3,600 world units
- 1920x1080: 7,680 x 4,320 world units
- 3840x2160 (4K): 15,360 x 8,640 world units

A 4K player sees **4x the world area** of the 800x600 player at the same zoom level. This is a gameplay advantage (see enemies sooner, react earlier) and a visual inconsistency (game feels "zoomed out" on big screens).

## Design

Introduce a **reference resolution** of **1440x900**. The world projection scales so that at any given zoom level, the visible world area never exceeds what 1440x900 would see in either dimension.

This is a **projection-only change** — the world ortho matrix uses normalized dimensions instead of raw screen pixels. All rendering still happens at the monitor's full native resolution. No render targets are resized, no texture quality is affected. A 4K player gets crisp 4K visuals; they just see the same *amount* of world as the reference resolution.

## Aspect Ratio: Largest-Dimension Locked

Determine which screen axis is proportionally largest relative to the reference, and lock to that. The other axis sees slightly less than the reference. This means no player ever sees more world than the reference in any direction — the fairest possible approach.

**Computation** (once, on window resize or fullscreen toggle):

```c
float ref_w = 1440.0f;
float ref_h = 900.0f;

// Scale factor: pick the SMALLER factor so the largest dimension
// gets clamped to reference. The other axis shrinks proportionally.
float scale_x = ref_w / screen.width;
float scale_y = ref_h / screen.height;
float scale = (scale_x < scale_y) ? scale_x : scale_y;

float norm_w = screen.width * scale;   // <= 1440
float norm_h = screen.height * scale;  // <= 900
```

The world projection uses these normalized dimensions:
```c
Mat4_ortho(0, norm_w, 0, norm_h, -1, 1)
```

**Examples at zoom 1.0:**
| Resolution | Aspect | norm_w | norm_h | Locked axis |
|-----------|--------|--------|--------|-------------|
| 1440x900 | 16:10 | 1440 | 900 | Both (reference) |
| 1920x1080 | 16:9 | 1440 | 810 | Width (height trimmed) |
| 800x600 | 4:3 | 1200 | 900 | Height (width trimmed) |
| 2560x1440 | 16:9 | 1440 | 810 | Width |
| 3840x2160 | 16:9 | 1440 | 810 | Width |
| 2560x1080 | 21:9 | 1440 | 617 | Width (tall trim on ultrawide) |

**Key property:** No resolution ever exceeds 1440 wide or 900 tall. The reference player sees the maximum possible area. Everyone else sees the same or slightly less. Zero per-frame cost — computed once on resize, cached.

At max zoom-out (0.25 scale), visible world for the reference resolution: 5,760 x 3,600 world units. All other resolutions see equal or less.

## What Changes

- `Graphics_get_world_projection()` — uses normalized dimensions instead of screen pixels
- `View_get_world_position()` — mouse-to-world conversion needs to account for the scaling (screen pixels to normalized coords before unprojection)
- Normalized dimensions computed once on window resize, cached as statics

## What Stays the Same

- Zoom range (0.25 to 4.0) — same feel, just normalized baseline
- All world-space logic (entities, physics, collision) — they don't know about the camera
- UI rendering — stays in screen pixels, untouched
- UI projection — `Mat4_ortho(0, screen.width, 0, screen.height, -1, 1)` unchanged
- FBO/bloom rendering — operates at actual pixel resolution for full-quality visuals
- Render target sizes — unchanged, all FBOs sized from actual drawable pixels
- `Graphics_get_drawable_size()` — still returns actual pixels

## HiDPI / Retina Gotcha

SDL provides two different size queries:
- `SDL_GetWindowSize()` → **logical** resolution (e.g., 1440x900 on a Retina Mac)
- `SDL_GL_GetDrawableSize()` → **actual pixel** resolution (e.g., 2880x1800 on that same Mac)

**The normalization math MUST use logical size** (`SDL_GetWindowSize`). This is what determines how much world the player sees. A Retina Mac at 1440x900 logical is the reference resolution — normalization is a no-op. A 4K laptop at 200% scaling reports 1920x1080 logical — normalization clamps that to 1440 wide, correct behavior.

**Drawable size stays for GPU work only** — `glViewport`, FBO allocation, bloom resolution. These need actual pixel counts for crisp rendering.

If we accidentally use drawable size for normalization, a Retina Mac would compute from 2880x1800 instead of 1440x900, halving the visible world area. Wrong.

**Rule of thumb:** Normalization and projection = logical size. Viewport and FBOs = drawable size. Same split we already follow, just be explicit about it during implementation.

## Implementation Notes

- Compute `norm_w` and `norm_h` once in the resize handler, cache as statics
- Use `SDL_GetWindowSize()` (logical) for the normalization input — NOT `SDL_GL_GetDrawableSize()`
- World projection: `Mat4_ortho(0, norm_w, 0, norm_h, -1, 1)`
- Mouse-to-world: convert screen pixel coords to normalized coords before applying view inverse
- UI projection: keep using raw screen pixels

## Done When

- View normalization produces consistent visible world area across resolutions
- Reference resolution (1440x900) is the baseline, largest-dimension locked
- Rendering stays at full native resolution — projection change only, no quality loss
- Mouse-to-world conversion works correctly with normalized view
- UI rendering is unaffected by normalization
