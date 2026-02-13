# Spec 002: Skill Bar & Catalog System

## Overview

The skill bar and catalog form the player's subroutine management interface. The skill bar is a persistent 10-slot hotbar at the bottom of the screen. The catalog is a modal window for browsing and equipping subroutines. Together they implement the core loop: unlock subs via progression, browse them in the catalog, drag them into skill bar slots, activate with number keys, fire with type-specific inputs.

This spec covers three implementation phases:
1. **Phase 1 — Functional Skill Bar**: Slots, activation, type exclusivity, geometric icons
2. **Phase 2 — Catalog Window**: Modal browser, tabs, drag-and-drop equipping
3. **Phase 3 — God Mode Dual Loadout**: Two-loadout swap, placeable catalog

---

## Subroutine Type System

Every subroutine has a **type** that determines which input fires it and enforces exclusivity — only one sub of each type can be active at a time.

| Type | Fire Input | Example Subs |
|------|-----------|-------------|
| Projectile | LMB | sub_pea, sub_plasma (future) |
| Deployable | Space | sub_mine, sub_proximity (future) |
| Movement | Shift | sub_egress (future) |
| Shield | Slot key toggle | (future) |
| Healing | Slot key trigger | (future) |

**Note on Movement type**: Shift is the movement sub input. The current speed boost (Shift in ship.c) is the hardcoded prototype of sub_egress. When sub_egress is implemented as a proper subroutine, that hardcoded boost logic moves into `sub_egress.c` and fires via `Skillbar_is_active(SUB_ID_EGRESS)` like any other sub. Behavior changes from the prototype: Shift activates a 5-second boost, then a 30-second cooldown before it can be used again (the cooldown sweep on the skill bar will be very visible for this one).

### Activation Model

Number keys (1-9, 0) **activate** the sub in that slot. They don't fire it. Activation means "this is now the active sub for its type." When you activate a slot:

- If the slot is empty: nothing happens
- If the slot has a sub: it becomes the active sub for its type
- Any other slot with a sub of the **same type** is deactivated
- Subs of **different types** are unaffected (multiple types active simultaneously is normal)

Firing happens via the type's input (LMB, Space, etc.), which invokes whatever the currently active sub of that type is.

**Example flow**:
```
Slot 1: sub_pea (projectile) — ACTIVE
Slot 2: sub_mine (deployable) — ACTIVE
Slot 3: sub_plasma (projectile) — inactive

State: LMB fires pea, Space deploys mine

Player presses 3:
Slot 1: sub_pea (projectile) — deactivated (same type as slot 3)
Slot 2: sub_mine (deployable) — ACTIVE (unaffected, different type)
Slot 3: sub_plasma (projectile) — ACTIVE

State: LMB fires plasma, Space deploys mine
```

---

## Phase 1: Functional Skill Bar

### Data Model

New central registry in `skillbar.h/c`:

```c
typedef enum {
    SUB_TYPE_PROJECTILE,
    SUB_TYPE_DEPLOYABLE,
    SUB_TYPE_MOVEMENT,
    SUB_TYPE_SHIELD,
    SUB_TYPE_HEALING,
    SUB_TYPE_COUNT
} SubroutineType;

typedef struct {
    SubroutineId id;          // from progression.h enum
    SubroutineType type;
    const char *name;         // "sub_pea", "sub_mine"
    const char *short_name;   // "PEA", "MINE" (fallback label)
    bool unlocked;            // mirrors Progression_is_unlocked()
} SubroutineInfo;
```

**Skill bar state**: array of 10 slots, each holding a `SubroutineId` (or -1 for empty). Plus an `active_sub[SUB_TYPE_COUNT]` array tracking which sub ID is active per type.

### Slot Layout

Current layout: 10 slots at bottom-left, 50x50 each, 60px spacing (10px gaps). Keep this layout — it's already positioned and working.

### Slot Visual States

Each slot renders differently based on its state:

| State | Background | Border | Content |
|-------|-----------|--------|---------|
| Empty | dark gray (0.1, 0.1, 0.1, 0.8) | none | number label only |
| Equipped, inactive | dark gray | dim white (0.3, 0.3, 0.3, 0.6) 1px | geometric icon + number |
| Equipped, active | dark gray | bright white (1.0, 1.0, 1.0, 0.9) 2px | geometric icon + number |
| Equipped, on cooldown | dark gray | bright white border | dimmed icon + clockwise pie sweep (see below) |
| Hover (catalog drag) | slightly lighter gray | yellow (phase 2) | — |

### Geometric Icons

Each sub gets a tiny icon rendered with the same primitives as its in-game visual. Icons are rendered centered within the 50x50 slot area. These are defined per-sub, not per-type.

| Subroutine | Icon Description |
|-----------|-----------------|
| sub_pea | White filled circle (point), size ~8px, centered in slot |
| sub_mine | Gray diamond (small quad at 45deg, ~16px) + red center dot (point, 3px) |
| (future subs) | Each defines its own icon render function |

Only unlocked subs can be equipped in slots, so the skill bar never shows locked/unknown items.

### Cooldown Sweep

When a sub fires and enters cooldown, its skill bar slot shows a **clockwise pie-chart sweep** that visually fills as the cooldown expires:

- **On fire**: icon dims to ~30% opacity. A dark overlay covers the full slot.
- **During cooldown**: the overlay recedes clockwise starting from 12 o'clock (top center). The revealed portion shows the icon at full brightness. At 25% cooldown elapsed, the top-right quadrant is uncovered. At 50%, the right half. At 75%, only the bottom-left quadrant is still dimmed. At 100%, the full icon is bright again.
- **Cooldown complete**: icon returns to full brightness, overlay gone.

**Implementation**: Render a filled pie segment (dark, semi-transparent) over the slot icon representing the remaining cooldown fraction. The pie starts as a full circle and shrinks clockwise from 12 o'clock. Use `Render_filled_circle` segments or triangle fans to draw the pie wedge.

**Data flow**: Each sub needs to expose its cooldown state. Add a query API:
```c
float Skillbar_get_cooldown_fraction(SubroutineId id);
// Returns 0.0 (ready) to 1.0 (just fired, full cooldown remaining)
```

The skill bar calls into each sub's cooldown state. Sub_pea's 500ms cooldown will sweep fast. Sub_mine's 250ms will be nearly instant. Longer-cooldown subs (future) will have a more dramatic sweep.

Implementation: each sub registers an icon render callback `void (*render_icon)(float cx, float cy, float size)` that draws its icon at the given center position and scale.

### Number Key Input

Add `bool key1` through `bool key9` and `bool key0` to the `Input` struct. Wire as one-shot on key-up (same pattern as keyG, keyTab, keySpace).

Alternatively: add a single `int keySlot` field (-1 = none, 0-9 = which slot was pressed). Cleaner, avoids 10 bools.

**Recommendation**: Single `int keySlot` field, set to -1 in reset_input, set to 0-9 on key-up for the corresponding number key.

### Auto-equip on Unlock

When `Progression_update()` detects a new unlock, it calls `Skillbar_auto_equip(SubroutineId id)`. This:
1. Finds the first empty slot
2. Equips the sub there
3. If no other sub of that type is active, auto-activates it

This means sub_pea starts in slot 1 (equipped at Ship_initialize time since it's always unlocked), and sub_mine goes into slot 2 when unlocked.

### Gating Sub Usage

Currently sub_pea and sub_mine check their own conditions (progression unlock, cooldown, input). With the skill bar, add one more gate:

```c
// In Sub_Pea_update:
if (userInput->mouseLeft && cooldownTimer <= 0 && !state->destroyed
        && Skillbar_is_active(SUB_ID_PEA)) {
    // fire
}

// In Sub_Mine_update:
if (userInput->keySpace && cooldownTimer <= 0
        && Skillbar_is_active(SUB_ID_MINE)) {
    // deploy
}
```

`Skillbar_is_active()` returns true if the given sub ID is the active sub for its type. This replaces the `Progression_is_unlocked()` check in sub_mine.c (the skillbar won't let you equip something that isn't unlocked, so the gate is implicit).

Actually — keep the progression check too, as a safety net. Belt and suspenders. But the skill bar is the primary gate for "can I use this sub right now."

### Public API (skillbar.h)

```c
void Skillbar_initialize(void);
void Skillbar_cleanup(void);
void Skillbar_update(const Input *input, const unsigned int ticks);
void Skillbar_render(const Screen *screen);

bool Skillbar_is_active(SubroutineId id);
void Skillbar_auto_equip(SubroutineId id);
void Skillbar_equip(int slot, SubroutineId id);  // for catalog drag-drop (phase 2)
void Skillbar_clear_slot(int slot);
```

### Integration

- `Hud_render()` calls `Skillbar_render()` instead of its own `render_skill_bar()`.
- `Ship_update()` (or `mode_gameplay.c` update) calls `Skillbar_update()` to process number key input.
- Sub_pea and sub_mine check `Skillbar_is_active()` before firing.
- `Progression_update()` calls `Skillbar_auto_equip()` on new unlocks.
- Sub_pea is equipped in slot 1 at initialization (it's always available).

### What Changes for the Player

Before: LMB always fires pea, Space always deploys mine (if unlocked).
After: Same behavior by default (auto-equip, auto-activate). But now number keys can switch between subs of the same type when more exist. The skill bar visually shows what's active.

---

## Phase 2: Catalog Window

### Opening/Closing

**P key** toggles the catalog open/closed. One-shot on key-up.

When open:
- Game world continues running — entities update, mines blink, animations play. The world is alive behind the overlay. You can die while browsing your loadout. Stay sharp.
- Player movement/firing input is suppressed (WASD, LMB, Space, etc.)
- Mouse input captured by catalog UI
- ESC also closes the catalog
- Skill bar remains visible and interactive (drop target for drags)

### Visual Design

The catalog should feel like an in-world data terminal, not an OS window. Dark panels with geometric borders, matching the game's aesthetic.

```
+------------------------------------------------------------------+
|  [P] CATALOG                                                      |
|                                                                    |
|  +------------+ +-----------------------------------------------+ |
|  | PROJECTILE | |                                                | |
|  |            | |  [o] sub_pea                                   | |
|  | DEPLOYABLE | |      Basic projectile. Fires toward cursor.    | |
|  |            | |      UNLOCKED                                  | |
|  | MOVEMENT   | |                                                | |
|  |            | |  [?] sub_plasma                                | |
|  | SHIELD     | |      ???                                       | |
|  |            | |      12/50 fragments                           | |
|  | HEALING    | |                                                | |
|  |            | |                                                | |
|  +------------+ +-----------------------------------------------+ |
|                                                                    |
+------------------------------------------------------------------+
|  [ 1:PEA ][ 2:MINE ][ 3:   ][ 4:   ][ 5:   ]...    [MINIMAP]   |
+------------------------------------------------------------------+
```

### Layout Dimensions

Catalog overlay: centered on screen, ~80% width, ~70% height. Semi-transparent dark background (0.05, 0.05, 0.08, 0.9) covers the full screen behind it.

- **Left panel** (tab bar): ~120px wide. Vertical stack of tab buttons.
- **Right panel** (item list): remaining width. Vertically scrollable.
- **Margin/padding**: 20px outer, 10px inner.

### Tab Bar

One tab per `SubroutineType`. Each tab is a rectangle (~120x40) with:
- Type icon (small geometric shape representing the type)
- Type name text

| Tab | Icon | Color |
|-----|------|-------|
| Projectile | small dot/circle | white |
| Deployable | small diamond | gray |
| Movement | small arrow/chevron | red |
| Shield | small square outline | blue |
| Healing | small plus/cross | green |

**Selected tab**: bright border + slightly lighter background.
**Click**: switches the right panel to show subs of that type.
**Default**: first tab with content (Projectile).

### Item List

Each entry is a horizontal row (~80px tall) containing:

```
+---+----------------------------------------+
| G |  sub_pea                               |
| E |  Basic projectile. Fires toward cursor.|
| O |  UNLOCKED              [in slot 1]     |
+---+----------------------------------------+
```

- **Left**: 60x60 geometric icon (same as slot icon but larger)
- **Right**: name (bright text), description (dim text), status line

**Discovery states**: A subroutine has three visibility states:
- **Undiscovered** (0 fragments collected): not visible in the catalog at all. The player doesn't know it exists.
- **Discovered** (1+ fragments, not yet unlocked): visible but locked. Shows "???" name, "?" icon, fragment progress. No description.
- **Unlocked** (threshold met): full visibility. Name, geometric icon, description, draggable.

**Status line variants**:
- Unlocked, equipped: "UNLOCKED" (magenta) + "slot 3" (dim)
- Unlocked, not equipped: "UNLOCKED" (magenta) + "drag to equip" (dim)
- Discovered, locked: "???" name, "?" character centered in icon area (dim white text), "3/5 fragments" + progress bar. Not draggable.
- Undiscovered (0 fragments): not shown in catalog at all

**Hover**: entry highlights with a subtle bright border.

**Scrolling**: mouse wheel scrolls the list if it exceeds the panel height. Scrollbar track on the right edge (thin, geometric).

### Drag-and-Drop

**Initiation**: click and hold (mouse down) on an **unlocked** catalog entry for ~100ms (brief hold to distinguish from a click). Or just mouse-down-and-move — if mouse moves more than 5px while held, it's a drag.

**During drag**:
- A ghost icon (the sub's geometric icon, semi-transparent) follows the mouse cursor
- Skill bar slots that are valid drop targets highlight with a colored border
- Catalog entry stays in place (you're dragging a copy, not moving)

**Drop on skill bar slot**:
- Sub is equipped in that slot
- If the slot previously held a different sub, that sub is unequipped (removed from slot, but still in catalog)
- If no other sub of this type is active, auto-activate the newly equipped one
- If another sub of this type IS active, the new one is equipped but inactive (player must press the number key to switch)

**Drop elsewhere** (not on a slot): cancel, nothing happens.

**Right-click on a skill bar slot**: unequip (clear the slot). Alternative: drag from slot to catalog area to unequip.

### Catalog Data Source

The catalog reads from:
- `SubroutineInfo` registry (defined in skillbar.c) for sub metadata
- `Progression_is_unlocked()` / `Progression_get_progress()` for unlock state
- `Skillbar` slot data for equipped/active state

No separate catalog data structure needed — it's a view over existing data.

### Input Handling

When catalog is open:
- Mouse events go to catalog UI (tabs, list, drag)
- Number keys still work for slot activation
- P or ESC closes catalog
- All other input suppressed (no ship movement, no firing)

### New Input

Add `bool keyP` to Input struct, one-shot on key-up.

---

## Phase 3: God Mode Dual Loadout

### Two Loadout Arrays

The skill bar maintains two independent slot arrays:
- `gameplay_slots[10]` — subroutines
- `godmode_slots[10]` — placeables

Pressing G swaps which array the skill bar displays and interacts with. The underlying data for each is preserved independently.

### God Mode Placeables

In god mode, skill bar slots hold **placeables** instead of subroutines. A placeable is anything you can place in the world via the level editor.

```c
typedef enum {
    PLACEABLE_TYPE_CELL,
    PLACEABLE_TYPE_ENEMY,
    PLACEABLE_TYPE_PORTAL,
    PLACEABLE_TYPE_DECORATION,
    PLACEABLE_TYPE_COUNT
} PlaceableType;
```

### God Mode Catalog Tabs

When the catalog opens in god mode, tabs show placeable categories instead of sub types:

| Tab | Contents |
|-----|----------|
| Map Cells | Cell types from the current zone definition (solid, circuit, etc.) |
| Enemies | Enemy spawn point types (mine, future enemy types) |
| Portals | Zone transition points (future) |
| Decorations | Visual-only elements (future) |

### God Mode Activation

In god mode, the "active slot" determines what LMB places. This replaces the current Tab-to-cycle system.

- Press 1-0 to select the active placeable slot
- LMB places the active placeable at cursor (grid-snapped for cells)
- RMB removes (same as current)
- Ctrl+Z undoes (same as current)

### Migration from Current God Mode

Current god mode state in `mode_gameplay.c` (`godModeSelectedType`, Tab cycling) gets replaced by the skill bar slot system. The god mode HUD text ("Type: solid") can be removed once the skill bar visually shows the active placeable.

---

## File Plan

### New Files
- `src/skillbar.h` / `src/skillbar.c` — skill bar data, activation, rendering, slot management
- `src/catalog.h` / `src/catalog.c` — catalog window state, rendering, drag-and-drop

### Modified Files

| File | Changes |
|------|---------|
| `src/input.h` | Add `int keySlot` (-1/0-9), `bool keyP` |
| `src/sdlapp.c` | Wire number keys (1-9, 0) and P key |
| `src/hud.c` | Remove `render_skill_bar()`, delegate to `Skillbar_render()` |
| `src/mode_gameplay.c` | Add `Skillbar_update()` call, catalog toggle on P, pause logic when catalog open |
| `src/sub_pea.c` | Add `Skillbar_is_active(SUB_ID_PEA)` gate |
| `src/sub_mine.c` | Add `Skillbar_is_active(SUB_ID_MINE)` gate |
| `src/progression.h` | Add `SUB_ID_PEA` to the enum (currently only `SUB_ID_MINE` exists) |
| `src/progression.c` | Call `Skillbar_auto_equip()` on unlock |
| `src/ship.c` | Wire `Skillbar_initialize/cleanup` |

### Dependency Order

Phase 1 has no dependency on Phase 2 or 3. Phase 2 depends on Phase 1 (needs slot data to show equipped state and provide drop targets). Phase 3 depends on Phase 1 (dual loadout extends the slot system) and can be done independently of Phase 2.

```
Phase 1 (Skill Bar)
  |         |
  v         v
Phase 2   Phase 3
(Catalog)  (God Mode)
```

---

## Open Questions

1. ~~**SUB_ID_PEA**~~: Resolved — sub_pea gets `SUB_ID_PEA` in the progression enum with `threshold = 0` (unlocked from the start). On new game, it's auto-equipped in slot 1 and auto-activated. The player can unequip and re-equip it freely via the catalog like any other sub.

2. ~~**Catalog pause behavior**~~: Resolved — game keeps running while catalog is open. Player movement/firing suppressed but AI, mines, animations all continue. You can die while browsing. This adds tension to loadout decisions and keeps the world feeling alive.

3. ~~**Slot persistence**~~: Resolved — slots persist until the player explicitly changes them. Deaths, quitting, zone transitions, god mode toggle and back — loadout is always preserved. Both gameplay and god mode loadouts are independent persistent state. Save-load serialization comes with the save system later.

4. ~~**Slot rearrangement**~~: Resolved — dragging from one skill bar slot to another swaps their contents. If the target slot was empty, the source slot becomes empty after the swap. This is part of the Phase 2 drag-and-drop system (same drag mechanics, just slot-to-slot instead of catalog-to-slot).
