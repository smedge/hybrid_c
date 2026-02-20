# Spec: Procgen Phase 3 — Center Anchor + Hotspots + Landmarks + Influence Fields

**DRAFT** — for review

## Summary

Phase 3 transforms procgen zones from uniform noise blobs into places with spatial identity. The zone center gets a recognizable structure that rotates per seed. Landmark chunks (boss arenas, portal rooms, encounter gates) are placed at procedurally generated hotspot positions. Each landmark carries a terrain influence that warps the surrounding noise — dense landmarks create labyrinths, sparse landmarks create open battlefields. The result: every seed produces a meaningfully different zone where terrain character emerges from what's *there*, not where it is on the grid.

**Builds on**: Phase 2 (PRNG, simplex noise, single-threshold wall generation, circuit vein layer, hand-placed cell preservation)

**Produces**: Center anchor with per-seed rotation, landmark chunks at randomized positions, terrain density that varies organically around landmarks

---

## What Exists Today (Phase 2 State)

| Component | Status |
|-----------|--------|
| `prng.c/h` | Complete — xoshiro128**, seeded, deterministic |
| `noise.c/h` | Complete — 2D simplex + multi-octave fBm |
| `procgen.c/h` | Phase 2 — single wall threshold, circuit vein layer, center exclusion zone |
| `zone.c/h` | Parses `procgen`, `noise_*` params. Saves only hand-placed cells for procgen zones |
| Zone files | Two procgen zones with hand-placed center structures (~200 cell lines each) |
| `chunk.c/h` | Does not exist |

### Current `Procgen_generate` flow:
1. Derive zone seed from master seed + filepath
2. Seed PRNG
3. Detect circuit vs solid wall types from zone cell types
4. Clear 64x64 center exclusion zone (hardcoded)
5. For each non-hand-placed, non-center cell: noise → wall threshold → set wall type (circuit/solid via vein noise)

---

## Implementation Plan

### Step 1: Chunk Loader (`chunk.c/h`)

The chunk system loads small hand-authored room templates from text files and stamps them onto the map grid. This is the foundation for center anchors, landmark rooms, and (eventually) structured sub-areas.

**Chunk file format** (`.chunk` text files in `resources/chunks/`):

```
# Comments start with #
chunk origin_center
size 48 48

# Cell lines: wall <local_x> <local_y> <celltype_index>
# celltype_index references the zone's celltype list (0-based)
wall 0 0 0
wall 0 1 1
wall 5 5 0

# Empty lines: explicit empty space (overrides noise)
empty 10 10
empty 10 11

# Spawn slots: spawn <local_x> <local_y> <entity_type> <probability>
spawn 24 24 savepoint 1.0
spawn 20 20 mine 0.5
```

Keep it simple — walls and empties are the minimum viable chunk. `maybe` cells, `obstacle_zone`, and exits can be added in Phase 4 when structured sub-areas need them.

**Data structures:**

```c
// chunk.h

#define CHUNK_MAX_CELLS 4096
#define CHUNK_MAX_SPAWNS 32
#define CHUNK_MAX_TEMPLATES 64

typedef struct {
    int x, y;           // Local coords within chunk
    int celltype_index; // Index into zone's celltype array
} ChunkCell;

typedef struct {
    int x, y;
    // empty = cleared space, terrain gen skips these
} ChunkEmpty;

typedef struct {
    int x, y;
    char entity_type[32]; // "mine", "hunter", "seeker", "defender", "stalker", "savepoint", "portal"
    float probability;     // 0.0 - 1.0
} ChunkSpawn;

typedef struct {
    char name[64];
    int width, height;

    ChunkCell walls[CHUNK_MAX_CELLS];
    int wall_count;

    ChunkEmpty empties[CHUNK_MAX_CELLS];
    int empty_count;

    ChunkSpawn spawns[CHUNK_MAX_SPAWNS];
    int spawn_count;
} ChunkTemplate;
```

**API:**

```c
// Load a chunk template from file. Returns NULL on failure.
ChunkTemplate* Chunk_load(const char* filepath);

// Free a loaded chunk template.
void Chunk_free(ChunkTemplate* chunk);

// Stamp a chunk onto the zone grid at (center_x, center_y) with transform.
// center_x/center_y = grid coords of the chunk's center cell.
// Marks stamped cells as "fixed" in the zone so terrain gen skips them.
void Chunk_stamp(const ChunkTemplate* chunk, Zone* zone,
                 int center_x, int center_y, ChunkTransform transform);
```

**Transform system:**

```c
typedef enum {
    TRANSFORM_IDENTITY,
    TRANSFORM_ROT90,
    TRANSFORM_ROT180,
    TRANSFORM_ROT270,
    TRANSFORM_MIRROR_H,
    TRANSFORM_MIRROR_V,
    TRANSFORM_MIRROR_H_ROT90,
    TRANSFORM_MIRROR_V_ROT90,
    TRANSFORM_COUNT           // = 8
} ChunkTransform;
```

Transform is applied to local chunk coords before adding the center offset. A local coord `(lx, ly)` relative to chunk center `(w/2, h/2)` becomes:
- `IDENTITY`: `(lx, ly)`
- `ROT90`: `(-ly, lx)`
- `ROT180`: `(-lx, -ly)`
- `ROT270`: `(ly, -lx)`
- `MIRROR_H`: `(-lx, ly)`
- `MIRROR_V`: `(lx, -ly)`
- `MIRROR_H_ROT90`: `(ly, lx)`
- `MIRROR_V_ROT90`: `(-ly, -lx)`

**Stamping logic:**

```
Chunk_stamp(chunk, zone, cx, cy, transform):
    half_w = chunk->width / 2
    half_h = chunk->height / 2

    // First pass: mark the entire chunk footprint as fixed
    for local_y in 0..chunk->height:
        for local_x in 0..chunk->width:
            (gx, gy) = apply_transform(local_x - half_w, local_y - half_h, transform) + (cx, cy)
            if in_bounds(gx, gy):
                zone->cell_fixed[gx][gy] = true   // terrain gen skips this cell

    // Second pass: stamp walls
    for each wall in chunk->walls:
        (gx, gy) = apply_transform(wall.x - half_w, wall.y - half_h, transform) + (cx, cy)
        if in_bounds(gx, gy):
            zone->cell_grid[gx][gy] = wall.celltype_index

    // Third pass: stamp empties (clear any wall that might be there)
    for each empty in chunk->empties:
        (gx, gy) = apply_transform(empty.x - half_w, empty.y - half_h, transform) + (cx, cy)
        if in_bounds(gx, gy):
            zone->cell_grid[gx][gy] = -1   // no cell

    // Spawns are resolved later during entity population
```

**The `cell_fixed` grid**: A new `bool` grid on Zone (or reuse `cell_hand_placed`) that marks cells as "do not overwrite with noise terrain." Both hand-placed cells AND chunk-stamped cells get this flag.

**Decision: `cell_fixed` vs `cell_hand_placed`**:
- `cell_hand_placed` already exists and preserves godmode edits through procgen regeneration
- Chunk-stamped cells are different — they should be regenerated when the seed changes
- **Use a separate `cell_chunk_stamped` grid** that gets cleared on regeneration, so chunk cells regenerate with new transforms but hand-placed cells persist
- Terrain gen skips cells where EITHER `cell_hand_placed` OR `cell_chunk_stamped` is true

---

### Step 2: Center Anchor System

**Extract existing center structures to chunk files.**

The two procgen zones currently have ~200 hand-placed `cell` lines each that form their center structures. These become the first chunk files:

1. Load the zone in godmode
2. Identify the center structure bounds
3. Write a chunk file with those cells at local coords
4. Remove the `cell` lines from the zone file
5. Add `center_anchor <chunk_file>` to the zone file

This produces two real chunk files we can test the system with immediately.

**Zone file additions:**

```
center_anchor resources/chunks/origin_center.chunk
```

**Zone struct additions:**

```c
// In Zone struct
char center_anchor_path[256];  // Path to center anchor chunk file
bool has_center_anchor;
```

**Generation sequence** (updated):

```
Procgen_generate:
    1. Derive zone seed, seed PRNG
    2. IF has_center_anchor:
         a. Load center chunk
         b. Pick transform: seed % TRANSFORM_COUNT
         c. Stamp at map center (size/2, size/2)
         d. Free chunk
       ELSE:
         Apply hardcoded 64x64 center exclusion (backward compat)
    3. Generate hotspot positions (Step 3)
    4. Resolve landmarks → stamp chunks (Step 4)
    5. Compute influence field + generate terrain (Step 5)
```

---

### Step 3: Hotspot Position Generator

Generate candidate positions where landmarks might be placed. Positions vary per seed.

**Zone file additions:**

```
hotspot_count 10
hotspot_edge_margin 80
hotspot_center_exclusion 120
hotspot_min_separation 150
```

**Zone struct additions:**

```c
// In Zone struct (procgen params)
int hotspot_count;            // Target number of hotspot positions (default 10)
int hotspot_edge_margin;      // Min distance from map edges in grid cells (default 80)
int hotspot_center_exclusion; // Min distance from center (default 120)
int hotspot_min_separation;   // Min distance between hotspots (default 150)
```

**Hotspot data (internal to procgen.c, not stored on Zone):**

```c
#define MAX_HOTSPOTS 32

typedef struct {
    int x, y;       // Grid coords
    bool used;      // Assigned to a landmark
} Hotspot;
```

**Algorithm:**

```
generate_hotspots(zone, rng):
    margin = zone->hotspot_edge_margin
    center_excl = zone->hotspot_center_exclusion
    min_sep = zone->hotspot_min_separation
    target = zone->hotspot_count
    center = zone->size / 2

    hotspots = []
    max_attempts = target * 50

    for attempt in 0..max_attempts:
        if len(hotspots) >= target: break

        x = Prng_range(margin, zone->size - margin, rng)
        y = Prng_range(margin, zone->size - margin, rng)

        // Center exclusion
        dx = x - center
        dy = y - center
        if sqrt(dx*dx + dy*dy) < center_excl: continue

        // Separation check
        too_close = false
        for each h in hotspots:
            dx = x - h.x
            dy = y - h.y
            if sqrt(dx*dx + dy*dy) < min_sep:
                too_close = true
                break
        if too_close: continue

        hotspots.append({x, y, used=false})

    if len(hotspots) < target:
        printf("Warning: only placed %d of %d hotspots\n", len(hotspots), target)

    return hotspots
```

**Defaults** (used when zone file doesn't specify):

| Parameter | Default | Rationale |
|-----------|---------|-----------|
| `hotspot_count` | 10 | Enough for 5-7 landmarks with surplus |
| `hotspot_edge_margin` | 80 | ~8% of map, keeps landmarks off edges |
| `hotspot_center_exclusion` | 120 | Prevents landmarks overlapping center structure |
| `hotspot_min_separation` | 150 | Ensures landmarks don't cluster |

---

### Step 4: Landmark Definitions + Resolution

**Zone file additions:**

```
# landmark <type> <chunk_file> <priority> <influence_type> <radius> <strength> <falloff>
landmark boss_arena resources/chunks/boss_01.chunk 1 dense 120 0.9 1.5
landmark exit_portal resources/chunks/portal_exit.chunk 2 moderate 80 0.6 2.0
landmark safe_zone resources/chunks/safe_01.chunk 3 sparse 100 0.8 1.0

landmark_min_separation 120
```

**Influence types:**

```c
typedef enum {
    INFLUENCE_DENSE,     // Raise wall threshold → more walls, labyrinthine
    INFLUENCE_MODERATE,  // Slight adjustments → balanced mix
    INFLUENCE_SPARSE     // Lower wall threshold → fewer walls, open battlefield
} InfluenceType;
```

Note: `INFLUENCE_STRUCTURED` is deferred to Phase 4 — it requires the structured sub-area fill system which is a significant chunk of work on its own.

**Data structures:**

```c
#define MAX_LANDMARKS 16

typedef struct {
    InfluenceType type;
    float radius;       // Grid cells — how far influence extends
    float strength;     // 0.0 - 1.0, how strongly it overrides base noise
    float falloff;      // Exponent: 1.0 = linear, 2.0 = quadratic
} TerrainInfluence;

typedef struct {
    char type[32];            // Freeform label: "boss_arena", "safe_zone", etc.
    char chunk_path[256];     // Path to chunk file
    int priority;             // Lower = placed first
    TerrainInfluence influence;
} LandmarkDef;

// Placed landmark (result of resolution)
typedef struct {
    LandmarkDef* def;
    int grid_x, grid_y;      // Where it landed
} PlacedLandmark;
```

**Zone struct additions:**

```c
// In Zone struct
LandmarkDef landmarks[MAX_LANDMARKS];
int landmark_count;
int landmark_min_separation;  // Grid cells (default 120)
```

**Landmark resolution algorithm:**

```
resolve_landmarks(zone, hotspots, hotspot_count, rng):
    // Sort landmark defs by priority (lower first)
    sorted = sort_by_priority(zone->landmarks, zone->landmark_count)
    placed = []

    for each def in sorted:
        // Build candidate list: unused hotspots far enough from all placed landmarks
        candidates = []
        for each hotspot in hotspots:
            if hotspot.used: continue

            far_enough = true
            for each p in placed:
                dx = hotspot.x - p.grid_x
                dy = hotspot.y - p.grid_y
                if sqrt(dx*dx + dy*dy) < zone->landmark_min_separation:
                    far_enough = false
                    break

            if far_enough:
                candidates.append(hotspot)

        if candidates is empty:
            // Fallback: use any unused hotspot (best effort)
            for each hotspot in hotspots:
                if !hotspot.used:
                    candidates.append(hotspot)

        if candidates is empty:
            printf("Warning: no hotspot available for landmark '%s'\n", def->type)
            continue

        // Pick randomly from candidates
        chosen = candidates[Prng_range(0, len(candidates) - 1, rng)]
        chosen.used = true

        // Load and stamp the landmark chunk
        chunk = Chunk_load(def->chunk_path)
        if chunk:
            transform = Prng_range(0, TRANSFORM_COUNT - 1, rng)
            Chunk_stamp(chunk, zone, chosen.x, chosen.y, transform)
            Chunk_free(chunk)

        placed.append({def, chosen.x, chosen.y})

    return placed
```

Note: Gate-aware placement (preferring gated landmarks behind their gate relative to center) is deferred to Phase 4 along with gate enforcement. Phase 3 uses simple random selection from valid candidates.

---

### Step 5: Influence Field + Terrain Integration

This is where the magic happens. Instead of a flat wall threshold across the entire map, each cell's threshold is modulated by nearby landmark influences. Dense landmarks raise the threshold (more walls), sparse landmarks lower it (fewer walls).

**No separate influence grid.** The influence is computed inline during terrain generation — for each cell, loop over placed landmarks and accumulate threshold adjustments. With <=16 landmarks and a 1024x1024 grid, this is ~16M distance checks — fast enough in C (the noise evaluation is more expensive).

**Influence constants:**

```c
#define DENSE_WALL_SHIFT    0.35   // Dense landmarks raise wall threshold by up to this much
#define SPARSE_WALL_SHIFT   0.30   // Sparse landmarks lower wall threshold by up to this much
#define MODERATE_WALL_SHIFT 0.10   // Moderate landmarks raise slightly
```

**Per-cell threshold computation:**

```c
static double compute_wall_threshold(int gx, int gy, double base_threshold,
                                     PlacedLandmark* landmarks, int landmark_count)
{
    double threshold = base_threshold;

    for (int i = 0; i < landmark_count; i++) {
        double dx = gx - landmarks[i].grid_x;
        double dy = gy - landmarks[i].grid_y;
        double dist = sqrt(dx*dx + dy*dy);
        TerrainInfluence* inf = &landmarks[i].def->influence;

        if (dist >= inf->radius) continue;

        // Falloff: 1.0 at landmark center, 0.0 at radius edge
        double t = 1.0 - (dist / inf->radius);
        double weight = pow(t, inf->falloff) * inf->strength;

        switch (inf->type) {
            case INFLUENCE_DENSE:
                threshold += weight * DENSE_WALL_SHIFT;
                break;
            case INFLUENCE_SPARSE:
                threshold -= weight * SPARSE_WALL_SHIFT;
                break;
            case INFLUENCE_MODERATE:
                threshold += weight * MODERATE_WALL_SHIFT;
                break;
        }
    }

    // Clamp to sane range
    if (threshold < -0.8) threshold = -0.8;
    if (threshold > 0.8) threshold = 0.8;

    return threshold;
}
```

**Updated terrain generation loop:**

```c
// In Procgen_generate, after hotspots + landmarks are placed:
for (int y = 0; y < zone->size; y++) {
    for (int x = 0; x < zone->size; x++) {
        if (zone->cell_hand_placed[x][y]) continue;
        if (zone->cell_chunk_stamped[x][y]) continue;

        // Consume one PRNG value per cell regardless (determinism)
        Prng_float(&rng);

        double noise_val = Noise_fbm(x, y, octaves, freq, lac, pers, zone_seed);
        double wall_thresh = compute_wall_threshold(x, y, base_wall_threshold,
                                                     placed_landmarks, placed_count);

        if (noise_val < wall_thresh) {
            // Wall — decide circuit vs solid via vein noise (existing Phase 2 logic)
            // ...
            zone->cell_grid[x][y] = wall_type_index;
        }
        // else: empty (no cell)
    }
}
```

**What this produces:**
- Near a `dense` landmark: wall threshold is raised, so more noise values fall below it → more walls → labyrinth
- Near a `sparse` landmark: wall threshold is lowered → fewer walls → open area with scattered walls
- Near a `moderate` landmark: slight threshold increase → balanced terrain
- Between landmarks: base threshold applies → default density
- Overlapping influences: additive — two dense landmarks create extra-dense terrain
- Transition: smooth gradient from landmark center to radius edge, controlled by `falloff` exponent

---

### Step 6: Debug Visualization

Essential for tuning. Godmode overlays to see what the generator is doing.

**Hotspot markers**: Render colored dots at each hotspot position in godmode. Used hotspots (assigned to landmarks) = one color, unused = another.

**Landmark labels**: Render the landmark type label ("boss_arena", "safe_zone") above each placed landmark position.

**Influence radius rings**: Render circles at each landmark's influence radius. Color-coded by type (red = dense, blue = sparse, green = moderate).

**Seed display**: Show current zone seed in the HUD during godmode.

**Seed cycling hotkey**: A godmode key (e.g., `;`) that increments the master seed and calls `Zone_regenerate_procgen()`. Instant visual feedback on how different seeds change the zone.

---

## Updated `Procgen_generate` Flow

```
Procgen_generate(zone):
    1. Guard: if !zone->procgen, return
    2. Derive zone_seed from master_seed + filepath (existing)
    3. Seed PRNG (existing)
    4. Clear cell_chunk_stamped grid

    // ─── New: Center Anchor ───
    5. If zone->has_center_anchor:
         Load center chunk file
         transform = zone_seed % TRANSFORM_COUNT
         Stamp at (size/2, size/2) with transform
         Free chunk
       Else:
         Clear 64x64 center zone (existing fallback)

    // ─── New: Hotspots ───
    6. Generate hotspot positions (constrained scatter)

    // ─── New: Landmarks ───
    7. Resolve landmarks → assign to hotspots, stamp chunks

    // ─── Modified: Terrain with Influence ───
    8. Detect circuit vs solid wall types (existing)
    9. For each cell (existing loop, modified):
         Skip hand-placed and chunk-stamped cells
         Evaluate noise (existing)
         Compute influence-modulated wall threshold (NEW)
         Apply wall/empty decision
         Circuit vs solid vein noise (existing)

    // ─── New: Debug info ───
    10. Store placed landmarks + hotspots for debug rendering
    11. Print summary with landmark placements
```

---

## Zone File Format Additions

New lines parsed by `zone.c`:

```
# Center anchor
center_anchor resources/chunks/origin_center.chunk

# Hotspot generation
hotspot_count 10
hotspot_edge_margin 80
hotspot_center_exclusion 120
hotspot_min_separation 150

# Landmarks
# landmark <type> <chunk_file> <priority> <influence_type> <radius> <strength> <falloff>
landmark boss_arena resources/chunks/boss_01.chunk 1 dense 120 0.9 1.5
landmark exit_portal_1 resources/chunks/portal_exit.chunk 2 moderate 80 0.6 2.0
landmark safe_zone resources/chunks/safe_01.chunk 3 sparse 100 0.8 1.0
landmark_min_separation 120
```

All new parameters have sensible defaults so existing procgen zones work without changes (they just won't have landmarks).

---

## New Files

| File | Purpose |
|------|---------|
| `src/chunk.c` | Chunk loader, transform math, stamping |
| `src/chunk.h` | ChunkTemplate, ChunkTransform, API |
| `resources/chunks/origin_center.chunk` | Center anchor for zone_procgen_01 |
| `resources/chunks/fire_center.chunk` | Center anchor for zone_procgen_02 |

## Modified Files

| File | Changes |
|------|---------|
| `src/procgen.c` | Hotspot gen, landmark resolution, influence field, center anchor stamping |
| `src/procgen.h` | PlacedLandmark struct, debug data accessors |
| `src/zone.c` | Parse new zone file lines, new Zone struct fields |
| `src/zone.h` | LandmarkDef, TerrainInfluence, hotspot params, center_anchor on Zone struct |
| `src/mode_gameplay.c` | Debug rendering for hotspots/landmarks/influence (godmode only) |
| `src/input.c` | Seed cycling hotkey in godmode |
| `Makefile` | Add chunk.c |

---

## Content Work

**Chunk authoring** — needed before testing:

1. **Extract center anchors**: Take the ~200 hand-placed cell lines from each procgen zone file and convert them to `.chunk` files. This is a one-time manual conversion (or we write a quick script).

2. **Author 2-3 simple landmark chunks**: Doesn't need to be final content. Simple rooms with distinctive shapes so we can visually verify placement and influence effects:
   - A dense landmark (small arena with thick walls)
   - A sparse landmark (minimal structure, mostly empty)
   - A moderate landmark (balanced room)

3. **Test chunk transforms**: Verify all 8 rotations/mirrors produce correct geometry.

---

## Testing Checklist

- [ ] Chunk loader correctly parses `.chunk` files
- [ ] All 8 transforms produce correct geometry (no off-by-one, no mirroring artifacts)
- [ ] Center anchor appears at map center with per-seed rotation
- [ ] Hotspot positions vary per seed
- [ ] Hotspot constraints respected (margin, center exclusion, separation)
- [ ] Landmarks appear at different hotspot positions per seed
- [ ] Landmark priority ordering works (lower priority placed first gets better positions)
- [ ] Terrain is denser around dense-influence landmarks
- [ ] Terrain is sparser around sparse-influence landmarks
- [ ] Transitions between influence zones are smooth (no hard edges)
- [ ] Existing hand-placed cells survive regeneration
- [ ] Chunk-stamped cells regenerate correctly on seed change
- [ ] Seed cycling in godmode instantly shows different layouts
- [ ] Debug overlays show hotspots, landmarks, influence radii
- [ ] Deterministic: same seed = identical output
- [ ] Backward compatible: existing procgen zones without center_anchor/landmarks still work

---

## Deferred to Phase 4

These items from the original Phase 3 spec are deferred:

- **Gate-aware landmark placement** — Requires gate definitions and connectivity validation. Simple random placement is sufficient for Phase 3.
- **`INFLUENCE_STRUCTURED`** — Requires the structured sub-area fill system (Spelunky-style room walk). Dense noise terrain is good enough.
- **Effect cells** — Traversable cells with visual properties. The cell system and rendering changes deserve their own focused implementation, either late Phase 3 or early Phase 4.
- **Enemy bias / density_mult on landmarks** — Phase 5 (enemy population).
- **Chunk exits and connectivity metadata** — Phase 4 (corridors need to know where chunks have openings).

---

## Open Questions

1. **Center anchor chunk authoring**: Write a script to extract center cells from existing zone files into `.chunk` format? Or do it by hand? (The hand-placed cells are already in text format, so conversion is mostly reformatting coords to be chunk-local.)

2. **Chunk-stamped cell tracking**: New `cell_chunk_stamped[][]` grid, or add a flags field to the existing cell grid? A separate bool grid is simpler and matches the `cell_hand_placed` pattern.

3. **Landmark chunk transforms**: Should all landmark chunks get random transforms, or should some landmarks specify `no_rotate` / `no_mirror` flags? (A boss arena might need a specific orientation relative to its entrance.)

4. **Influence overlap**: Two dense landmarks with overlapping radii — their influences are additive, so the overlap region becomes extra-dense. Is this desirable or should we use max-wins? Start with additive, tune if needed.

5. **Hotspot surplus**: With 10 hotspots and 3-5 landmarks, we have 5-7 unused hotspots. These are wasted today but become useful in Phase 5 for secondary enemy clusters or resource nodes. Worth keeping them around in the debug data.

6. **Performance**: 1024x1024 cells × 16 landmark distance checks = ~16M sqrt calls. Should be fast in C but worth profiling. If slow, can precompute a coarse influence grid (e.g., 1 value per 4x4 tile block) and interpolate.
