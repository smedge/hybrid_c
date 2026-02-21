# Spec: Fog of War

## Overview

The **world map** (M key overlay) starts fully blacked out. As the player explores, cells that fall within the **minimap's** display range (the radar in the bottom-right corner during gameplay) are permanently revealed on the world map. The minimap drives the reveal — whatever cells it can see get marked as explored.

**What's affected:** Only the world map (M key). The gameplay view and minimap display are not fogged.

## Terminology

- **Minimap**: the radar in the bottom-right during gameplay. Shows a `RADAR_RANGE` (15,000 world units = 150x150 cells) area centered on the player. Defined in `hud.c`.
- **World map**: the full-zone overlay opened with M key. Implemented in `map_window.c`. This is what gets fogged.
- **Gameplay view**: the main camera. Not affected by FoW — the player always sees what's around them. The gameplay view is always smaller than the minimap range (except during the death/rebirth zoom-out, which is intentional).

## Reveal Radius

Derived directly from the minimap's `RADAR_RANGE`:

```
reveal_radius_cells = RADAR_RANGE / (2 * MAP_CELL_SIZE)
                    = 15000 / (2 * 100) = 75 cells in each direction
```

Each frame, all cells within a 150x150 rectangle centered on the player get marked as revealed. If `RADAR_RANGE` changes later (minimap zoom), the reveal radius changes automatically.

To keep FoW decoupled from hud.c internals, expose this value via a function:

```c
// hud.h
float Hud_get_minimap_range(void);  // returns RADAR_RANGE
```

FoW computes the cell radius from that.

## FoW State

A static boolean grid tracks revealed cells:

```c
static bool revealed[MAP_SIZE][MAP_SIZE];  // ~1MB, zero-initialized per zone load
```

- **Zone load**: all cells reset to `false` (fully fogged)
- **Each update tick**: mark cells within reveal radius of player as `true`
- **Player death**: revealed state **persists** (exploration progress is never lost within a session)
- **Savepoint save**: FoW state is written to `./save/fog_of_war.bin` alongside the checkpoint `.sav` file. Restored on load-from-save and cross-zone death respawn.
- **Zone transition**: reset (new zone = new fog)
- **Procgen regeneration**: reset (new terrain layout = new exploration)

The reveal update runs in `FogOfWar_update()`, called once per frame. It only iterates the 150x150 rect around the player — not the full 1024x1024 grid.

## Rendering on the World Map

The world map texture is baked in `map_window.c`'s `generate_texture()` when the M key is pressed. FoW integrates directly into this bake:

```c
for each cell (x, y):
    if (!FogOfWar_is_revealed(x, y)):
        pixel = black (0, 0, 0, 255)
    else:
        pixel = normal cell color (existing logic)
```

That's it. No shaders, no FBOs, no GPU textures for FoW. The existing world map texture bake already iterates every cell — we just add a revealed check.

### Soft Edges

For a nicer look at fog boundaries, blend cells that border unrevealed areas. During the texture bake, if a revealed cell has any unrevealed neighbor (8-connected), darken it partially:

```c
if revealed[x][y]:
    if any_unrevealed_neighbor(x, y):
        pixel = dimmed cell color (50% brightness)
    else:
        pixel = normal cell color
else:
    pixel = black
```

This gives a one-cell-wide dim border at the exploration frontier without any GPU work. Simple and effective.

### Overlay Dots

Savepoint, portal, and player dots already render on top of the texture. These should also respect FoW:
- **Player dot**: always visible (you know where you are)
- **Savepoints/portals**: only render if `FogOfWar_is_revealed(grid_x, grid_y)` — discovering POIs is part of exploration

## New Files

### `src/fog_of_war.h`
```c
void FogOfWar_initialize(void);   // no-op (static array, zero-init is enough)
void FogOfWar_cleanup(void);      // no-op
void FogOfWar_reset(void);        // clear all revealed state
void FogOfWar_update(Position player_pos);  // reveal cells within minimap range
void FogOfWar_reveal_all(void);   // god mode: mark everything revealed
bool FogOfWar_is_revealed(int gx, int gy);  // query for world map rendering
void FogOfWar_save_to_file(void);   // write revealed[][] to ./save/fog_of_war.bin
void FogOfWar_load_from_file(void); // restore revealed[][] from disk (no-op if missing)
```

### `src/fog_of_war.c`
- Static `revealed[MAP_SIZE][MAP_SIZE]` array
- `FogOfWar_update`: convert player world pos to grid coords, compute radius from `Hud_get_minimap_range()`, iterate rect, mark cells `true`
- `FogOfWar_is_revealed`: bounds-check + array lookup
- `FogOfWar_reveal_all`: `memset(revealed, true, ...)`
- `FogOfWar_reset`: `memset(revealed, false, ...)`

No GL resources. Pure CPU state.

## Modified Files

### `hud.c` / `hud.h`
- Expose `Hud_get_minimap_range()` returning `RADAR_RANGE` (currently 15000.0f)

### `map_window.c`
- In `generate_texture()`: check `FogOfWar_is_revealed(x, y)` per cell. Unrevealed = black pixel. Revealed with unrevealed neighbor = dimmed. Revealed = normal.
- In savepoint/portal dot rendering: skip dots where `!FogOfWar_is_revealed(grid_x, grid_y)`

### `mode_gameplay.c`
- Call `FogOfWar_update(Ship_get_position())` in update phase (skip while ship is destroyed — dead player doesn't reveal)
- Call `FogOfWar_initialize()` / `FogOfWar_cleanup()` in lifecycle
- In `Mode_Gameplay_initialize_from_save()`: call `FogOfWar_load_from_file()` after `Zone_load()` to restore saved exploration state
- In cross-zone death respawn: call `FogOfWar_load_from_file()` after `zone_teardown_and_load()` to restore FoW when respawning back at checkpoint zone

### `zone.c`
- Call `FogOfWar_reset()` in `Zone_load()` (after `apply_zone_to_world()`) and `Zone_unload()`
- Call `FogOfWar_reset()` in `Zone_regenerate_procgen()`

### `savepoint.c`
- `#include "fog_of_war.h"`
- In `do_save()`: call `FogOfWar_save_to_file()` after writing the checkpoint `.sav` file
- In `Savepoint_delete_save_file()`: also `remove("./save/fog_of_war.bin")` to clean up FoW data

### `settings.c`
- Add "Fog of War" toggle (default: ON)
- When OFF, `FogOfWar_is_revealed()` always returns `true` (world map shows everything)
- `FogOfWar_update()` still runs when disabled (so toggling back ON shows correct state)

### God mode
- `FogOfWar_reveal_all()` called when god mode is toggled ON (editing needs full visibility)

## Edge Cases

- **Player at map edge**: reveal rect clamped to `[0, MAP_SIZE)` — standard bounds check
- **Zone size < MAP_SIZE**: cells outside zone size stay fogged, but player can't reach them
- **World map opened before moving**: only the spawn area (within minimap range) is revealed
- **Procgen regen**: fog resets since terrain layout changed — player re-explores

## Performance

- **CPU**: 150x150 = 22,500 cell checks per frame (trivial — mostly already `true`)
- **Texture bake**: same 1024x1024 iteration as before, just with an `if` check per cell
- **Memory**: ~1MB static array (no GPU resources)
- **Zero rendering cost**: FoW is baked into the existing world map texture, not a separate pass

## Not In Scope

- FoW on the gameplay view (intentionally unaffected)
- FoW on the minimap display (intentionally unaffected)
- Minimap zoom controls (separate future spec — when added, reveal radius auto-adapts)
- Enemy vision / detection tied to FoW
