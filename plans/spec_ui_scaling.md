# UI Scaling Spec

## Overview

All UI is currently rendered in raw pixel coordinates. A 50px skillbar slot is 50 physical pixels regardless of resolution — tiny on 4K, fine on 1080p. This spec adds a single `ui_scale` multiplier that every UI module applies to its sizes, positions, and margins. A slider in Graphics settings lets the user tune it. The default is computed from window resolution.

## Design

### Scale Factor

One float: `ui_scale`. Stored in `graphics.c` alongside existing screen state. Range **0.50 to 2.50** in **0.25 increments** (9 stops).

Default computed on window creation and resize:
```
raw = min(screen.width / 1440.0f, screen.height / 900.0f)
ui_scale = round(raw * 4.0f) / 4.0f
ui_scale = clamp(ui_scale, 0.50f, 2.50f)
```

Reference resolution is 1440x900 (matches world projection reference). At that resolution, scale = 1.0 and all UI looks exactly as it does today.

### Default Scale by Resolution

| Resolution | Default |
|---|---|
| 720p (1280x720) | 0.75 |
| 900p (1440x900) | 1.00 |
| 1080p (1920x1080) | 1.25 |
| 1440p (2560x1440) | 1.50 |
| 4K (3840x2160) | 2.50 |
| MacBook Pro 14 | 1.00 |
| MacBook Pro 16 | 1.25 |
| Steam Deck | 1.00 |
| Ultrawide 1080 | 1.25 |

### API

```c
/* graphics.h */
float Graphics_get_ui_scale(void);
void  Graphics_set_ui_scale(float scale);  /* clamps + quantizes */
```

`Graphics_set_ui_scale()` clamps to range, quantizes to nearest 0.25, stores the value, and triggers font atlas rebuild.

### Font Atlas Rebuild

The text renderer rasterizes a bitmap font atlas at initialization. When `ui_scale` changes, the atlas must be rebuilt at the new effective font size:

```
effective_size = base_font_size * ui_scale
```

`Graphics_set_ui_scale()` calls `Text_cleanup()` + `Text_initialize()` on both text renderers (body and title) with the scaled size. This happens on slider release, not during drag, to avoid per-frame atlas rebuilds.

`Text_measure_width()` returns values based on the current atlas, so text-driven layouts (column widths in player_stats, button sizing in settings) automatically track the scale — no additional math needed for text measurements.

### How UI Modules Use It

Every UI module calls `Graphics_get_ui_scale()` once at the top of its render function and multiplies its layout constants by the result. The `#define` constants remain unchanged — they represent "base size at scale 1.0."

Pattern:
```c
void Skillbar_render(const Screen *screen)
{
    float s = Graphics_get_ui_scale();
    float slot_size = SLOT_SIZE * s;
    float slot_spacing = SLOT_SPACING * s;
    float slot_margin = SLOT_MARGIN * s;
    float slot_chamf = SLOT_CHAMF * s;
    float base_x = slot_margin;
    float base_y = (float)screen->height - slot_size - slot_margin;
    // ... rest uses local scaled vars
}
```

Hit testing in update functions must apply the same scale:
```c
void Skillbar_update(const Input *input, const Screen *screen)
{
    float s = Graphics_get_ui_scale();
    float slot_size = SLOT_SIZE * s;
    // mouse hit testing uses scaled values
}
```

### Affected Modules

Every file that uses `Graphics_get_ui_projection()` and hardcoded pixel constants:

| Module | Constants to Scale | Hit Testing |
|---|---|---|
| `skillbar.c` | SLOT_SIZE, SLOT_SPACING, SLOT_MARGIN, SLOT_CHAMF | Yes — slot clicks |
| `player_stats.c` | MARGIN_X/Y, BAR_WIDTH, BAR_HEIGHT, COL_GAP, FLASH_THICKNESS | No |
| `catalog.c` | Window size, tab sizes, slot sizes, drag ghost | Yes — tabs, slots, drag |
| `settings.c` | Window size, tab sizes, slider dims, keybind buttons, scrollbar | Yes — tabs, sliders, buttons, scrollbar |
| `data_logs.c` | Panel size, entry heights, scrollbar, accordion | Yes — entries, scrollbar |
| `data_node.c` | Prompt/scrollbar sizes | Yes — scrollbar |
| `map_window.c` | Window size, POI dot sizes | Yes — close button |
| `hud.c` | Position offsets | No |
| `fragment.c` | Notification text position | No |
| `progression.c` | Notification text position | No |
| `zone.c` | Notification text position | No |
| `savepoint.c` | UI overlay sizes | No |
| `portal.c` | UI overlay sizes | No |
| `palette_editor.c` | Color picker sizes, buttons | Yes — pickers, buttons |
| `mode_mainmenu.c` | Menu layout, button sizes | Yes — menu clicks |
| `mode_gameplay.c` | Misc UI overlays | Minimal |

### Line Width and Point Size

`Render_thick_line` widths and point sizes used in UI (borders, separators) also need scaling. These are typically 1-2px values in the render calls. UI modules should multiply these by `s` as well.

### Settings Integration

**Graphics tab** gets a new slider: "UI Scale" with the quantized 0.25-step range.

Display format: percentage — `"75%"`, `"100%"`, `"125%"`, etc.

Behavior:
- Dragging the slider shows a live preview (positions/sizes update immediately)
- Font atlas rebuilds on slider release (not during drag — too expensive)
- During drag, text may appear slightly soft (rendered at old atlas size, positioned at new scale) — acceptable since it's momentary
- Cancel restores original scale + rebuilds atlas
- Persisted as integer (50-250) in settings.cfg: `ui_scale=100`
- "Reset Defaults" recomputes from current window resolution

### Persistence and Initialization Order

1. `Graphics_initialize()` computes default scale from window size
2. `Settings_load()` reads `ui_scale` from settings.cfg (if present), calls `Graphics_set_ui_scale()`
3. On window resize, default is NOT recomputed if user has a saved preference — user's choice takes priority
4. "Reset Defaults" in settings recomputes from window size and clears the saved preference

### Map Window and Minimap Scaling

The map window frame AND its content must scale together so the same amount of terrain is always visible. If the window doubles in size, the map viewport doubles — the player sees the same coverage at the same relative detail. This preserves spatial awareness across scale settings.

- `map_window.c`: Window dimensions, POI dot sizes, border thickness — all scale by `s`
- The map's internal projection (cells-per-pixel or world-units-per-pixel) must also scale by `s` so the rendered terrain fills the scaled frame identically
- If the minimap uses a separate ortho projection for its content, that projection's extents scale with the window size — NOT independently
- Same applies to the HUD minimap if one exists — frame and content scale together

**The rule: a player at scale 0.50 and a player at 2.50 see the exact same terrain coverage in their map. The map just gets physically bigger/smaller on screen.**

### What Does NOT Scale

- **World projection and gameplay** — completely independent, already normalized
- **Bloom FBOs** — resolution-dependent, not UI
- **Map cell rendering** — world space
- **Cursor/crosshair** — scales with UI. Line thickness, center dot, and crosshair arm lengths all multiply by `s`. Line widths are rounded to nearest integer pixel and clamped to a minimum of 1px — prevents sub-pixel rendering artifacts and invisible lines at low scales

## Implementation Order

1. Add `ui_scale` to graphics.c with get/set API + quantization + clamping
2. Add font atlas rebuild on scale change
3. Scale `skillbar.c` (most visible, fast to validate)
4. Scale `player_stats.c` (simple — no hit testing)
5. Scale `settings.c` (complex — has all widget types, include new slider)
6. Scale `catalog.c` (complex — drag-drop hit testing)
7. Scale remaining modules (data_logs, data_node, map_window, hud, fragment, progression, zone, savepoint, portal, palette_editor, mode_mainmenu, mode_gameplay)
8. Add settings.cfg persistence
9. Test at 0.50, 1.00, 1.50, 2.50 across window sizes

## Risk

- **Missed spots.** If one constant in one file doesn't get scaled, that element will be wrong. Mitigation: systematic file-by-file pass, visual test at extreme scales (0.50 and 2.50 make errors obvious).
- **Font softness at non-integer scales.** The bitmap atlas won't be pixel-perfect at 1.25x or 1.75x. The square_sans_serif_7 font is chunky enough that this should be acceptable. If it's not, we could quantize to 0.5 increments instead (5 stops: 0.5, 1.0, 1.5, 2.0, 2.5).
- **Layout overflow.** Scaled UI elements might overlap or extend off-screen at high scales on small windows. The clamp to 2.50 and the resolution-based default mitigate this — you'd have to manually crank scale way past the default to hit problems.
