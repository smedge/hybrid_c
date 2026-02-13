# Spec 003: UI Panel Layer

## Problem

The current UI rendering is hand-rolled immediate-mode: each system (skillbar, catalog, HUD, progression) directly calls `Render_*` and `Text_render`, manages its own hit testing, tracks its own mouse state, and handles input consumption through ad-hoc flag passing between files.

This works but has recurring friction:

1. **Z-order bugs**: The batch renderer flushes lines → triangles → points. Any UI element using the wrong primitive type gets buried. This has burned us twice (line-based icons under quad backgrounds, text under geometry).

2. **Two-pass rendering**: Text renders immediately via the text shader; geometry batches. Every panel must manually do geometry → `Render_flush` → text, or text disappears under quads. Easy to forget, hard to debug.

3. **Input routing sprawl**: `mouseWasDown`, `rightWasDown`, `clickConsumed`, `ui_wants_mouse` scattered across skillbar.c, catalog.c, mode_gameplay.c. Each new UI element needs to wire into this chain. Getting the order wrong means clicks bleed through to gameplay.

4. **Redundant hit testing**: Every clickable region hand-codes its own bounds check. Skillbar has `slot_at_position`, catalog has inline bounds checks for tabs, items, etc.

## Goals

- Eliminate z-order bugs by automating the geometry → flush → text pattern
- Centralize input consumption so UI elements don't need to coordinate via flags
- Provide reusable hit testing without per-widget boilerplate
- **Zero heap allocation** — all state in static arrays, sized at compile time
- **Negligible CPU cost** — no tree traversal, no layout solver, no per-frame allocation
- Preserve the immediate-mode feel: code reads top-to-bottom, render and logic in the same place
- Build on top of existing `Render_*` and `Text_render` APIs, not replace them

## Non-Goals

- Full widget toolkit (buttons, text fields, sliders, etc.)
- Automatic layout (flexbox, grid, anchors)
- Theming or skinning system
- Serialization or hot-reload of UI definitions
- Animation system (existing code handles this fine per-widget)

## Design

### Core Concept: UI Panels

A **panel** is a rectangular screen region that:
- Captures all rendering within it into a proper two-pass sequence (geometry, then text)
- Registers itself for input hit testing
- Can be marked as opaque to block input from reaching panels behind it

Panels are not nested widgets. They're just rectangular regions with automatic flush management. The code inside a panel still uses raw `Render_*` and `Text_render` calls — nothing changes about how you draw.

### API

```c
/* ui.h */

#define UI_MAX_PANELS 16

typedef struct {
    float x, y, w, h;       /* screen-space bounds */
    bool opaque;             /* blocks mouse input to panels/gameplay behind it */
    int z_order;             /* higher = on top (for input priority) */
} UiPanelDesc;

typedef int UiPanel;         /* handle, -1 = invalid */

/* Frame lifecycle — call once per frame in mode_gameplay */
void Ui_begin_frame(const Input *input);
void Ui_end_frame(void);

/* Panel management */
UiPanel Ui_begin_panel(UiPanelDesc desc);
void    Ui_end_panel(void);        /* flushes geometry, then text */

/* Input queries — valid between Ui_begin_frame and Ui_end_frame */
bool Ui_mouse_in_panel(UiPanel panel);
bool Ui_clicked(UiPanel panel);     /* mouse-down edge within panel */
bool Ui_mouse_blocked(void);        /* true if any opaque panel is under cursor */

/* Rect hit testing helpers */
bool Ui_rect_hovered(float x, float y, float w, float h);
bool Ui_rect_clicked(float x, float y, float w, float h);  /* edge-detected */
bool Ui_rect_held(float x, float y, float w, float h);     /* continuous */

/* Text rendering that queues for second pass (no manual flush needed) */
void Ui_text(TextRenderer *tr, Shaders *shaders,
             const char *str, float x, float y,
             float r, float g, float b, float a);
```

### How It Works

#### Rendering

```
Ui_begin_panel()
    → stores current batch vertex counts as "checkpoint"
    → pushes panel onto render stack

    ... user code calls Render_quad, Render_thick_line, etc. (geometry) ...
    ... user code calls Ui_text() instead of Text_render() (text is queued) ...

Ui_end_panel()
    → calls Render_flush() to draw all geometry
    → replays queued Ui_text() calls via Text_render()
    → text renders on top of geometry, always
```

This eliminates the two-pass bug entirely. You never need to think about flush ordering inside a panel.

#### Input

```
Ui_begin_frame(input)
    → stores input pointer
    → resets per-frame state
    → mouseWasDown edge detection handled internally

Ui_end_frame()
    → after all panels have been processed
    → determines if any opaque panel consumed the mouse

Ui_mouse_blocked()
    → returns true if cursor is inside any opaque panel
    → mode_gameplay uses this instead of ad-hoc Catalog_is_open() || Skillbar_consumed_click()
```

Input priority: panels with higher `z_order` get first claim. If the cursor is inside an opaque panel, all panels with lower z_order (and gameplay) are blocked.

### Usage Example

**Before** (current catalog render):
```c
void Catalog_render(const Screen *screen) {
    // geometry pass
    Render_quad_absolute(...);  // background
    Render_thick_line(...);     // borders
    Render_flush(&proj, &ident);  // MANUAL FLUSH — easy to forget

    // text pass
    Text_render(tr, shaders, &proj, &ident, "PROJECTILE", ...);
    // ... more text ...
}
```

**After** (with UI panels):
```c
void Catalog_render(const Screen *screen) {
    UiPanel p = Ui_begin_panel((UiPanelDesc){
        .x = px, .y = py, .w = pw, .h = ph,
        .opaque = true, .z_order = 10
    });

    Render_quad_absolute(...);  // background
    Render_thick_line(...);     // borders
    Ui_text(tr, shaders, "PROJECTILE", ...);  // just works, no flush needed

    Ui_end_panel();  // geometry flushed, then text rendered on top
}
```

**Before** (mode_gameplay input routing):
```c
bool ui_wants_mouse = Catalog_is_open() || Skillbar_consumed_click();
Input filtered = *input;
if (ui_wants_mouse) {
    filtered.mouseLeft = false;
    ...
}
```

**After**:
```c
Input filtered = *input;
if (Ui_mouse_blocked()) {
    filtered.mouseLeft = false;
    filtered.mouseRight = false;
    filtered.mouseMiddle = false;
}
```

### Internal Data Structures

```c
/* All static, no heap allocation */

typedef struct {
    float x, y, w, h;
    bool opaque;
    int z_order;
    bool active;
    /* Text queue for deferred rendering */
    int text_start;          /* index into text queue ring buffer */
    int text_count;
} PanelState;

static PanelState panels[UI_MAX_PANELS];
static int panel_count;
static int active_panel;     /* stack of one — no nesting needed */

/* Text queue: ring buffer of deferred text calls */
#define UI_MAX_TEXT_CALLS 128
typedef struct {
    const char *str;
    float x, y, r, g, b, a;
} TextCall;
static TextCall text_queue[UI_MAX_TEXT_CALLS];
static int text_queue_count;

/* Input state */
static const Input *frame_input;
static bool mouse_was_down;
static bool frame_mouse_blocked;
```

### Performance Analysis

**Per frame overhead:**
- `Ui_begin_frame`: reset ~3 variables. Trivial.
- `Ui_begin_panel`: write one `PanelState` struct. Trivial.
- `Ui_end_panel`: one `Render_flush` call (already happens), loop over queued text calls (already happens). Same GPU cost as today.
- `Ui_text`: copy ~7 floats + 1 pointer into ring buffer. Trivial.
- `Ui_mouse_blocked`: iterate ≤16 panels, check bounds. Trivial.
- `Ui_rect_clicked`: one AABB test + one bool check. Trivial.

**Memory:** ~3KB total for all static arrays. Negligible.

**GPU:** Identical to current approach. Same number of draw calls, same vertex data. The layer adds zero GPU work.

**What it replaces:** Manual `Render_flush` calls, manual `mouseWasDown` tracking, manual bounds checking. These are the same operations, just centralized.

### Migration Path

This is **additive** — existing code continues to work unchanged. Migration is opt-in per system:

1. **Phase 1**: Implement `ui.c/h`. Migrate catalog.c (biggest pain point — complex panel, drag-drop, text over geometry).
2. **Phase 2**: Migrate skillbar.c (simpler — just input consumption and hit testing).
3. **Phase 3**: Migrate HUD, progression notifications. Replace `ui_wants_mouse` chain in mode_gameplay.c with `Ui_mouse_blocked()`.

Each phase is independent. Old-style and new-style UI can coexist in the same frame.

### Open Questions

1. **Projection matrix**: Panels need the UI projection + identity view for `Render_flush`. Should `Ui_end_panel` take these as params, or should it grab them from `Graphics_get_ui_projection()` internally? Leaning toward internal — all UI uses the same projection.

2. **Scrollable regions**: The catalog has a scrollable item list with clipping. Should panels support a clip rect (via `glScissor`)? Or handle clipping at the application level as we do now? A simple optional clip rect on `UiPanelDesc` would cover this cleanly.

3. **Drag-and-drop**: The catalog's drag system (threshold detection, ghost rendering, source/target tracking) is complex. Should the UI layer provide drag primitives, or leave that to application code? Leaning toward application code — drag logic is game-specific and there's only one drag system.

4. **Skillbar z_order**: The skillbar renders in the world below the catalog. It should have a lower z_order so catalog input takes priority when both are visible. Suggested: skillbar z=1, catalog z=10, so future panels can slot between them.

## File Summary

| File | Action |
|------|--------|
| `src/ui.h` | New — panel API, input helpers |
| `src/ui.c` | New — implementation (~200 lines) |
| `src/catalog.c` | Migrate to Ui_begin/end_panel, Ui_text |
| `src/skillbar.c` | Migrate click handling to Ui_rect_clicked |
| `src/mode_gameplay.c` | Replace ad-hoc input routing with Ui_mouse_blocked |
