# Spec: Phase 1 — Godmode Entity Placement + Zone Navigation ✅ COMPLETE

## Summary

Expand godmode from cell-only editing to full entity placement (enemies, savepoints, portals), add zone navigation (create new zones, jump between zones without portals), and make background colors and music per-zone properties defined in the zone file.

**Status**: Implemented. See `plans/spec_procgen_phases.md` for Phase 1.5 (potential additional godmode tools for procgen authoring — to be evaluated after Phase 2).

## What Already Exists

| System | Status | Key Files |
|--------|--------|-----------|
| Godmode cell editing | Working | `mode_gameplay.c` (O key, LMB/RMB, Tab cycles type) |
| Portal entities | Working | `portal.c/h` (dwell timer, zone transitions, labels) |
| Savepoint entities | Working | `savepoint.c/h` (checkpoint save/load, disk persistence) |
| Zone file I/O | Working | `zone.c/h` (load/save portal, savepoint, spawn lines) |
| Spawn editing API | Working | `Zone_place_spawn()` / `Zone_remove_spawn()` with undo |
| Zone transitions | Working | `mode_gameplay.c` warp sequence (WARP_PULL → FLASH → ARRIVE) |
| Available zones | 2 | `zone_001.zone` (The Origin), `zone_002.zone` (Data Nexus) |
| Background | Hardcoded | `background.c` — 4 purple palette colors, same for all zones |
| Music | Hardcoded | `mode_gameplay.c` — 7 tracks, random pick, infinite loop via `Audio_loop_music()` |

## Architecture

### Godmode Placement Modes

Current godmode only handles cells. Add a **mode** concept that determines what LMB/RMB/Tab do:

```c
typedef enum {
    GOD_MODE_CELLS,       // Current behavior — place/remove map cells
    GOD_MODE_ENEMIES,     // Place/remove enemy spawns
    GOD_MODE_SAVEPOINTS,  // Place/remove savepoints
    GOD_MODE_PORTALS,     // Place/remove portals
    GOD_MODE_COUNT
} GodPlacementMode;

static GodPlacementMode godPlacementMode = GOD_MODE_CELLS;
```

### Key Bindings

All within godmode (O key toggle unchanged):

| Key | Action |
|-----|--------|
| **Q / E** | Cycle placement mode (prev / next) |
| **Tab** | Cycle sub-type within mode (cell type, enemy type, etc.) |
| **LMB** | Place entity at cursor |
| **RMB** | Remove entity at cursor |
| **Ctrl+Z** | Undo |
| **Ctrl+S** | Explicit save (in addition to auto-save on edit) |
| **N** | Create new zone (prompts for name) |
| **J** | Open zone jump menu |
| **WASD** | Pan camera |
| **Scroll** | Zoom |

### Sub-type Cycling (Tab key)

Each mode has its own sub-type selection:

| Mode | Tab cycles through | State variable |
|------|-------------------|----------------|
| Cells | `zone.cell_types[0..n]` | `godModeSelectedType` (existing) |
| Enemies | `"mine", "hunter", "seeker", "defender"` | `godEnemyType` (new) |
| Savepoints | N/A (single type) | — |
| Portals | N/A (single type) | — |

```c
static const char *ENEMY_TYPES[] = {"mine", "hunter", "seeker", "defender"};
#define ENEMY_TYPE_COUNT 4
static int godEnemyType = 0;
```

---

## Implementation: Zone Editing APIs

### New Functions in zone.c

**Portal placement:**

```c
void Zone_place_portal(int grid_x, int grid_y,
    const char *id, const char *dest_zone, const char *dest_portal_id);
void Zone_remove_portal(int grid_x, int grid_y);
```

- `Zone_place_portal`: Adds to `zone.portals[]`, pushes undo, spawns via `Portal_initialize()`, saves.
- `Zone_remove_portal`: Finds portal at grid position, pushes undo, calls `apply_zone_to_world()`, saves.
- Auto-generate ID at call site: `"portal_N"` where N = `zone.portal_count + 1`. Destination starts empty (`""`) — configured by editing zone file or future linking UI.

**Savepoint placement:**

```c
void Zone_place_savepoint(int grid_x, int grid_y, const char *id);
void Zone_remove_savepoint(int grid_x, int grid_y);
```

- Same pattern as portal. Auto-generate ID: `"save_N"`.

**New undo types:**

```c
typedef enum {
    UNDO_PLACE_CELL,
    UNDO_REMOVE_CELL,
    UNDO_PLACE_SPAWN,
    UNDO_REMOVE_SPAWN,
    UNDO_PLACE_PORTAL,      // NEW
    UNDO_REMOVE_PORTAL,     // NEW
    UNDO_PLACE_SAVEPOINT,   // NEW
    UNDO_REMOVE_SAVEPOINT   // NEW
} UndoType;
```

The undo entry union needs new fields:

```c
typedef struct {
    UndoType type;
    int cell_x, cell_y;
    int type_index;
    ZoneSpawn spawn;
    int spawn_index;
    ZonePortal portal;        // NEW — full portal data for undo
    int portal_index;         // NEW
    struct {                  // NEW — savepoint data for undo
        int grid_x, grid_y;
        char id[32];
    } savepoint;
    int savepoint_index;      // NEW
} UndoEntry;
```

### Removal by Grid Position

Portals and savepoints are stored at grid coordinates — removal matches by `grid_x, grid_y`. Enemy spawns are stored in world coordinates — removal needs to find the nearest spawn to the cursor's world position within a tolerance (e.g., `MAP_CELL_SIZE`).

```c
// In zone.c — find spawn nearest to world position, returns index or -1
int Zone_find_spawn_near(double world_x, double world_y, double radius);
```

---

## Implementation: Godmode Update Logic

### Mode Switching

In `god_mode_update()`:

```c
// Q = prev mode, E = next mode
if (input->keyQ) {
    godPlacementMode = (godPlacementMode + GOD_MODE_COUNT - 1) % GOD_MODE_COUNT;
}
if (input->keyE) {
    godPlacementMode = (godPlacementMode + 1) % GOD_MODE_COUNT;
}
```

### Tab Cycling Per Mode

```c
if (input->keyTab) {
    switch (godPlacementMode) {
    case GOD_MODE_CELLS:
        godModeSelectedType = (godModeSelectedType + 1) % z->cell_type_count;
        break;
    case GOD_MODE_ENEMIES:
        godEnemyType = (godEnemyType + 1) % ENEMY_TYPE_COUNT;
        break;
    default:
        break; // savepoints/portals have no sub-types
    }
}
```

### LMB Placement Per Mode

```c
if (input->mouseLeft && godModeCursorValid) {
    switch (godPlacementMode) {
    case GOD_MODE_CELLS:
        // Existing cell placement logic
        Zone_place_cell(grid_x, grid_y, z->cell_types[godModeSelectedType].id);
        break;

    case GOD_MODE_ENEMIES: {
        double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        Zone_place_spawn(ENEMY_TYPES[godEnemyType], wx, wy);
        break;
    }

    case GOD_MODE_SAVEPOINTS: {
        // Check no savepoint already at this grid position
        char id[32];
        snprintf(id, sizeof(id), "save_%d", zone.savepoint_count + 1);
        Zone_place_savepoint(grid_x, grid_y, id);
        break;
    }

    case GOD_MODE_PORTALS: {
        // Check no portal already at this grid position
        char id[32];
        snprintf(id, sizeof(id), "portal_%d", zone.portal_count + 1);
        Zone_place_portal(grid_x, grid_y, id, "", "");
        break;
    }
    }
}
```

### RMB Removal Per Mode

```c
if (input->mouseRight && godModeCursorValid) {
    switch (godPlacementMode) {
    case GOD_MODE_CELLS:
        Zone_remove_cell(grid_x, grid_y);
        break;

    case GOD_MODE_ENEMIES: {
        double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        int idx = Zone_find_spawn_near(wx, wy, MAP_CELL_SIZE);
        if (idx >= 0) Zone_remove_spawn(idx);
        break;
    }

    case GOD_MODE_SAVEPOINTS:
        Zone_remove_savepoint(grid_x, grid_y);
        break;

    case GOD_MODE_PORTALS:
        Zone_remove_portal(grid_x, grid_y);
        break;
    }
}
```

---

## Implementation: Cursor Rendering

### Per-Mode Cursor Appearance

The cursor should visually indicate what's being placed:

| Mode | Cursor Visual |
|------|--------------|
| Cells | Current behavior — colored outline of cell type |
| Enemies | Red/orange outline + enemy type icon (simple triangle/diamond/etc.) |
| Savepoints | Green outline + small diamond icon |
| Portals | Blue/purple outline + double-bar icon |

Keep it simple for v1 — just change the cursor outline color per mode. The enemy sub-type is shown in the HUD text.

```c
static void god_mode_render_cursor(void) {
    // Color per mode
    float cr, cg, cb;
    switch (godPlacementMode) {
    case GOD_MODE_CELLS:
        // Use cell type's outline color (existing)
        ...
        break;
    case GOD_MODE_ENEMIES:
        cr = 1.0f; cg = 0.3f; cb = 0.2f; // Red-orange
        break;
    case GOD_MODE_SAVEPOINTS:
        cr = 0.2f; cg = 1.0f; cb = 0.3f; // Green
        break;
    case GOD_MODE_PORTALS:
        cr = 0.5f; cg = 0.3f; cb = 1.0f; // Purple
        break;
    }
    // Render outline quad at cursor in that color
}
```

### HUD Updates

`god_mode_render_hud()` shows:

```
GOD MODE
Mode: Enemies
Type: hunter
Grid: (576, 512)
Zone: The Origin (zone_001.zone)
```

Mode name from: `{"Cells", "Enemies", "Savepoints", "Portals"}`.
Sub-type only shown for cells and enemies.
**Grid coordinates** always shown when cursor is valid — shows `godModeCursorX, godModeCursorY`. Essential for precise placement and for setting portal destinations by hand (you need to know the grid coords to reference in another zone file).
Zone name always shown (helps when jumping between zones).

### Snap to Grid Intersections

Entity placement (enemies, savepoints, portals) snaps to **grid intersection points** (cell corners), not cell centers. This matches how existing spawns are placed — all at clean multiples of `MAP_CELL_SIZE` with no `+50` offset. Intersections are natural landmark positions where 2-4 cells meet.

```c
// Grid intersection = corner of cell at (grid_x, grid_y)
double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
```

Cell placement remains as-is (fills the cell, not an intersection point). The cursor for entity modes should visually indicate the intersection point (e.g., crosshair at the corner) rather than highlighting the full cell quad.

---

## Implementation: Zone Navigation

### Zone Jumping

**Key: J** opens a simple zone selector.

Implementation approach: scan `resources/zones/` for `.zone` files, display numbered list in the HUD, number keys select.

```c
// State
static bool godZoneMenuOpen = false;
static char godZoneFiles[16][256];   // Up to 16 zone paths
static char godZoneNames[16][64];    // Display names
static int godZoneFileCount = 0;
```

**Scanning for zones** (on J press):

```c
// Use opendir/readdir to list resources/zones/*.zone
// Parse first "name" line from each for display
// Store paths in godZoneFiles[]
```

**Rendering the menu:**

```
=== ZONE JUMP ===
1: The Origin (zone_001.zone)
2: Data Nexus (zone_002.zone)
3: Fire Zone (zone_003.zone)
```

**Selecting** (number key while menu open):

```c
if (godZoneMenuOpen && input->keySlot >= 0 && input->keySlot < godZoneFileCount) {
    god_mode_jump_to_zone(godZoneFiles[input->keySlot]);
    godZoneMenuOpen = false;
}
```

**Zone jump function:**

```c
static void god_mode_jump_to_zone(const char *zone_path) {
    // Clean teardown
    Sub_Pea_cleanup();
    Sub_Mgun_cleanup();
    Sub_Mine_cleanup();
    Fragment_deactivate_all();
    Zone_unload();
    Entity_destroy_all();

    // Re-initialize core systems
    Grid_initialize();
    Map_initialize();
    Ship_initialize();

    // Load target zone
    Zone_load(zone_path);
    Destructible_initialize();

    // Place ship at origin (or first savepoint if exists)
    Position spawn = {0.0, 0.0};
    // Check for savepoint
    const Zone *z = Zone_get();
    if (z->savepoint_count > 0) {
        spawn.x = (z->savepoints[0].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        spawn.y = (z->savepoints[0].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
    }
    Ship_force_spawn_at(spawn);

    // Restore god mode state
    Ship_set_god_mode(true);
    godPlacementMode = GOD_MODE_CELLS;
    godModeSelectedType = 0;
}
```

**Note:** Uses the shared `zone_teardown_and_load()` helper extracted from `warp_do_zone_swap()` (see below).

### New Zone Creation

**Key: N** creates a new zone.

For v1, use a simple naming scheme — auto-increment from existing files:

```c
static void god_mode_create_zone(void) {
    // Find next available zone number
    // e.g., if zone_001 and zone_002 exist, create zone_003

    // Write minimal zone file:
    // name New Zone
    // size 1024
    // (copy cell types from current zone for palette continuity)
    // celltype solid ...
    // celltype circuit ...

    // Save file to resources/zones/zone_NNN.zone
    // Then jump to it
    god_mode_jump_to_zone(new_path);
}
```

**Copy cell types from current zone:** When creating a new zone, copy the current zone's `celltype` definitions. This way the palette matches and you can immediately start painting. The designer can modify them later by editing the zone file.

---

## Implementation: Extract Shared Zone Reload Helper

### Current Duplication

`warp_do_zone_swap()` in `mode_gameplay.c` performs a 9-step teardown/load/setup sequence. The godmode zone jump needs the same core teardown + load steps (1-5). Rather than duplicate this, extract the shared portion into a reusable helper.

### Extracted Helper

```c
// In mode_gameplay.c (static, same file — both warp and godmode live here)
static void zone_teardown_and_load(const char *zone_path)
{
    // 1. Cleanup active subs and fragments
    Sub_Pea_cleanup();
    Sub_Mgun_cleanup();
    Sub_Mine_cleanup();
    Fragment_deactivate_all();

    // 2. Unload current zone (clears map, enemies, portals, savepoints)
    Zone_unload();

    // 3. Destroy all entities
    Entity_destroy_all();

    // 4. Re-initialize base systems
    Grid_initialize();
    Map_initialize();
    Ship_initialize();

    // 5. Load destination zone
    Zone_load(zone_path);
    Destructible_initialize();
}
```

### Callers

**`warp_do_zone_swap()`** becomes:

```c
static void warp_do_zone_swap(void)
{
    zone_teardown_and_load(warpDestZone);

    // Warp-specific: find destination portal, spawn there
    Position arrival = {0.0, 0.0};
    if (!Portal_get_position_by_id(warpDestPortalId, &arrival))
        printf("Warp: destination portal '%s' not found\n", warpDestPortalId);
    Ship_force_spawn(arrival);

    // Restore player state from warp snapshot
    PlayerStats_restore(warpStatsSnap);
    Skillbar_restore(warpSkillSnap);

    // Suppress re-trigger at arrival portal
    Portal_suppress_arrival(warpDestPortalId);

    // Start zone BGM
    start_zone_bgm();
}
```

**`god_mode_jump_to_zone()`** becomes:

```c
static void god_mode_jump_to_zone(const char *zone_path)
{
    zone_teardown_and_load(zone_path);

    // Godmode-specific: spawn at first savepoint or origin
    Position spawn = {0.0, 0.0};
    const Zone *z = Zone_get();
    if (z->savepoint_count > 0) {
        spawn.x = (z->savepoints[0].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
        spawn.y = (z->savepoints[0].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
    }
    Ship_force_spawn(spawn);

    // Restore god mode
    Ship_set_god_mode(true);
    godPlacementMode = GOD_MODE_CELLS;
    godModeSelectedType = 0;

    // Start zone BGM
    start_zone_bgm();
}
```

Both callers handle spawn position, player state, and BGM themselves since those differ between warp and godmode. The teardown/load sequence is identical.

---

## Implementation: Zone API Additions

### Zone_get() Accessor

Already exists — returns `const Zone*` for read access.

### Zone_place_portal / Zone_remove_portal

```c
void Zone_place_portal(int grid_x, int grid_y,
    const char *id, const char *dest_zone, const char *dest_portal_id)
{
    if (zone.portal_count >= ZONE_MAX_PORTALS) return;

    // Push undo
    UndoEntry undo;
    undo.type = UNDO_REMOVE_PORTAL;
    undo.portal_index = zone.portal_count;
    push_undo(undo);

    // Add to zone data
    ZonePortal *p = &zone.portals[zone.portal_count];
    p->grid_x = grid_x;
    p->grid_y = grid_y;
    strncpy(p->id, id, 31);
    strncpy(p->dest_zone, dest_zone, 255);
    strncpy(p->dest_portal_id, dest_portal_id, 31);
    zone.portal_count++;

    // Spawn in world
    double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
    double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
    Position pos = {wx, wy};
    Portal_initialize(pos, id, dest_zone, dest_portal_id);

    Zone_save();
}

void Zone_remove_portal(int grid_x, int grid_y)
{
    // Find portal at this grid position
    int index = -1;
    for (int i = 0; i < zone.portal_count; i++) {
        if (zone.portals[i].grid_x == grid_x &&
            zone.portals[i].grid_y == grid_y) {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    // Push undo with full portal data
    UndoEntry undo;
    undo.type = UNDO_PLACE_PORTAL;
    undo.portal = zone.portals[index];
    undo.portal_index = index;
    push_undo(undo);

    // Shift portals down
    for (int i = index; i < zone.portal_count - 1; i++)
        zone.portals[i] = zone.portals[i + 1];
    zone.portal_count--;

    // Rebuild world
    apply_zone_to_world();
    Zone_save();
}
```

### Zone_place_savepoint / Zone_remove_savepoint

Same pattern as portal, using `zone.savepoints[]`. The savepoint struct is anonymous in Zone — may want to extract a named `ZoneSavepoint` typedef for the undo entry.

### Zone_find_spawn_near

```c
int Zone_find_spawn_near(double world_x, double world_y, double radius)
{
    double best_dist = radius * radius;
    int best_index = -1;
    for (int i = 0; i < zone.spawn_count; i++) {
        double dx = zone.spawns[i].world_x - world_x;
        double dy = zone.spawns[i].world_y - world_y;
        double d2 = dx * dx + dy * dy;
        if (d2 < best_dist) {
            best_dist = d2;
            best_index = i;
        }
    }
    return best_index;
}
```

---

## Implementation: Existing Entity Labels in Godmode

Portal and savepoint already render world-space labels when godmode is active:

```c
// Already called in mode_gameplay.c render:
if (godModeActive) {
    Portal_render_god_labels();
    Savepoint_render_god_labels();
}
```

Need to add similar labels for enemy spawns — render enemy type text at each spawn position. Add to `mode_gameplay.c`'s godmode render section:

```c
if (godModeActive) {
    god_mode_render_spawn_labels();  // NEW — iterate zone.spawns[], render type text
    Portal_render_god_labels();
    Savepoint_render_god_labels();
}
```

---

## Implementation: Per-Zone Background Colors

### Current State

`background.c` has a hardcoded 4-color palette used for all cloud blob colors:

```c
static const float palette[][3] = {
    {0.35f, 0.10f, 0.55f},  /* violet */
    {0.20f, 0.10f, 0.50f},  /* blue-purple */
    {0.45f, 0.08f, 0.40f},  /* red-purple */
    {0.15f, 0.05f, 0.35f},  /* dark violet */
};
```

Each blob randomly picks one of the 4 colors at init time. The palette defines the zone's visual mood.

### Zone File Format

Add `bgcolor` lines near the top of zone files (after `size`, before `celltype`):

```
bgcolor 0 89 26 140    # violet (R G B, 0-255)
bgcolor 1 51 26 128    # blue-purple
bgcolor 2 115 20 102   # red-purple
bgcolor 3 38 13 89     # dark violet
```

Four colors, indices 0-3. If omitted, fall back to the current hardcoded defaults.

### Zone Data

Add to Zone struct in `zone.h`:

```c
bool has_bg_colors;
ColorRGB bg_colors[4];
```

### Background API

Add a function to set the palette from outside:

```c
// background.h
void Background_set_palette(const float colors[4][3]);

// background.c — replace static palette with a mutable one
static float palette[4][3] = {
    {0.35f, 0.10f, 0.55f},
    {0.20f, 0.10f, 0.50f},
    {0.45f, 0.08f, 0.40f},
    {0.15f, 0.05f, 0.35f},
};

void Background_set_palette(const float colors[4][3])
{
    memcpy(palette, colors, sizeof(palette));
}
```

Call `Background_set_palette()` from `apply_zone_to_world()` when loading a zone with custom colors. Then call `Background_initialize()` to regenerate clouds with the new palette. The order matters: set palette → initialize.

---

## Implementation: Per-Zone Music Playlist

### Current State

`mode_gameplay.c` has 7 hardcoded music paths. On zone entry, one is randomly selected and looped infinitely via `Audio_loop_music()` (which calls `Mix_PlayMusic(music, -1)`).

### Zone File Format

Add `music` lines near the top of zone files:

```
music ./resources/music/zone1_track1.mp3
music ./resources/music/zone1_track2.mp3
music ./resources/music/zone1_track3.mp3
```

Up to 3 tracks, played sequentially as a playlist. After the third finishes, loop back to the first. If no `music` lines are present, fall back to the current random-from-global-pool behavior.

### Zone Data

Add to Zone struct in `zone.h`:

```c
#define ZONE_MAX_MUSIC 3
char music_paths[ZONE_MAX_MUSIC][256];
int music_count;
```

### Parsing (zone.c)

```c
else if (strncmp(line, "music ", 6) == 0) {
    if (zone.music_count < ZONE_MAX_MUSIC) {
        strncpy(zone.music_paths[zone.music_count],
                line + 6, 255);
        zone.music_paths[zone.music_count][255] = '\0';
        // Trim trailing whitespace/newline
        zone.music_count++;
    }
}
```

### Playlist System (mode_gameplay.c)

Replace the single-track infinite loop with a sequential playlist:

```c
// Zone music state
static int zoneMusicIndex = 0;       // Current track in playlist (0-2)
static int zoneMusicCount = 0;       // Number of tracks for this zone
static char zoneMusicPaths[3][256];  // Copied from zone data on load
static bool useZoneMusic = false;    // false = fallback to global pool

static void start_zone_bgm(void)
{
    if (useZoneMusic && zoneMusicCount > 0) {
        zoneMusicIndex = 0;
        Audio_play_music(zoneMusicPaths[0]);  // Play once (not loop)
    } else {
        // Fallback: random pick from global pool, loop
        Audio_loop_music(bgm_paths[selectedBgm]);
    }
}
```

### Track Advancement

Use SDL_Mixer's `Mix_HookMusicFinished()` callback to detect when a track ends:

```c
// Called by SDL_Mixer when the current music track finishes
static void on_music_finished(void)
{
    if (!useZoneMusic || zoneMusicCount == 0) return;
    zoneMusicIndex = (zoneMusicIndex + 1) % zoneMusicCount;
    // Can't call Mix_PlayMusic from callback — set a flag
    zoneMusicAdvance = true;
}
```

In the gameplay update loop, check the flag:

```c
static bool zoneMusicAdvance = false;

// In gameplay update:
if (zoneMusicAdvance) {
    zoneMusicAdvance = false;
    Audio_play_music(zoneMusicPaths[zoneMusicIndex]);
}
```

**Important**: `Mix_HookMusicFinished` callbacks must not call SDL_Mixer functions directly. The flag approach defers the actual play call to the main update loop.

### Audio API Addition

Add a non-looping play function:

```c
// audio.h
void Audio_play_music(const char *path);

// audio.c
void Audio_play_music(const char *path)
{
    if (music) Mix_FreeMusic(music);
    music = Mix_LoadMUS(path);
    Mix_PlayMusic(music, 0);  // 0 = play once (no loop)
}
```

### Zone Load Integration

When a zone is loaded (in `apply_zone_to_world` or `start_zone_bgm` setup):

```c
const Zone *z = Zone_get();
if (z->music_count > 0) {
    useZoneMusic = true;
    zoneMusicCount = z->music_count;
    for (int i = 0; i < z->music_count; i++)
        strncpy(zoneMusicPaths[i], z->music_paths[i], 255);
} else {
    useZoneMusic = false;
    selectedBgm = rand() % 7;
}
```

### Zone Save

Write `music` lines in `Zone_save()`:

```c
for (int i = 0; i < zone.music_count; i++)
    fprintf(f, "music %s\n", zone.music_paths[i]);
```

---

## File Changes Summary

| File | Changes |
|------|---------|
| `mode_gameplay.c` | Placement modes, mode switching, per-mode cursor/HUD, zone menu, zone jump, new zone creation, spawn labels, music playlist system |
| `zone.c` | `Zone_place_portal`, `Zone_remove_portal`, `Zone_place_savepoint`, `Zone_remove_savepoint`, `Zone_find_spawn_near`, new undo types, parse `bgcolor`/`music` lines, write them in save |
| `zone.h` | New function declarations, `ZoneSavepoint` typedef, undo type enum updates, `bg_colors[4]`, `music_paths[3]`, `music_count` |
| `background.c/h` | `Background_set_palette()` — make palette mutable, accept 4 colors from zone |
| `audio.c/h` | `Audio_play_music()` — non-looping variant of `Audio_loop_music()` |
| `input.c/h` | Add `keyN`, `keyJ` if not already captured |

### Input Keys Needed

Check if `N` and `J` are already in the Input struct. If not, add:
```c
bool keyN;    // New zone
bool keyJ;    // Zone jump menu
```

---

## Implementation Order

1. **Per-zone background colors** — `bgcolor` parsing in zone.c, `Background_set_palette()`, wire into zone load. Quick win with immediate visual payoff.
2. **Per-zone music playlist** — `music` parsing, `Audio_play_music()`, `Mix_HookMusicFinished` callback, playlist advancement. Test with existing zone files.
3. **Zone editing APIs** — `Zone_place_portal`, `Zone_remove_portal`, `Zone_place_savepoint`, `Zone_remove_savepoint`, `Zone_find_spawn_near`, new undo types. Test from code before wiring to UI.
4. **Placement mode framework** — Mode enum, state variables, Q/E switching, Tab cycling per mode, HUD updates.
5. **Per-mode placement** — Wire LMB/RMB to correct Zone_* calls per mode. Cursor color per mode.
6. **Spawn labels** — Render enemy type text at spawn positions in godmode.
7. **Zone jumping** — Zone file scanner, J key menu, number key selection, jump function.
8. **New zone creation** — N key, auto-name, copy cell types, jump to new zone.

---

## Testing Checklist

- [ ] Place and remove each entity type (cell, enemy, savepoint, portal)
- [ ] Undo works for all entity types
- [ ] Placing an enemy immediately spawns it in the world
- [ ] Removing an enemy despawns it
- [ ] Placing a portal shows it with god mode labels
- [ ] Placing a savepoint shows it with god mode labels
- [ ] Save → reload → all placed entities persist
- [ ] Zone jump loads target zone and places ship correctly
- [ ] Can jump to a zone, edit it, jump back, edits persist
- [ ] New zone creates a file, loads it, can edit and save
- [ ] New zone inherits cell type palette from source zone
- [ ] Mode cycling (Q/E) wraps around correctly
- [ ] Tab cycling works per mode
- [ ] HUD shows correct mode name, sub-type, zone name, grid coords
- [ ] God mode labels render for all entity types
- [ ] Zone with `bgcolor` lines loads with custom background colors
- [ ] Zone without `bgcolor` lines uses default purple palette
- [ ] Zone with `music` lines plays tracks sequentially, loops after last
- [ ] Track advances automatically when one finishes (no gap/hang)
- [ ] Zone without `music` lines falls back to random global track
- [ ] Zone jump switches to new zone's music

## Open Questions

1. **Portal linking workflow**: Decided — place portals with auto-IDs, edit zone file by hand to set destinations. Revisit in-editor linking later if it becomes a pain.

2. **Entity snap position**: Decided — enemies, portals, and savepoints snap to grid intersection points (cell corners), matching existing spawn placement convention. Cells fill the cell as usual.

3. **Duplicate prevention**: Decided — skip if a portal/savepoint already exists at that grid position. Enemies can always stack (multiple at the same position allowed).

4. **Zone jump shared helper**: Decided — extract `zone_teardown_and_load()` as a shared static helper in `mode_gameplay.c`. Both `warp_do_zone_swap()` and `god_mode_jump_to_zone()` call it, then handle their own spawn/state/BGM setup.
