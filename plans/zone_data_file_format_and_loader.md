# Plan: Zone Data File Format and Loader

## Context

The game world is currently hardcoded — `map.c` places ~70 cells via `set_map_cell()` calls and `mode_gameplay.c` spawns ~40 mines at hardcoded positions. This needs to become data-driven so that God Mode can persist edits, zones can be authored without code changes, and the game can support 12+ distinct zones.

This is the foundation for God Mode, the zone editor, and the entire zone/portal system.

## Zone File Format

Line-based text format (`.zone`), one entry per line, `#` for comments. See PRD "Zone Data Files" section for full format spec with examples.

**Entry types**: `name`, `size`, `celltype`, `cell`, `spawn`, `portal`

## New Files

### `src/zone.h`

```c
#define ZONE_MAX_CELL_TYPES 32
#define ZONE_MAX_SPAWNS 256
#define ZONE_MAX_UNDO 512

typedef struct {
    char id[32];             // e.g. "solid", "circuit", "fire_wall"
    ColorRGB primaryColor;
    ColorRGB outlineColor;
    char pattern[16];        // "none", "circuit", future patterns
} ZoneCellType;

typedef struct {
    char enemy_type[16];     // e.g. "mine"
    double world_x, world_y;
} ZoneSpawn;

typedef struct {
    char name[64];
    int size;                // map grid size (e.g. 128)
    char filepath[256];

    ZoneCellType cell_types[ZONE_MAX_CELL_TYPES];
    int cell_type_count;

    // Cell grid — which type ID at each position (-1 = empty)
    // Dynamically sized based on zone size, but max MAP_SIZE
    int cell_grid[MAP_SIZE][MAP_SIZE];

    ZoneSpawn spawns[ZONE_MAX_SPAWNS];
    int spawn_count;
} Zone;

void Zone_load(const char *path);
void Zone_unload(void);
void Zone_save(void);
const Zone *Zone_get(void);

// Editing API (for God Mode)
void Zone_place_cell(int grid_x, int grid_y, const char *type_id);
void Zone_remove_cell(int grid_x, int grid_y);
void Zone_place_spawn(const char *enemy_type, double world_x, double world_y);
void Zone_remove_spawn(int index);
void Zone_undo(void);
```

### `src/zone.c`

**Zone_load(path)**:
1. Open file, read line by line
2. Parse `name`, `size` metadata
3. Parse `celltype` lines → populate `cell_types[]`
4. Parse `cell` lines → populate `cell_grid[][]`
5. Parse `spawn` lines → populate `spawns[]`
6. Close file, store filepath for save
7. Call `Map_clear()` to reset the map grid
8. Call `Map_resize(size)` to set active map size
9. Iterate `cell_grid`, call `Map_set_cell(x, y, &mapCell)` for each non-empty cell
10. Iterate `spawns`, call `Mine_initialize(position)` for each mine spawn

**Zone_unload()**:
1. Call `Map_clear()` — reset all cells to empty
2. Call `Mine_cleanup()` — destroy all mines
3. Zero out the Zone struct

**Zone_save()**:
1. Open filepath for writing
2. Write `name` and `size` lines
3. Write all `celltype` entries
4. Iterate `cell_grid`, write `cell` line for each non-empty cell
5. Write all `spawn` entries
6. Close file

**Undo system**:
- Static array of undo entries, each recording the action type (place_cell, remove_cell, place_spawn, remove_spawn) and the data needed to reverse it
- `Zone_undo()` pops the last entry, applies the reverse operation, calls `Zone_save()`

**Parsing**: Simple `sscanf`/`strcmp` per line. No external libraries needed.

## Files to Modify

### `src/map.c` / `src/map.h`

Need to expose cell manipulation for the zone loader:

- **Add `Map_clear(void)`**: Reset all cells to `&emptyCell`, called by zone loader before populating
- **Add `Map_set_cell(int grid_x, int grid_y, MapCell *cell)`**: Public version using 0-indexed grid coordinates (no centered coordinate quirks). The zone loader and God Mode use this.
- **Add `Map_clear_cell(int grid_x, int grid_y)`**: Set a cell back to empty
- **Add `Map_get_cell(int grid_x, int grid_y)`**: Read a cell (for save/serialization)
- **MapCell storage**: Currently cells are pointers to static singletons (`cell001`, `cell002`). Zone-defined cell types need dynamically created MapCell instances. Add a cell type pool in map.c or allocate them in zone.c and pass pointers.

The old `set_map_cell` with its centered coordinates stays for backwards compatibility during transition but is no longer called by zone loading.

### `src/mine.c` / `src/mine.h`

- **Add `Mine_cleanup(void)`**: Destroy all mine entities, reset `highestUsedIndex = 0`. Called by `Zone_unload()`.
- `Mine_initialize(Position)` already exists and works — zone loader calls it directly.

### `src/mode_gameplay.c`

- Remove all hardcoded `Mine_initialize()` calls (~40 lines)
- Remove the call chain that builds the map in `Map_initialize()` (or leave Map_initialize as a no-op that just clears)
- Replace with `Zone_load("./resources/zones/zone_001.zone")`
- Add `Zone_unload()` to cleanup

### `src/map.c` initialization

- `Map_initialize()` currently calls `set_map_cell()` ~70 times to build the hardcoded layout. This should be gutted — `Map_initialize()` becomes just `Map_clear()` (set everything to empty). The zone loader populates cells after.

## Migration: Hardcoded Zone → zone_001.zone

Create `resources/zones/zone_001.zone` that reproduces the current hardcoded layout:
- Translate the ~70 `set_map_cell()` calls to `cell` lines with 0-indexed grid coordinates
- Translate the ~40 `Mine_initialize()` calls to `spawn mine` lines
- Define `celltype solid` and `celltype circuit` with current colors

This file becomes the reference zone and proves the format works.

## Steps

1. Add `Map_clear()`, `Map_set_cell()`, `Map_clear_cell()`, `Map_get_cell()` to map.c/h
2. Add `Mine_cleanup()` to mine.c/h
3. Create `src/zone.h` — Zone struct and API declarations
4. Create `src/zone.c` — load, unload, save, parsing, editing, undo
5. Create `resources/zones/zone_001.zone` — migrated hardcoded zone
6. Gut `Map_initialize()` — just `Map_clear()`, no hardcoded cells
7. Modify `mode_gameplay.c` — replace hardcoded spawns with `Zone_load()`
8. `make compile` and test

## Verification

1. `make compile` — clean build, no warnings
2. Game loads `zone_001.zone` and the world looks identical to the current hardcoded layout
3. All mines spawn at correct positions
4. All map cells render correctly (solid and circuit types)
5. Collision still works (ship-wall, projectile-wall, ship-mine)
6. `Zone_save()` writes a file that `Zone_load()` can round-trip (save then load produces identical state)
7. Manually edit zone_001.zone (add/remove a cell), reload — change is reflected in-game

---

## Post-Implementation Addendum: MapCell Border Rendering Fix

**Problem**: The original plan called for a `type_id` field on `MapCell` so the border rendering macro (`EDGE_DRAW`) could compare cell types. The old code relied on pointer identity (`ptr != map[x][y]`) which worked with static singletons but broke with the cell pool (every cell gets a unique pointer).

**Decision**: Removed `type_id` from `MapCell`. Instead, the renderer compares visual properties directly via a `CELLS_MATCH` macro:

```c
#define CELLS_MATCH(a, b) \
    ((a)->circuitPattern == (b)->circuitPattern && \
     memcmp(&(a)->primaryColor, &(b)->primaryColor, sizeof(ColorRGB)) == 0 && \
     memcmp(&(a)->outlineColor, &(b)->outlineColor, sizeof(ColorRGB)) == 0)
```

**Rationale**: MapCell should be a pure visual/physical description with no coupling to the zone type system. This keeps the rendering concern in the renderer and allows gameplay events to mutate individual cells (damage, dissolve, transform) without type bookkeeping — borders update naturally based on what cells look like, not what they were created from.
