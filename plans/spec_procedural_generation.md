# Spec: Procedural Level Generation — A Hybrid Approach

## Summary & Implementation Reality

### What this system does

You build the identity of each zone by hand — landmark rooms (boss arenas, portal rooms, safe zones), anchor walls, ability gates. The algorithm procedurally generates the terrain between those landmarks: corridors, open battlefields, combat rooms, and enemy placements. Every generation run is **seed-deterministic**: same zone + same seed = identical world. This is a hard requirement — foundational for multiplayer, shared seeds, reproducible bugs, and daily/weekly challenge runs.

The key innovation over the reference designs (Dead Cells, Spelunky): **landmark positions vary per seed.** The designer places generic hotspot markers at viable locations throughout the zone. The generator assigns landmark types (boss, portal, safe zone) to hotspots each run. The boss arena's internal geometry is always the same (hand-authored), but it could be anywhere. Players know the fire zone *has* a boss — they have to explore to find it.

**Two generation modes** handle the range of zone layouts Hybrid needs:

- **Structured mode** — Chunk-based grid with solution path guarantee (Spelunky-style). For corridors, labyrinths, and dense navigational areas. Variable chunk sizes (8×8 to 64×64) mix within a single region.
- **Open mode** — Scatter-based placement of individual walls, obstacle blocks, and enemy spawns across open space. For expansive battlefields, arenas, and sparse cyberspace. Density tunable per region. No chunks, no solution path — traversability is trivially guaranteed because the space is mostly empty.

The content authoring investment (chunk templates + obstacle blocks) is a fraction of hand-coding every room across 12+ zones at 1024×1024 scale. Each authored template multiplies through procedural assembly, probabilistic variation, and mirroring.

### Who does what

**Jason (designer) is responsible for:**
- Authoring landmark chunk templates (boss arena geometry, portal rooms, safe zones — hand-tuned internal design, position varies per seed)
- Placing general hotspot markers at viable positions throughout each zone (4+ per zone, untyped — the generator assigns landmark types)
- Optionally hand-placing specific landmarks at fixed positions when you want explicit control
- Authoring chunk templates for structured regions (reusable building blocks — 50-100 across all biomes, variable sizes)
- Authoring obstacle blocks for both structured and open regions (small sub-patterns — 15-30 generic + biome-specific)
- Defining region rectangles, constraints, and generation mode in zone files
- Tuning: difficulty, density, combat ratios, pacing tags, hotspot weights, probabilistic cell percentages
- Implementing prerequisites: god mode editing tools, portals/zone transitions, diagonal walls (before procgen)

**Bob (code) is responsible for:**
- Chunk file parser + loader with validation (exit traversability, safety rule enforcement)
- Obstacle block parser + loader
- Hotspot resolver (weighted selection, min_distance enforcement, landmark chunk stamping)
- Seedable PRNG (xoshiro128** or PCG)
- Region parser (extension to zone.c)
- Structured mode: solution path walk, chunk selection, multi-cell chunk placement, non-path fill, chunk stamping, exit stitching
- Open mode: scatter algorithm, density/spacing enforcement, obstacle block placement
- Enemy population system (budget-capped, spacing-aware, pacing-modulated)
- Region border sealing
- Integration into Zone_load() call sequence

### Prerequisites (implement before procgen)

1. **God mode editing tools** — Spec and implement the authoring tools needed for procgen content (chunk editing workflow, hotspot placement, region markers). Separate spec to be written.
2. **Portals and zone transitions** — Procgen is most valuable with multiple zones. Jason will spec and implement these first.
3. **Diagonal walls** — New cell types with angled geometry + collision changes. Procgen treats them as just another cell type by name, but they need to exist first.
4. **At least one active enemy type** — Mines are passive. Procedural enemy placement in combat rooms isn't interesting until something hunts the player.

### Implementation order (incremental, tight feedback loop)

| Step | What | Visible result |
|------|------|---------------|
| 1 | PRNG + chunk file parser + hardcoded test chunks | Can load and print chunk data (console) |
| 2 | Chunk stamper (stamp one chunk at a known position) | See a chunk's walls appear on the map |
| 3 | Region parser + coarse grid + straight-line path | See a corridor of chunks connecting two points |
| 4 | Solution path walk (random direction, biased) | See winding paths through structured regions |
| 5 | Multi-cell chunks (large chunks spanning grid cells) | See mixed room sizes in a region |
| 6 | Non-path fill (sealed + connected rooms) | See full structured regions |
| 7 | Open mode scatter algorithm | See sparse regions with scattered walls and enemies |
| 8 | Hotspot resolver + landmark chunk placement | See landmarks at different positions per seed |
| 9 | Probabilistic cells + obstacle zones + mirroring | See per-seed variation within chunks |
| 10 | Enemy population with budget system | See enemies distributed through generated content |
| 11 | Tuning pass | Tweak densities, bias weights, chunk variety |

### Complexity assessment

| Component | Difficulty | Risk | Notes |
|-----------|-----------|------|-------|
| PRNG | Trivial | None | ~10 lines of C |
| Chunk parser | Easy | None | Same line-based text pattern as zone parser |
| Chunk stamper | Easy | Low | Map_set_cell in a loop. Off-by-one risk with mirroring. |
| Region parser | Easy | None | Bolt onto zone.c |
| Hotspot resolver | Easy | None | Weighted pick + distance check |
| Solution path walk | Medium-Hard | Medium | Core algorithm. Backtracking state management is where bugs hide. |
| Multi-cell chunk placement | Medium | Medium | Large chunks claiming NxN grid cells. Needs careful walk integration. |
| Non-path fill | Medium | Low | Straightforward once path generator works |
| Open mode scatter | Medium | Low | Random placement with spacing constraints. Simpler than structured mode. |
| Exit stitching | Medium | Low | Simplified if we standardize exit offsets per chunk size class |
| Enemy population | Medium | Low | Arithmetic on existing spawn code + budget cap |
| Loader validation (flood fill) | Medium | Low | BFS on small grids. Must be correct. |

---

## Inspiration

Two key references, extended with our own innovations:

1. **Dead Cells** — [Building the Level Design of a Procedurally Generated Metroidvania](https://www.gamedeveloper.com/design/building-the-level-design-of-a-procedurally-generated-metroidvania-a-hybrid-approach-). Six-layer hybrid: fixed world framework → hand-designed room templates → conceptual level graphs → algorithmic room placement → enemy population → loot distribution. Key takeaway: algorithms serve hand-crafted constraints, not the other way around.

2. **Spelunky** — [Part 1: Solution Path](https://tinysubversions.com/spelunkyGen/), [Part 2: Room Templates](https://tinysubversions.com/spelunkyGen2/). Key takeaways:
   - **Solution path first**: Build the critical path before filling around it. Reachability by construction, not validation.
   - **Exit configurations**: Classify rooms by which sides have openings. Cascading constraints propagate automatically.
   - **Hierarchical templates**: Room → probabilistic tiles → obstacle blocks. Multiple randomization layers within a single template.
   - **Mirroring**: Flip rooms for free variety.

**Our innovations beyond the references:**
- **General hotspots**: Landmark positions vary per seed — the designer places candidate spots, the generator assigns landmark types. Players can't memorize boss/portal locations.
- **Two generation modes**: Structured (chunk-based, Spelunky-style) and Open (scatter-based) per region, controlled by a style parameter. Handles both corridor-crawling and expansive cyberspace battlefields.
- **Variable chunk sizes**: 8×8 to 64×64 within a single region. Large chunks span multiple coarse grid cells. Mixes tight corridors with large arenas.
- **Composite regions**: Multiple overlapping rectangles unioned into irregular shapes (L-shapes, T-shapes).

---

## Definitions

| Term | Definition |
|------|-----------|
| **Zone** | A complete playable map. Grid size varies per zone (up to 1024×1024 cells of 100 world units each). One .zone file per zone. |
| **Map cell** | A single cell in the zone grid. 100×100 world units. Coordinates are 0-indexed. |
| **Fixed asset** | Any map cell or structure hand-placed by the designer. Stored in the .zone file. Never modified by the generator. |
| **Hotspot** | A designer-placed candidate position where any landmark chunk might be placed. Untyped — the generator assigns landmark types (boss, portal, safe zone) from the hotspot pool per seed. |
| **Landmark chunk** | A special chunk template for a key location (portal room, boss arena, safe zone). Hand-authored internal geometry, but position chosen by the generator from hotspot candidates (or fixed by the designer). |
| **Region** | An area of the zone grid designated for procedural generation, defined by one or more rectangles sharing the same ID. Multiple rectangles with the same ID are unioned into a composite region. Each region has a **generation mode**: structured or open. |
| **Chunk** | A reusable room/corridor template for structured regions. Variable dimensions (8×8 to 64×64 map cells). Contains cell data, exit configs, probabilistic elements, and variation rules. |
| **Coarse grid** | A structured region's bounding box subdivided into cells of the region's **base chunk size** (the smallest chunk size used in that region). Large chunks span multiple coarse cells. Each coarse cell is **valid** (inside the region) or **invalid** (outside — impassable). |
| **Exit** | An opening on a chunk's edge that guarantees traversability. Defined by side (N/S/E/W), offset along that edge, and width in cells. |
| **Exit config** | Bitmask of which sides have exits (L/R/T/B). Determines where a chunk can appear on the solution path. |
| **Solution path** | A sequence of coarse grid cells from entry to exit where adjacent chunks have matching exits. Guarantees traversability through structured regions. Not used in open regions. |
| **Critical cells** | Map cells within a chunk on the internal path connecting its exits. Must never be walled off — enforces the exit config guarantee. |
| **Obstacle block** | A small pre-authored sub-pattern (3×3, 4×4, etc.) of wall cells. Used within chunk obstacle zones (structured mode) and as scatter elements (open mode). |
| **Seed** | A uint32 stored in the zone file, initializing the PRNG. Same zone + same seed = identical output. Required. The PRNG call order must be deterministic — no conditional rolls based on runtime state. Foundational for multiplayer. |

---

## Layer 1: Zone Skeleton (Hand-Authored)

The designer authors a zone's **skeleton**: the elements that define the zone's identity and the framework the generator operates within.

### Fixed elements (always present, never modified by generator)

| Element | Purpose |
|---------|---------|
| **Anchor walls** | Decorative or structural geometry — always present, recognizable landmarks |
| **Ability gates** | Gaps/walls requiring specific subroutines to pass — enforce progression order |
| **Region markers** | Rectangles designating procedural fill areas + generation mode |
| **Hotspots** | Candidate positions for landmark chunks — untyped, generator assigns |

### Hand-placed landmarks (optional)

The designer can place specific landmark chunks at **fixed positions** when explicit control is needed. A hand-placed landmark is locked in — the generator treats it as a fixed asset and works around it. Its position counts toward hotspot min_distance checks, so generator-placed landmarks won't crowd it.

Use fixed placement when a landmark's position is critical to the zone's design (e.g., a portal that must be at a specific zone boundary for inter-zone connectivity). Use hotspots when you want the position to vary.

### Hotspot system

Hotspots solve the "boss is always in the upper-right corner" problem.

The designer places **generic hotspot markers** at viable positions throughout the zone. These are untyped — they're just "good spots" that could work for a boss arena, a portal room, a safe zone, or any other landmark. The generator assigns landmark types to hotspots per seed.

This is simpler than per-type hotspots because:
- You don't worry about boss hotspots being too close to portal hotspots
- You place fewer markers (one pool, not N pools)
- The generator handles separation constraints

**Hotspot data:**

```c
typedef struct {
    int grid_x, grid_y;          // Position in the zone grid
    int coarse_col, coarse_row;  // Computed position in the coarse grid (if inside a structured region)
    float weight;                // Selection weight (default 1.0, higher = more likely)
} Hotspot;
```

**Landmark type list** (zone-level, defines what needs to be placed):

```c
typedef struct {
    char type[32];               // "entry_portal", "boss_arena", "exit_portal", "safe_zone"
    char chunk_file[64];         // Landmark chunk template file
    int priority;                // Resolution order (lower = placed first)
} LandmarkDef;
```

**Resolution algorithm:**

```
function resolve_hotspots(hotspots, landmark_defs, fixed_landmarks, min_distance, rng):
    placed = []

    // Fixed hand-placed landmarks go in first — their positions are non-negotiable
    for each fixed in fixed_landmarks:
        placed.append({fixed.grid_x, fixed.grid_y, fixed.type})

    // Sort landmark defs by priority (entry_portal first, then boss, etc.)
    sort(landmark_defs, by priority)

    for each def in landmark_defs:
        if def.type already in placed:
            continue  // Already hand-placed

        // Filter hotspots: must be far enough from all placed landmarks
        candidates = []
        for each hotspot in hotspots:
            too_close = false
            for each p in placed:
                if manhattan_distance(hotspot, p) < min_distance:
                    too_close = true
                    break
            if not too_close:
                candidates.append(hotspot)

        if candidates is empty:
            // Fallback: pick the hotspot that maximizes min distance from placed
            candidates = hotspots (excluding already-used hotspots)
            sort by max_min_distance_from_placed, descending

        chosen = weighted_pick(candidates, weight_field, rng)
        stamp_landmark_chunk(chosen, def.chunk_file)
        placed.append({chosen.grid_x, chosen.grid_y, def.type})
        remove chosen from hotspots  // Can't be reused

    return placed
```

**Hotspot rules (zone loader validation):**
- At least **N+2** hotspots where N is the number of landmark types that aren't hand-placed (gives the generator meaningful choice)
- Hotspots must fall within a region's bounds
- Loader warns if any two hotspots are closer than `min_distance` (they'd constrain each other unnecessarily)

### Design constraint

Regions must not overlap fixed assets or each other. The generator treats every map cell within a region's valid area as writable, except cells occupied by fixed assets. The zone loader rejects overlapping regions or region/fixed-asset collisions as hard errors.

Landmark chunks stamped at hotspot positions become effectively fixed for that generation run — the solution path routes around or through them.

---

## Layer 2: Chunk Templates (Structured Mode)

### Chunk data structure

```c
typedef struct {
    char name[64];               // Unique identifier
    int width, height;           // Grid dimensions in map cells (8-64)

    // Classification
    ChunkCategory category;      // CHUNK_COMBAT, CHUNK_CORRIDOR, CHUNK_JUNCTION, etc.
    char biome[32];              // "neutral", "fire", "ice", etc.
    int difficulty;              // 1-5
    uint8_t flags;               // CHUNK_NO_HMIRROR, CHUNK_NO_VMIRROR

    // Exit configuration
    ExitConfig exit_config;      // Bitmask: EXIT_LEFT | EXIT_RIGHT | EXIT_TOP | EXIT_BOTTOM
    Exit exits[4];               // One per side max
    int exit_count;

    // Cell data — three sparse arrays
    ChunkWall walls[MAX_CHUNK_CELLS];       // Guaranteed solid
    int wall_count;
    ChunkEmpty empties[MAX_CHUNK_CELLS];    // Guaranteed open (critical path + exits)
    int empty_count;
    ChunkMaybe maybes[MAX_CHUNK_CELLS];     // Probabilistic
    int maybe_count;

    // Obstacle zones
    ObstacleZone obstacle_zones[MAX_OBSTACLE_ZONES];
    int obstacle_zone_count;

    // Enemy spawn slots
    ChunkSpawnSlot spawn_slots[MAX_CHUNK_SPAWNS];
    int spawn_slot_count;
} ChunkTemplate;
```

### Exit configurations

```c
typedef enum {
    EXIT_NONE   = 0,
    EXIT_LEFT   = 1 << 0,
    EXIT_RIGHT  = 1 << 1,
    EXIT_TOP    = 1 << 2,
    EXIT_BOTTOM = 1 << 3,
} ExitConfig;

typedef struct {
    Direction side;
    int offset;       // Cell offset along edge (0-indexed from top-left corner of that edge)
    int width;        // Opening width in cells
} Exit;
```

Common configurations:

| Config | Bitmask | Use |
|--------|---------|-----|
| LR | 0x03 | Horizontal corridor |
| TB | 0x0C | Vertical shaft |
| LRB | 0x07 | Downward fork |
| LRT | 0x06 | Upward fork |
| LRTB | 0x0F | Crossroads / junction |
| L/R/T/B | single bit | Dead-end |
| None | 0x00 | Sealed filler room |

**The guarantee**: If a chunk has `EXIT_LEFT | EXIT_RIGHT`, a walkable path exists from the left exit to the right exit through `empty` cells. The chunk loader validates this via flood fill.

### Critical cells and the probabilistic safety rule

**Critical cells** form the internal paths connecting exits. Marked `empty` in the template — never obstructed.

**The rule**: `maybe` cells must never overlap `empty` cells. A probabilistic wall on the critical path would break the exit guarantee. The chunk loader validates this as a hard error.

### Three-tier cell resolution

When a chunk is stamped into the map:

1. **Fill chunk bounds** with default wall type (everything starts solid)
2. **Carve empties** — `Map_clear_cell` for critical path + exits
3. **Resolve maybes** — roll PRNG against probability, set cell if hit
4. **Resolve obstacle zones** — pick block from pool (or empty), stamp it
5. **Resolve spawn slots** — roll PRNG, spawn enemy if hit

Unmarked cells stay as the default wall from step 1. The chunk is mostly wall with carved-out rooms — same philosophy as Spelunky.

### Obstacle blocks

Small pre-authored wall patterns stamped into designated zones within chunks. Also used as scatter elements in open regions.

```c
typedef struct {
    char name[64];
    int width, height;
    ChunkWall cells[MAX_OBSTACLE_CELLS];
    int cell_count;
} ObstacleBlock;
```

```c
typedef struct {
    int x, y;                    // Top-left within chunk (local coords)
    int width, height;           // Zone dimensions
    char pool[MAX_POOL][64];     // Eligible blocks (includes "empty")
    int pool_count;
    float probability;           // Chance of placing any block
} ObstacleZone;
```

**Safety**: Obstacle zones must not overlap `empty` cells. Validated by loader.

### Chunk mirroring

After template selection, before stamping:

- **Horizontal**: cell (x, y) → (width - 1 - x, y). Swap EXIT_LEFT ↔ EXIT_RIGHT.
- **Vertical**: cell (x, y) → (x, height - 1 - y). Swap EXIT_TOP ↔ EXIT_BOTTOM.
- **Both**: 180° rotation.

50% chance per axis (independent), controlled by PRNG. Chunks flagged `CHUNK_NO_HMIRROR` or `CHUNK_NO_VMIRROR` skip the flip.

### Intra-chunk variation math

For a chunk with 10 `maybe` cells (50% each), 2 obstacle zones (4 options each), and 4 mirror states: 2^10 × 4 × 4 × 4 = **65,536** distinct instances from one authored template.

### Chunk file format

```
chunk combat_pillars_01
size 16 16
category combat
biome neutral
difficulty 2
flags no_vmirror

exits LR
exit left 6 4
exit right 6 4

# Guaranteed walls (local coords, 0-indexed from top-left)
wall 0 0 solid
wall 1 0 solid
# ... (perimeter)

# Critical path — guaranteed open
empty 0 6
empty 0 7
empty 0 8
empty 0 9
# ... (path from left exit to right exit)

# Probabilistic cells
maybe 4 4 solid 0.5
maybe 11 4 solid 0.5
maybe 4 11 solid 0.5
maybe 11 11 solid 0.5

# Obstacle zones
obstacle_zone 6 2 4 4 pillar_cluster,wall_segment,empty 0.7
obstacle_zone 6 10 4 4 pillar_cluster,barrier,empty 0.7

# Enemy spawn slots
spawn_slot 8 8 mine 0.3
```

**Parsing rules:**
- `#` comments, blank lines ignored, unrecognized lines are parse errors (strict)
- Coordinates are local to chunk (0,0 = top-left)
- All unmarked cells default to solid wall
- `exits` is the bitmask; `exit` lines are physical openings
- `exit <side> <offset> <width>`

**Loader validation (hard errors):**
1. Every side in `exits` has at least one `exit` line
2. Every `exit` opening cell is within bounds and marked `empty`
3. Flood fill from any exit reaches all other exits via `empty` cells
4. No `maybe` overlaps `empty`
5. No obstacle zone overlaps `empty`
6. Obstacle zone dimensions fit every block in its pool

---

## Layer 3: Region Graph

### Region definition

A **region** is one or more rectangles sharing the same ID, unioned into a composite shape. Each region has a **generation mode**: structured or open.

```c
#define MAX_REGION_RECTS 8

typedef struct {
    int grid_x, grid_y;
    int width, height;
} RegionRect;

typedef struct {
    char id[16];

    // Component rectangles (1+, unioned)
    RegionRect rects[MAX_REGION_RECTS];
    int rect_count;

    // Bounding box (computed)
    int bbox_x, bbox_y;
    int bbox_w, bbox_h;

    // Generation mode
    RegionMode mode;             // MODE_STRUCTURED or MODE_OPEN

    // --- Structured mode fields ---
    int base_chunk_size;         // Smallest chunk size used (coarse grid unit), e.g. 16
    int cols, rows;              // Coarse grid dimensions (bbox / base_chunk_size)
    bool *valid;                 // [cols * rows] validity mask

    // --- Open mode fields ---
    float density;               // 0.0-1.0, fraction of region area with walls/obstacles
    float obstacle_spacing;      // Minimum distance between placed obstacle blocks (map cells)
    float cell_scatter_chance;   // Probability of placing a random individual wall cell per grid point

    // --- Shared fields ---
    int min_chunks, max_chunks;  // For structured: chunk count. For open: obstacle block count.
    float combat_ratio;          // Fraction of combat-oriented content
    int difficulty_min, difficulty_max;
    RegionStyle style;           // STYLE_LINEAR, STYLE_LABYRINTHINE, STYLE_BRANCHING (structured)
                                 // STYLE_SPARSE, STYLE_SCATTERED, STYLE_CLUSTERED (open)
    PacingTag pacing;            // PACING_HOT, PACING_COOL, PACING_NEUTRAL

    // Connections — reference landmark types, resolved after hotspot selection
    RegionConnection entry;
    RegionConnection exit;       // exit.type = "none" for dead-end regions
} Region;
```

```c
typedef struct {
    char landmark_type[32];      // Resolved to a coarse cell / grid position after hotspot selection
    int resolved_x, resolved_y;  // Filled in by hotspot resolver
} RegionConnection;
```

### Composite regions

Multiple `region` lines with the same ID are unioned:

```
region A 100 100 320 320
region A 100 340 512 128
```

Bounding box computed from the union. Coarse grid (structured mode) covers the bounding box; cells outside all component rectangles are marked invalid.

```
Validity mask example (. = invalid, # = valid):
  ##########..........
  ##########..........
  ##########..........
  ##########..........
  ################....
  ################....
```

**Bottleneck warning**: Loader warns if a valid cell has only one valid neighbor — indicates a potential routing bottleneck.

### Coarse grid sizing (structured mode)

The coarse grid covers the bounding box, divided by `base_chunk_size`. A region with `base_chunk_size = 16` and bounding box 320×320 has a 20×20 coarse grid.

Large chunks (e.g., 32×32 in a base-16 grid) occupy 2×2 coarse cells. The walk algorithm treats the block of cells as a single node.

Component rectangles should be multiples of `base_chunk_size`. If not evenly divisible, the loader rounds down and pads remaining cells with solid wall.

### Region connections and hotspot resolution

Connections reference landmark types, not fixed positions:

```
connect A entry entry_portal
connect A exit boss_arena
```

After hotspot resolution, `entry_portal` and `boss_arena` map to specific grid coordinates. The solution path (structured mode) or traversal guarantee (open mode — trivial) connects them.

**Validation**: After resolution, both endpoints must be inside the region's valid area.

**Shared landmarks**: `boss_arena` can be region A's exit and region B's entry. The landmark chunk is placed once; both regions route to/from it.

**Dead-end regions**: `connect C exit none` — the path starts at the entry and explores inward. No exit required.

### Region graph in the zone file

```
# Regions
region A 100 100 320 320
region A 100 340 512 128
region B 500 100 400 400
region C 200 500 200 200

# Generation mode
regionmode A structured 16      # structured, base chunk size 16
regionmode B open                # open scatter mode
regionmode C structured 32      # structured, base chunk size 32

# Hotspots (generic — generator assigns landmark types)
# hotspot <grid_x> <grid_y> [weight]
hotspot 120 120 1.0
hotspot 380 200 1.0
hotspot 600 150 1.2
hotspot 200 400 1.0
hotspot 700 350 0.8
hotspot 300 550 1.0

# Landmark types to place (what + which chunk template + priority)
# landmark <type> <chunk_file> <priority>
landmark entry_portal portal_entry_fire.chunk 1
landmark boss_arena boss_arena_fire.chunk 3
landmark exit_portal portal_exit_fire.chunk 2
landmark safe_zone safe_zone_fire.chunk 4

# Fixed hand-placed landmarks (optional — locked position, generator works around them)
# fixed_landmark <type> <chunk_file> <grid_x> <grid_y>
fixed_landmark entry_portal portal_entry_fire.chunk 100 100

# Minimum distance between placed landmarks (coarse grid cells for structured, map cells for open)
hotspot_min_distance 5

# Region connections
connect A entry entry_portal
connect A exit boss_arena
connect B entry boss_arena
connect B exit exit_portal
connect C entry safe_zone
connect C exit none

# Region constraints
# regionconfig <id> <min_content> <max_content> <combat_ratio> <diff_min> <diff_max> <style> <pacing>
regionconfig A 12 20 0.4 2 3 labyrinthine hot
regionconfig B 20 40 0.6 3 4 scattered hot
regionconfig C 4 8 0.2 1 2 linear cool

# Open mode density (only for open regions)
# opendensity <id> <density> <obstacle_spacing> <cell_scatter_chance>
opendensity B 0.15 8.0 0.02
```

---

## Layer 4: Algorithmic Assembly — Structured Mode

### Phase 1: Solution path generation

The core algorithm from Spelunky, adapted for variable chunk sizes and composite regions.

**The insight**: Don't generate then validate. Build the traversable path first, fill around it. Reachability guaranteed by construction.

**Inputs:**
- Region definition (coarse grid, validity mask, entry cell, exit cell)
- Chunk template library (filtered by biome, difficulty, size)
- Seeded PRNG

**Output:**
- Array of (coarse_col, coarse_row, ChunkTemplate*, mirror_flags, span) for each path node. `span` indicates how many coarse cells the chunk occupies (1×1, 2×2, etc.).

**Algorithm:**

```
function generate_solution_path(region, chunk_lib, rng):
    path = []
    visited = 2D bool array [cols × rows], all false

    entry_cell = region.entry.resolved_cell
    exit_cell  = region.exit.resolved_cell  // INVALID for dead-end regions

    current = entry_cell
    mark_visited(visited, current, 1)  // 1×1 initially

    prev_required_exit = EXIT_NONE  // No constraint on first chunk's "incoming" side

    while current != exit_cell:
        direction = pick_walk_direction(current, exit_cell, region, visited, rng)

        if direction == STUCK:
            // Backtrack: pop last path entry, unmark, try different direction
            // Limit backtrack depth to 5. If exhausted, restart walk (limit 10 restarts).
            // If all restarts fail, fall back to straight-line corridor.
            backtrack_or_restart()
            continue

        next_cell = current + direction_offset(direction)

        // Current chunk needs: prev_required_exit + exit toward next cell
        current_required = prev_required_exit | direction_to_exit(direction)

        // Try to place a large chunk first (more interesting), fall back to base size
        chunk = select_chunk_with_size_preference(
            chunk_lib, current_required, region, current, visited, rng
        )

        if chunk == NULL:
            // Try different direction
            continue

        span = chunk.width / region.base_chunk_size  // 1 for base size, 2 for 2x, etc.
        mirror = roll_mirror(chunk, rng)
        path.append({current, chunk, mirror, span})
        mark_visited(visited, current, span)

        prev_required_exit = opposite_exit(direction)
        current = next_cell

    // Place exit chunk
    exit_required = prev_required_exit
    if exit_cell != INVALID:
        exit_required |= exit_toward_landmark(region.exit)
    chunk = select_chunk(chunk_lib, exit_required, region.constraints, rng)
    path.append({current, chunk, roll_mirror(chunk, rng), span})

    return path
```

**`select_chunk_with_size_preference`** — tries large chunks first for variety:

```
function select_chunk_with_size_preference(chunk_lib, required_exits, region, pos, visited, rng):
    // Try sizes from largest to smallest
    for size_class in [64, 32, 16, base_chunk_size] (descending, >= base_chunk_size):
        span = size_class / region.base_chunk_size
        if span > 1:
            // Check if NxN coarse cells starting at pos are all valid and unvisited
            if not can_fit(pos, span, region, visited):
                continue
        // Select chunk of this size matching exits + constraints
        chunk = select_chunk(chunk_lib, required_exits, region.constraints, rng,
                            size_filter=size_class)
        if chunk != NULL:
            return chunk
    return NULL  // Nothing fits
```

**`pick_walk_direction`** — biased by region style:

```
function pick_walk_direction(current, target, region, visited, rng):
    candidates = []
    for dir in [LEFT, RIGHT, UP, DOWN]:
        next = current + offset(dir)
        if in_bounds(next, region) and region.valid[next] and not visited[next]:
            candidates.append(dir)

    if empty: return STUCK

    weights = []
    for dir in candidates:
        if target == INVALID:
            weights.append(1.0)  // Dead-end: uniform
        else:
            alignment = dot(dir_vector, normalize(target - current))
            switch region.style:
                LINEAR:       weight = (alignment > 0) ? 5.0 : (== 0) ? 1.5 : 0.5
                LABYRINTHINE: weight = (alignment > 0) ? 2.0 : (== 0) ? 2.0 : 1.5
                BRANCHING:    weight = (alignment > 0) ? 3.0 : (== 0) ? 2.5 : 0.5
            weights.append(weight)

    return weighted_pick(candidates, weights, rng)
```

**`select_chunk`** — 4-pass constraint relaxation:

```
function select_chunk(chunk_lib, required_exits, constraints, rng, size_filter=0):
    // Connectivity is NEVER relaxed. Content constraints relax in order:
    // Pass 1: exits + category + difficulty + biome + size
    // Pass 2: exits + difficulty + biome + size  (relax category)
    // Pass 3: exits + biome + size              (relax difficulty)
    // Pass 4: exits + size                      (relax biome)
    // Pass 5: exits only                        (last resort)
    // Return NULL if no chunk has the required exits
```

### Phase 2: Non-path fill

```
function fill_non_path(region, path_cells, chunk_lib, rng):
    for each unoccupied valid coarse cell:
        // Check if adjacent path/filled chunks have exits facing this cell
        desired_exits = compute_desired_exits(cell, neighbors)

        // Style controls connectivity of non-path cells:
        //   labyrinthine → 70% connected, 30% sealed
        //   linear       → 20% connected, 80% sealed
        //   branching    → 50% connected, 50% sealed
        if rng_roll < connect_chance AND desired_exits != 0:
            chunk = select_chunk(chunk_lib, desired_exits, constraints, rng)
        else:
            chunk = select_chunk(chunk_lib, EXIT_NONE, constraints, rng)

        if chunk == NULL:
            fill_solid(cell)
            continue

        place_chunk(cell, chunk, roll_mirror(chunk, rng), rng)
```

### Phase 3: Stamp and stitch

```
function stamp_all_chunks(region, placed_chunks, rng):
    for each placed chunk:
        stamp_chunk(origin, template, mirror, rng)  // 5-pass resolution (see Layer 2)

    // Stitch adjacent chunks with matching exits
    for each adjacent pair with mutual exits:
        stitch_exits(chunk_a, chunk_b)

    seal_region_border(region)
```

**Exit stitching**: clear wall cells on the shared edge where exits overlap. If exits don't align (different offsets), carve an L-shaped connector. **Simplification for v1**: standardize exit offsets per chunk size class — all 16×16 chunks with a left exit at the same offset. Stitching becomes a no-op.

### Backtracking and failure recovery

1. **Local retry**: Try all untried directions from current cell.
2. **Backtrack**: Pop last 1-5 path cells, try alternate routes.
3. **Full restart**: Discard path, advance PRNG, retry. Limit 10 restarts.
4. **Hard fallback**: Straight-line corridor from entry to exit. Always works. Log warning — indicates chunk library content gap.

---

## Layer 5: Algorithmic Assembly — Open Mode

Open regions use a fundamentally different algorithm. No chunks, no coarse grid, no solution path. The space is mostly empty — traversability is trivially guaranteed because walls are sparse and separated.

### Scatter algorithm

```
function generate_open_region(region, obstacle_lib, rng):
    // 1. Compute available area (union of component rectangles, minus fixed assets)
    area = compute_region_area(region)

    // 2. Place obstacle blocks
    obstacle_budget = area * region.density * OBSTACLE_AREA_FRACTION
    placed_obstacles = []

    attempts = 0
    while placed_area < obstacle_budget and attempts < MAX_ATTEMPTS:
        // Pick random position within region bounds
        x = rng_range(region.bbox_x, region.bbox_x + region.bbox_w, rng)
        y = rng_range(region.bbox_y, region.bbox_y + region.bbox_h, rng)

        if not point_in_region(x, y, region):
            attempts++
            continue

        // Pick random obstacle block from biome-appropriate library
        block = pick_uniform(obstacle_lib, rng)

        // Check spacing from other placed obstacles
        if too_close(x, y, placed_obstacles, region.obstacle_spacing):
            attempts++
            continue

        // Check no overlap with fixed assets or landmarks
        if overlaps_fixed(x, y, block):
            attempts++
            continue

        stamp_obstacle_block(x, y, block, rng)  // May mirror the block
        placed_obstacles.append({x, y, block})
        attempts = 0  // Reset on success

    // 3. Scatter individual cells for texture
    for grid_y in region bounds:
        for grid_x in region bounds:
            if not point_in_region(grid_x, grid_y, region):
                continue
            if cell_already_occupied(grid_x, grid_y):
                continue
            if rng_float(rng) < region.cell_scatter_chance:
                cell_type = pick_scatter_cell_type(region.biome, rng)
                Map_set_cell(grid_x, grid_y, cell_type)

    // 4. Place enemies
    populate_enemies_open(region, placed_obstacles, rng)
```

### Open mode styles

The `style` field for open regions controls scatter behavior:

| Style | Behavior |
|-------|----------|
| **SPARSE** | Obstacle blocks placed uniformly with large spacing. Individual cells very rare. Wide open with occasional cover. |
| **SCATTERED** | Moderate spacing. Individual cells more common. Broken terrain with lots of small cover. |
| **CLUSTERED** | Obstacle blocks placed in groups (2-4 nearby, then a gap). Creates islands of structure in open space. |

### Traversability in open regions

Not explicitly guaranteed — it's emergent from low density. At 5-15% wall coverage with spacing constraints, it's geometrically impossible to wall off the player. The density parameter has a hard cap (e.g., 0.40) to prevent the designer from accidentally creating impassable regions in open mode. If you want >40% density, use structured mode.

---

## Layer 6: Enemy Population

### Two-pass placement (both modes)

**Pass 1 — Fixed spawns**: `spawn` lines from the .zone file placed first. Non-negotiable.

**Pass 2 — Budget-controlled procedural spawns**:

For structured regions, chunk `spawn_slot` entries are resolved subject to a region budget:

```
function populate_enemies(region, placed_chunks, fixed_spawns, rng):
    combat_cells = count_combat_cells(placed_chunks)
    density = difficulty_to_density(region.difficulty_avg)
    budget = combat_cells / density
    budget -= count_fixed_spawns_in_region(fixed_spawns, region)

    if budget <= 0: return

    all_slots = collect_spawn_slots(placed_chunks)
    shuffle(all_slots, rng)

    spawned = 0
    for slot in all_slots:
        if spawned >= budget: break
        if rng_float(rng) < slot.probability:
            if no_enemy_within(slot.world_pos, MIN_ENEMY_SPACING):
                spawn_enemy(slot.enemy_type, slot.world_pos)
                spawned++
```

For open regions, enemies are placed at random valid positions with spacing:

```
function populate_enemies_open(region, placed_obstacles, rng):
    open_area = compute_open_area(region)  // Total non-wall cells
    density = difficulty_to_density(region.difficulty_avg)
    budget = open_area / density * region.combat_ratio

    spawned = 0
    attempts = 0
    while spawned < budget and attempts < MAX_ATTEMPTS:
        x = rng_range(region bounds, rng)
        y = rng_range(region bounds, rng)
        if point_in_region(x, y) and not cell_occupied(x, y):
            if no_enemy_within(world_pos(x, y), MIN_ENEMY_SPACING):
                enemy_type = pick_enemy_type(region.biome, region.difficulty, rng)
                spawn_enemy(enemy_type, world_pos(x, y))
                spawned++
                attempts = 0
        attempts++
```

### Danger budget (future)

When multiple enemy types exist, each carries a danger rating. Budget becomes danger-weighted.

### Pacing tags

| Pacing | Density multiplier |
|--------|-------------------|
| HOT | 1.5× |
| NEUTRAL | 1.0× |
| COOL | 0.5× |

---

## Layer 7: Resource Distribution

- **Fragment drops**: Per-enemy-type, unchanged from current system.
- **Bonus pickups**: Spawn in treasure-category chunks (structured) or near obstacle clusters (open). Tuned by spawn_slot probability.
- **Pickup density**: Inversely correlates with zone difficulty.

No new systems needed — tuning numbers on existing mechanics.

---

## Architecture Integration

### Existing systems

| System | Role | Changes |
|--------|------|---------|
| Zone files (.zone) | Fixed skeleton + regions + hotspots | Add new line types |
| Map grid | Target for generator writes | None — Map_set_cell / Map_clear_cell exist |
| Cell types + cell pool | Chunks reference types by name | None (diagonals = new type, added separately) |
| God mode | Author content | New editing tools (separate spec) |
| Entity spawns | Fixed spawns respected by budget | None |
| Fragment/progression | Resource layer | None |

### New systems

| System | Files | Complexity | Notes |
|--------|-------|-----------|-------|
| PRNG | `prng.c/h` | Trivial | xoshiro128** or PCG |
| Chunk loader | `chunk.c/h` | Medium | Parse, validate, build library |
| Obstacle loader | Part of chunk.c | Low | Parse obstacle block files |
| Hotspot resolver | `procgen.c/h` | Low-Medium | Weighted selection, distance enforcement |
| Region parser | Extension to `zone.c` | Low | New line types |
| Structured generator | `procgen.c/h` | High | Walk, multi-cell chunks, fill, stitch |
| Open generator | `procgen.c/h` | Medium | Scatter algorithm, spacing |
| Enemy populator | `procgen.c/h` | Medium | Budget, spacing, pacing |
| Border sealer | `procgen.c/h` | Low | Wall off region edges |

### Generation call sequence

```
Zone_load("fire_zone.zone"):
    1. Parse fixed assets (anchor walls, ability gates, fixed landmarks)
    2. Parse regions, hotspots, landmark defs, connections
    3. Initialize PRNG with seed (required field)
    4. Resolve hotspots → assign landmark types, stamp landmark chunks
    5. Resolve region connections → map landmark types to grid positions
    6. For each region:
       IF structured:
         a. Compute coarse grid + validity mask
         b. generate_solution_path() → path
         c. fill_non_path() → full grid
         d. stamp_all_chunks() → map cells + enemies
       IF open:
         a. generate_open_region() → scatter walls + enemies
    7. Gameplay begins
```

### Zone file format (complete example)

```
name Fire Zone
size 1024

# Cell type definitions
celltype fire_wall 200 50 0 255 255 100 0 255 none
celltype fire_circuit 150 30 0 255 200 80 0 255 circuit
celltype fire_diagonal_ne 200 50 0 255 255 100 0 255 diagonal_ne

# Anchor walls (always present)
cell 500 500 fire_wall
cell 500 501 fire_wall
cell 501 500 fire_circuit

# Fixed enemy spawns
spawn mine 50000.0 50000.0

# === PROCEDURAL GENERATION ===

seed 48291

# Regions
region A 100 100 320 320
region A 100 340 512 128
region B 500 100 400 400
region C 200 600 200 200

# Generation mode per region
regionmode A structured 16
regionmode B open
regionmode C structured 32

# Generic hotspots (untyped — generator assigns landmark types)
hotspot 120 120 1.0
hotspot 380 200 1.0
hotspot 600 150 1.2
hotspot 200 400 1.0
hotspot 700 350 0.8
hotspot 300 650 1.0

# Landmark types to place
landmark entry_portal portal_entry_fire.chunk 1
landmark boss_arena boss_arena_fire.chunk 3
landmark exit_portal portal_exit_fire.chunk 2
landmark safe_zone safe_zone_fire.chunk 4

# Fixed landmark (optional — locked position)
fixed_landmark entry_portal portal_entry_fire.chunk 100 100

# Hotspot separation
hotspot_min_distance 5

# Region connections
connect A entry entry_portal
connect A exit boss_arena
connect B entry boss_arena
connect B exit exit_portal
connect C entry safe_zone
connect C exit none

# Region constraints
regionconfig A 12 20 0.4 2 3 labyrinthine hot
regionconfig B 20 40 0.6 3 4 scattered hot
regionconfig C 4 8 0.2 1 2 linear cool

# Open region density
opendensity B 0.15 8.0 0.02
```

---

## Design Philosophy

### Why hybrid over pure procedural?

1. **Identity**: Players recognize hand-crafted landmark geometry — the boss arena's shape, the portal room's design. These persist across runs even though their *positions* vary. Hotspots make every run feel familiar yet different.
2. **Progression integrity**: Boss arenas and ability gates need hand-tuned geometry. Hotspots let position vary while design stays precise.
3. **Pacing control**: The designer controls macro flow (portal → gauntlet → boss). The generator controls micro flow (corridors, cover, enemy combos).
4. **Scale**: 12+ zones at 1024×1024 is an enormous authoring surface. Procedural generation is the only way to fill it with interesting content without a team of level designers.

### Why two generation modes?

Hybrid's world is cyberspace — it's not all corridor crawling. Some areas are vast open digital plains with scattered security programs. Others are dense labyrinthine data structures. One algorithm can't serve both:

- **Structured mode** excels at: corridors, room-and-corridor dungeons, labyrinths, mazes, navigational challenges
- **Open mode** excels at: battlefields, arenas, sparse cyberspace voids, areas where enemies are the challenge and terrain is just cover

The region's `mode` field picks the right tool for each area.

### The endgame loop

Seed-based generation + hotspot-randomized landmark positions = a game that stays fresh after mastery. A maxed-out Hybrid doesn't replay memorized rooms — the world regenerates. Daily/weekly challenge seeds, leaderboard runs on shared seeds, multiplayer on deterministic worlds — all fall out of this architecture.

---

## Open Questions

1. **Chunk authoring workflow**: Separate `.chunk` files in `resources/chunks/<biome>/`, or repurpose god mode? Lean toward `.chunk` files + a minimal chunk editor in god mode.

2. **Generation timing**: Seed per save file (metroidvania — stable layout per playthrough), seed per zone entry (roguelike — fresh every visit), or seed changes at endgame milestones (new game+)? Start with seed per save file, layer endgame rotation later.

3. **Difficulty gradient within structured regions**: Weight chunk difficulty by distance along the solution path? `effective_difficulty = base + (path_position / path_length) * gradient`. Adds tension curve.

4. **Chunk rotation vs. mirroring**: Full 90° rotation changes LR → TB, which could auto-generate vertical chunks from horizontal ones. More complex exit config matching. Recommend mirroring only for v1.

5. **Exit alignment simplification**: For v1, standardize exit offsets per chunk size class. All 16×16 chunks with a left exit at the same offset. Stitching becomes trivial. Relax later for variety.

6. **Open mode traversability validation**: Current spec says traversability is trivially guaranteed by low density. Worth adding a cheap flood fill check after scatter as a safety net? Probably yes — it's O(n) and catches edge cases.

7. **Chunk authoring volume**: The system needs ~50-100 chunks across biomes + sizes to produce interesting variety. Fastest path: start with ~10 hand-written chunks, build procgen pipeline, invest in tooling once authoring volume ramps up.

8. **Multi-cell chunk stitching**: When a 32×32 chunk sits next to a 16×16 chunk, the exit on the large chunk's edge spans the full edge of the small chunk's side. Exit width matching needs careful handling. Define conventions per size-class pair.
