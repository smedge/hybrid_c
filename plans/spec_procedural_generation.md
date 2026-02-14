# Spec: Procedural Level Generation — A Hybrid Approach

## Summary & Implementation Reality

### What this system does

You build ~30% of each zone by hand in god mode — the landmarks, boss arenas, portals, ability gates, and key corridors that give the zone its identity. The algorithm procedurally fills the remaining ~70% — combat rooms, corridors, enemy placements, cover geometry — differently every time, controlled by a seed. Same seed = same world, deterministically. This is non-negotiable: seed-based determinism is a first-class requirement, not optional. It enables reproducible worlds, shared seeds between players, and is the foundation for future multiplayer where all clients must agree on world state.

The content authoring investment (chunk templates) is a fraction of hand-coding every corridor and combat room across 12+ zones. You author 50-100 reusable building blocks once; the algorithm assembles them into thousands of unique layouts. The payoff scales — every new chunk template improves every zone that uses it. And critically, this is what makes the endgame loop work: a maxed-out Hybrid doesn't just replay memorized rooms. The world regenerates. Seed-based daily/weekly challenges, leaderboard runs on shared seeds, multiplayer on deterministic worlds — all of that falls out of this architecture.

### Who does what

**Jason (designer) is responsible for:**
- Authoring zone skeletons in god mode (fixed landmarks, boss arenas, portals, ability gates)
- Defining region rectangles and constraints in zone files (text editing for v1)
- Authoring chunk templates (the reusable building blocks — 50-100 across all biomes)
- Authoring obstacle blocks (small sub-patterns — 15-30 generic + biome-specific)
- Tuning: difficulty numbers, combat ratios, pacing tags, density tables, probabilistic cell percentages
- Deciding open questions (generation timing, difficulty gradients, chunk size)

**Bob (code) is responsible for:**
- Chunk file parser + loader with validation (exit traversability, safety rule enforcement)
- Obstacle block parser + loader
- Seedable PRNG (xoshiro128** or PCG — ~10 lines of C)
- Region parser (extension to existing zone.c)
- Solution path walk algorithm (the hard part — Spelunky-style with direction biasing and backtracking)
- Non-path fill algorithm (Dead Cells-style constraint satisfaction)
- Chunk stamper (template resolution: probabilistic cells, obstacle zones, mirroring, Map_set_cell)
- Exit stitcher (aligning openings between adjacent chunks)
- Enemy population system (budget-capped, spacing-aware, pacing-modulated)
- Region border sealing
- Integration into Zone_load() call sequence

### Prerequisites before implementation can start

1. **At least one more enemy type** — The generator places enemies by type. With only mines (passive, stationary), procedural enemy placement isn't very interesting. We need at least one active security program (something that hunts the player) to make combat rooms meaningful. Without this, we can still build and test the geometry generation, but enemy population is hollow.

2. **God mode Phase 3 or a chunk editing workflow** — Chunks need to be authored somewhere. Options:
   - (a) Write .chunk files by hand in a text editor. Workable but tedious and error-prone for spatial layouts.
   - (b) Build a minimal chunk editor mode in god mode — edit an 8×8 grid, mark exits, save as .chunk file. This is the higher-investment option but pays for itself quickly once you're authoring 50+ chunks.
   - (c) Author chunks as tiny .zone files, then convert. Hacky but gets us started with existing tools.

   We can start coding the procgen engine (parser, walk algorithm, stamper) before chunks exist — just use hardcoded test chunks. But real playtesting needs real chunks, and that needs a workflow.

3. **Zone transition / portal system** — Procedural generation is most valuable when there are multiple zones to generate. Without zone transitions, we're generating one zone. Still useful for testing, but the full system only shines when you traverse a world graph of zones, each with its own biome and chunk library.

### Implementation order (incremental, tight feedback loop)

The system produces zero visible output until several pieces work together. To avoid a long blind-coding stretch, I'd build it in this order, getting something on screen at each step:

| Step | What | Visible result |
|------|------|---------------|
| 1 | PRNG + chunk file parser + hardcoded test chunks | Can load and print chunk data (console) |
| 2 | Chunk stamper (no walk, just stamp one chunk at a known position) | See a single chunk's walls appear on the map |
| 3 | Region parser + coarse grid + straight-line path (entry→exit, no randomness) | See a corridor of chunks connecting two points |
| 4 | Solution path walk (random direction, biased) | See winding paths through regions |
| 5 | Non-path fill (sealed + connected rooms) | See full regions filled with rooms |
| 6 | Probabilistic cells + obstacle zones + mirroring | See variation between same-seed and different-seed loads |
| 7 | Exit stitching (if not using standardized offsets) | Clean connections between chunks |
| 8 | Enemy population with budget system | See enemies distributed through generated rooms |
| 9 | Tuning pass | Tweak densities, bias weights, chunk variety |

Steps 1-3 could happen in a single session. Steps 4-5 in another. Steps 6-8 in a third. Step 9 is ongoing.

### Complexity assessment

| Component | Difficulty | Risk | Notes |
|-----------|-----------|------|-------|
| PRNG | Trivial | None | 10 lines of C, well-known algorithms |
| Chunk parser | Easy | None | Same pattern as zone parser — line-based text |
| Chunk stamper | Easy | Low | Map_set_cell in a loop with coordinate transforms. Off-by-one risk with mirroring. |
| Region parser | Easy | None | Bolt onto zone.c |
| Solution path walk | Medium-Hard | Medium | The core algorithm. Backtracking state management is where bugs hide. The pseudocode in this spec is detailed enough to implement from, but testing edge cases (stuck walks, boundary handling, dead-end regions) will take care. |
| Non-path fill | Medium | Low | Straightforward once the path generator works |
| Exit stitching | Medium | Low | Simplified to near-trivial if we standardize exit offsets (recommended for v1) |
| Enemy population | Medium | Low | Arithmetic on existing spawn code, budget cap logic |
| Loader validation (flood fill) | Medium | Low | Standard BFS/DFS on a small grid (8×8 max). Must be correct — a false positive means broken chunks ship. |
| Integration into Zone_load | Low | Low | Call procgen after fixed asset parsing. Clean separation. |

**Overall: this is a medium-large feature, not a moonshot.** The hardest single piece (solution path walk) is well-specified. The rest is plumbing. No new rendering code, no new GL calls, no shader changes. It's all game logic writing into existing map infrastructure.

---

## Inspiration

Two key references:

1. **Dead Cells** — [Building the Level Design of a Procedurally Generated Metroidvania](https://www.gamedeveloper.com/design/building-the-level-design-of-a-procedurally-generated-metroidvania-a-hybrid-approach-). Six-layer hybrid: fixed world framework → hand-designed room templates → conceptual level graphs → algorithmic room placement → enemy population → loot distribution. Key takeaway: algorithms serve hand-crafted constraints, not the other way around.

2. **Spelunky** — [Part 1: Solution Path](https://tinysubversions.com/spelunkyGen/), [Part 2: Room Templates](https://tinysubversions.com/spelunkyGen2/). Key takeaways:
   - **Solution path first**: Don't generate then validate. Build the critical path first, fill around it. Reachability is guaranteed by construction, not checked after the fact.
   - **Exit configurations**: Classify rooms by which sides have guaranteed openings. The path walk selects rooms whose exits match the walk direction — cascading constraints propagate automatically.
   - **Hierarchical templates**: Room → probabilistic tiles → obstacle blocks. Multiple levels of randomization within a single template multiply variety without multiplying authoring cost.
   - **Mirroring**: Flip rooms horizontally for free variety doubling.

Our model: **the designer places the skeleton in god mode (landmarks, boss arenas, portals, gates). The algorithm fills procedural regions between them, guaranteeing a solution path through each region before filling non-path space.**

---

## Definitions

These terms are used precisely throughout the spec.

| Term | Definition |
|------|-----------|
| **Zone** | A complete playable map (128×128 grid of 100-unit cells). One .zone file per zone. |
| **Map cell** | A single cell in the zone grid. 100×100 world units. Coordinates are 0-indexed (0,0) to (127,127). |
| **Fixed asset** | Any map cell, spawn point, or portal hand-placed by the designer. Stored in the .zone file. Never modified by the generator. |
| **Region** | A rectangular area of the zone grid designated for procedural generation. Defined by origin (grid_x, grid_y) and size (width, height) in map cells. Must not overlap fixed assets or other regions. |
| **Chunk** | A reusable room/corridor template. A small grid of cell data (e.g., 8×8) with metadata describing its exits, category, and variation rules. |
| **Coarse grid** | A region subdivided into chunk-sized cells. If a region is 40×40 map cells and the chunk size is 8×8, the coarse grid is 5×5. Each coarse cell holds exactly one chunk. |
| **Exit** | An opening on a chunk's edge that guarantees traversability to/from that side. Defined by side (N/S/E/W), offset along that edge, and width in cells. |
| **Exit config** | The set of sides on which a chunk has exits. Written as a combination of L/R/T/B (e.g., "LR", "LRTB", "None"). Determines where the chunk can appear on the solution path. |
| **Solution path** | A sequence of coarse grid cells from a region's entry to its exit, where every adjacent pair of chunks has matching exits on their shared edge. Guarantees the player can traverse the region. |
| **Critical cells** | Map cells within a chunk that lie on an internal path connecting all of that chunk's exits. These must never be walled off — they are what makes the exit config's guarantee real. |
| **Seed** | A uint32 stored in the zone file, initializing the PRNG. Same zone + same seed = identical generated output. Required field — determinism is foundational for multiplayer, shared seeds, and reproducibility. The PRNG call order must be fixed (no conditional rolls based on runtime state) or downstream generation diverges. |

---

## Layer 1: Zone Skeleton (Hand-Authored)

The designer authors a zone's **skeleton** in god mode: the structural elements that persist across every generated instance of that zone.

### Fixed element types

| Element | Purpose | Invariant |
|---------|---------|-----------|
| **Anchor walls** | Landmark geometry the player recognizes | Always present, never overwritten |
| **Portals** | Zone entry/exit points | Position fixed — defines world graph connectivity |
| **Boss arenas** | Enclosed areas for designed encounters | Geometry hand-tuned for the fight |
| **Ability gates** | Gaps/walls requiring specific subroutines | Enforce progression order |
| **Safe zones** | Spawn points, save stations | Must be reachable from portal |
| **Region markers** | Rectangles designating procedural fill areas | Define where the generator operates |

### What the skeleton is NOT

The skeleton does not contain corridors connecting landmarks to each other (unless the designer explicitly wants a fixed corridor). The space *between* landmarks is where procedural regions live.

### Design constraint

Regions must not overlap fixed assets. The generator treats every map cell within a region's rectangle as available for writing, *except* cells already occupied by fixed assets. The zone loader must validate this on load and reject zones where regions collide with fixed cells. This is a hard error, not a soft fallback — the designer must fix the zone file.

---

## Layer 2: Chunk Templates

### Chunk data structure

```c
typedef struct {
    char name[64];               // Unique identifier, e.g. "combat_pillars_01"
    int width, height;           // Grid dimensions in map cells (e.g. 8, 8)

    // Classification
    ChunkCategory category;      // CHUNK_COMBAT, CHUNK_CORRIDOR, CHUNK_JUNCTION, etc.
    char biome[32];              // "neutral", "fire", "ice", etc.
    int difficulty;              // 1-5
    uint8_t flags;               // CHUNK_NO_HMIRROR, CHUNK_NO_VMIRROR

    // Exit configuration
    ExitConfig exit_config;      // Bitmask: EXIT_LEFT | EXIT_RIGHT | EXIT_TOP | EXIT_BOTTOM
    Exit exits[4];               // One per side max. Offset + width along that edge.
    int exit_count;

    // Cell data — three arrays, each sparse (only populated cells listed)
    ChunkWall walls[MAX_CHUNK_CELLS];       // Guaranteed solid
    int wall_count;
    ChunkEmpty empties[MAX_CHUNK_CELLS];    // Guaranteed open (critical path + exits)
    int empty_count;
    ChunkMaybe maybes[MAX_CHUNK_CELLS];     // Probabilistic (resolved at generation time)
    int maybe_count;

    // Obstacle zones
    ObstacleZone obstacle_zones[MAX_OBSTACLE_ZONES];
    int obstacle_zone_count;

    // Enemy spawn slots
    ChunkSpawnSlot spawn_slots[MAX_CHUNK_SPAWNS];
    int spawn_slot_count;
} ChunkTemplate;
```

```c
typedef struct {
    Direction side;   // DIR_NORTH, DIR_SOUTH, DIR_EAST, DIR_WEST
    int offset;       // Cell offset along edge (0-indexed from top-left corner of that edge)
    int width;        // Opening width in cells (typically 2-4)
} Exit;
```

### Exit configurations

A chunk's exit config is a bitmask of which sides have guaranteed openings:

```c
typedef enum {
    EXIT_NONE   = 0,
    EXIT_LEFT   = 1 << 0,   // 0x01
    EXIT_RIGHT  = 1 << 1,   // 0x02
    EXIT_TOP    = 1 << 2,   // 0x04
    EXIT_BOTTOM = 1 << 3,   // 0x08
} ExitConfig;
```

Common configurations:

| Config | Bitmask | Use |
|--------|---------|-----|
| LR | 0x03 | Horizontal corridor |
| TB | 0x0C | Vertical shaft |
| LRB | 0x07 | Downward fork |
| LRT | 0x06 | Upward fork |
| LRTB | 0x0F | Crossroads / junction |
| L | 0x01 | Dead-end (from left) |
| R | 0x02 | Dead-end (from right) |
| T | 0x04 | Dead-end (from top) |
| B | 0x08 | Dead-end (from bottom) |
| None | 0x00 | Sealed filler room |

**The guarantee**: If a chunk has `EXIT_LEFT | EXIT_RIGHT`, there exists a walkable path from the left exit opening to the right exit opening through the chunk's interior. This path consists entirely of cells marked `empty` in the template. The designer must ensure this when authoring — the chunk loader should validate it (flood fill from each exit, verify all other exits are reachable).

### Critical cells and the probabilistic safety rule

**Critical cells** are the map cells that form the internal paths connecting a chunk's exits. These are marked `empty` in the template and must never be obstructed.

**The rule**: `maybe` cells must never be placed on critical cells. A `maybe` cell that rolls "solid" on a critical path would break the exit guarantee. The chunk file format enforces this — `empty` and `maybe` are distinct markers, and the chunk loader validates that every `empty` cell lies on a path between exits (flood fill validation, same as above).

Cells not marked `wall`, `empty`, or `maybe` default to **solid** (walls). This means the designer only needs to mark openings and variations — everything else is structure. This matches Spelunky's approach: templates are mostly wall with carved-out rooms.

### Three-tier cell resolution

When a chunk is stamped into the map during generation:

1. `wall` cells → `Map_set_cell(x, y, cell_type)` — always placed
2. `empty` cells → left empty (no cell set) — always open
3. `maybe` cells → roll the PRNG against probability P. If hit, place the cell. If miss, leave empty.
4. Obstacle zones → pick one pattern from the pool (or empty), stamp it. See below.
5. All unmarked cells within the chunk bounds → `Map_set_cell(x, y, default_wall_type)` — fill with walls

The result: the chunk's exterior is solid wall, its exits and critical paths are open, its interior features vary per generation.

### Obstacle blocks

An obstacle block is a small pre-authored sub-pattern (e.g., 3×3 L-shaped wall, 2×2 pillar, 4×1 barrier) that can be stamped into a designated rectangular zone within a chunk.

```c
typedef struct {
    char name[64];
    int width, height;
    ChunkWall cells[MAX_OBSTACLE_CELLS]; // All cells are solid walls — obstacle blocks are purely structural
    int cell_count;
} ObstacleBlock;
```

```c
typedef struct {
    int x, y;                    // Top-left corner of the zone within the chunk (local coords)
    int width, height;           // Zone dimensions (must be >= largest block in pool)
    char pool[MAX_POOL][64];     // Names of eligible obstacle blocks (includes "empty" as an option)
    int pool_count;
    float probability;           // Chance of placing any block (vs. leaving zone empty)
} ObstacleZone;
```

Resolution: roll PRNG against `probability`. If hit, pick uniformly from `pool` and stamp the block at the zone's origin. If miss (or if "empty" is selected), leave the zone clear.

**Safety constraint**: Obstacle zones must not overlap with `empty` (critical) cells. The chunk loader validates this. An obstacle block can never break a chunk's exit traversability guarantee.

### Chunk mirroring

After selecting a chunk template and before stamping it into the map, the generator may apply a mirror transform:

- **Horizontal (left↔right)**: For each cell at local (x, y), remap to (width - 1 - x, y). Swap EXIT_LEFT ↔ EXIT_RIGHT. Remap exit offsets on left/right sides accordingly.
- **Vertical (top↔bottom)**: For each cell at local (x, y), remap to (x, height - 1 - y). Swap EXIT_TOP ↔ EXIT_BOTTOM.
- **Both**: Apply horizontal then vertical. Equivalent to 180° rotation.

Mirroring is a PRNG coin flip per chunk placement (50% chance of each axis, independent). Chunks flagged `CHUNK_NO_HMIRROR` or `CHUNK_NO_VMIRROR` skip the corresponding flip.

Exit position remapping for horizontal mirror on a chunk of width W:
```
For an exit on the LEFT side at offset O, width D:
  → becomes an exit on the RIGHT side at offset O, width D
For an exit on the RIGHT side at offset O, width D:
  → becomes an exit on the LEFT side at offset O, width D
Top/Bottom exits: offset remaps to (W - 1 - (O + D - 1)), i.e., mirror the span along the edge
```

Vertical mirror is analogous with height H and top↔bottom.

### Intra-chunk variation summary

A single chunk template authored by the designer produces many distinct map instances through:

1. **Probabilistic cells** (~20-30% of interior cells) — coin flips per cell
2. **Obstacle zone selection** — one of N blocks or empty per zone
3. **Mirroring** — up to 4 orientations (identity, H, V, HV)

For a chunk with 10 `maybe` cells (each 50%), 2 obstacle zones (each with 4 options), and 4 mirror states: 2^10 × 4 × 4 × 4 = **65,536** distinct instances from one authored template. In practice not all are perceptibly different, but the variety is enormous.

### Chunk file format

```
# Metadata
chunk combat_pillars_01
size 8 8
category combat
biome neutral
difficulty 2
flags no_vmirror

# Exit configuration
exits LR
exit left 3 2       # Opening on left edge: starts at row 3, 2 cells tall
exit right 3 2       # Opening on right edge: starts at row 3, 2 cells tall

# Guaranteed walls (local coords, 0-indexed from top-left)
wall 0 0 solid
wall 1 0 solid
wall 2 0 solid
wall 3 0 solid
wall 4 0 solid
wall 5 0 solid
wall 6 0 solid
wall 7 0 solid
wall 0 7 solid
wall 1 7 solid
# ... (perimeter)

# Critical path cells — guaranteed open
empty 0 3
empty 0 4
empty 1 3
empty 1 4
empty 2 3
empty 2 4
# ... (path from left exit to right exit)
empty 7 3
empty 7 4

# Probabilistic cells — resolved at generation time
maybe 2 2 solid 0.5
maybe 5 2 solid 0.5
maybe 2 5 solid 0.5
maybe 5 5 solid 0.5

# Obstacle zones
obstacle_zone 3 1 3 2 pillar_cluster,wall_segment,empty 0.7
obstacle_zone 3 5 3 2 pillar_cluster,barrier,empty 0.7

# Enemy spawn slots (probabilistic)
spawn_slot 4 3 mine 0.3
spawn_slot 4 4 mine 0.3
```

**Parsing rules:**
- Lines starting with `#` are comments
- Blank lines ignored
- Unrecognized lines are a parse error (strict — no silent ignoring)
- `wall`, `empty`, `maybe` coordinates are local to the chunk (0,0 = top-left)
- All unmarked cells within the chunk bounds default to solid wall
- The `exits` line is the bitmask declaration; `exit` lines are the physical openings
- `exit <side> <offset> <width>` — offset is from top-left of that edge, measured in cells

**Loader validation (hard errors, not warnings):**
1. Every side in `exits` has at least one corresponding `exit` line
2. Every `exit` opening cell falls within the chunk bounds
3. Every `exit` opening cell is marked `empty` (not wall, not maybe)
4. Flood fill from any exit reaches all other exits, traversing only `empty` cells
5. No `maybe` cell overlaps an `empty` cell
6. No obstacle zone overlaps an `empty` cell
7. Obstacle zone dimensions can contain every block in its pool

---

## Layer 3: Region Graph

### Region definition

A **region** is a rectangular area within the zone grid reserved for procedural generation.

```c
typedef struct {
    char id[16];                 // "A", "B", "C", etc.
    int grid_x, grid_y;         // Top-left corner in map cell coordinates
    int width, height;           // Size in map cells

    // Coarse grid dimensions (computed: width / chunk_size, height / chunk_size)
    int cols, rows;

    // Constraints
    int min_chunks, max_chunks;  // Total chunks to place (path + fill)
    float combat_ratio;          // 0.0-1.0, fraction of non-path chunks that should be combat
    int difficulty_min, difficulty_max;
    RegionStyle style;           // STYLE_LINEAR, STYLE_LABYRINTHINE, STYLE_BRANCHING
    PacingTag pacing;            // PACING_HOT, PACING_COOL, PACING_NEUTRAL

    // Connections to fixed skeleton
    RegionConnection entry;      // Where this region connects to its "source" landmark
    RegionConnection exit;       // Where this region connects to its "destination" landmark
} Region;
```

```c
typedef struct {
    Direction side;              // Which side of the region the connection is on
    int offset;                  // Cell offset along that side where the opening is
    int width;                   // Opening width in cells
    char landmark_id[32];        // Which fixed landmark this connects to (e.g., "Portal_In", "Boss_Arena")
} RegionConnection;
```

### Coarse grid sizing

The region's rectangle must be an exact multiple of the chunk size. If the designer defines a 40×32 region and chunk size is 8, the coarse grid is 5×4 — clean division. If it's not evenly divisible, the zone loader rounds down and emits a warning. The remaining cells along the edges become solid wall (padding).

Chunk size is a global constant for the zone (or the entire game). Suggested starting value: **8×8 map cells** (800×800 world units). This is large enough for meaningful room interiors, small enough for fine-grained path variation.

### Region connections and the entry/exit problem

Each region has an **entry** and an **exit** — openings on its rectangular boundary that connect to fixed landmarks (or other regions). These define where the solution path must start and end within the coarse grid.

The entry/exit connections map to specific coarse grid cells:
- Entry on the **left** side at offset 16 (chunk size 8) → coarse grid cell (0, 2) — column 0, row 16/8=2
- Exit on the **bottom** side at offset 24 → coarse grid cell (3, rows-1) — column 24/8=3, last row

The solution path walk starts at the entry coarse cell and must reach the exit coarse cell.

### Region graph in the zone file

```
# Region definitions
# region <id> <grid_x> <grid_y> <width> <height>
region A 16 16 40 40
region B 64 16 32 48
region C 16 72 24 24

# Region connections (which side of the region, offset along that side, width of opening)
# connect <region_id> entry <side> <offset> <width> <landmark_id>
# connect <region_id> exit <side> <offset> <width> <landmark_id>
connect A entry left 16 2 Portal_In
connect A exit right 20 2 Boss_Arena
connect B entry left 8 2 Boss_Arena
connect B exit bottom 16 2 Portal_Out
connect C entry top 8 2 region_A_branch
connect C exit none 0 0 none              # Dead-end region: no exit, path terminates

# Region constraints
# regionconfig <id> <min_chunks> <max_chunks> <combat_ratio> <diff_min> <diff_max> <style> <pacing>
regionconfig A 12 20 0.4 2 3 labyrinthine hot
regionconfig B 6 10 0.2 3 4 linear cool
regionconfig C 4 6 0.6 2 2 branching neutral
```

**Dead-end regions**: A region can have `exit none`, meaning the solution path only needs to reach the entry — the interior is a branch, not a throughway. The walk algorithm places chunks inward from the entry and stops when the min_chunks constraint is met.

**Region-to-region connections**: If region A has a branch connection to region C, region A must have a chunk on its boundary with an exit facing region C's entry. This is an additional constraint on chunk selection at that coarse grid cell.

---

## Layer 4: Algorithmic Assembly

### Phase 1: Solution path generation

This is the core algorithm. It runs once per region, producing a guaranteed-traversable sequence of chunks from entry to exit.

**Inputs:**
- Region definition (bounds, coarse grid dimensions, entry cell, exit cell)
- Chunk template library (filtered by biome and difficulty range)
- Seeded PRNG

**Output:**
- An array of (coarse_col, coarse_row, ChunkTemplate*, mirror_flags) for each cell on the solution path

**Algorithm (pseudocode):**

```
function generate_solution_path(region, chunk_lib, rng):
    path = []
    visited = 2D bool array [region.cols × region.rows], all false

    // Determine entry and exit coarse grid cells
    entry_cell = map_connection_to_coarse_cell(region.entry, region)
    exit_cell  = map_connection_to_coarse_cell(region.exit, region)
    // For dead-end regions (exit == none), exit_cell = INVALID

    current = entry_cell
    visited[current] = true

    // The entry chunk must have an exit on the side facing the region's entry connection
    required_entry_exit = opposite_direction(region.entry.side)
    // e.g., if entry is on the LEFT side of the region, the entry chunk needs EXIT_LEFT
    // (the opening faces outward toward the landmark)
    // Wait — the entry is on the region's left side, so the chunk at that position needs
    // an opening on its LEFT to connect out. Actually: the entry connection is on the
    // region boundary. The chunk sitting at that boundary cell needs an exit on the
    // side facing the boundary. If entry is "left side of region", the chunk needs EXIT_LEFT.

    prev_required = required_entry_exit  // First chunk must have this exit

    while current != exit_cell:
        // Choose next direction
        direction = pick_walk_direction(current, exit_cell, region, visited, rng)

        if direction == STUCK:
            // All neighbors visited or out of bounds — backtrack
            // Remove last path entry, unmark visited, try a different direction
            // If backtrack exhausts all options at a cell, the region is unsolvable
            // with this path prefix — restart the entire walk (with next PRNG state)
            restart_walk()
            continue

        next_cell = current + direction_offset(direction)

        // Determine exit requirements for the CURRENT chunk:
        //   - It must have prev_required (to connect to the previous chunk or entry)
        //   - It must have an exit in `direction` (to connect to the next chunk)
        current_required_exits = prev_required | direction_to_exit(direction)

        // Select a chunk for `current` that satisfies current_required_exits
        chunk = select_chunk(chunk_lib, current_required_exits, region.constraints, rng)

        if chunk == NULL:
            // No chunk in the library matches these exit requirements + constraints
            // Try a different direction from current cell
            continue  // back to direction selection

        mirror = roll_mirror(chunk, rng)
        path.append({current.col, current.row, chunk, mirror})
        visited[next_cell] = true

        // The NEXT chunk must have an exit facing back toward current
        prev_required = opposite_direction_exit(direction)
        current = next_cell

    // Place the exit chunk (or final chunk for dead-end regions)
    if exit_cell != INVALID:
        required_exit_exit = direction_to_exit(
            side_to_direction(region.exit.side)  // exit faces outward
        )
        exit_required = prev_required | required_exit_exit
    else:
        exit_required = prev_required  // dead-end: just needs the inward-facing exit

    chunk = select_chunk(chunk_lib, exit_required, region.constraints, rng)
    mirror = roll_mirror(chunk, rng)
    path.append({current.col, current.row, chunk, mirror})

    return path
```

**`pick_walk_direction` — direction selection with bias:**

```
function pick_walk_direction(current, target, region, visited, rng):
    // Build list of valid directions (in-bounds, not visited)
    candidates = []
    for dir in [LEFT, RIGHT, UP, DOWN]:
        next = current + offset(dir)
        if in_bounds(next, region) and not visited[next]:
            candidates.append(dir)

    if candidates is empty:
        return STUCK

    // Compute bias weights based on region style
    weights = []
    for dir in candidates:
        if target == INVALID:  // dead-end region, no target
            weights.append(1.0)  // uniform
        else:
            displacement = target - current
            alignment = dot(direction_vector(dir), normalize(displacement))
            // alignment: +1 = toward target, -1 = away, 0 = perpendicular

            switch region.style:
                case LINEAR:
                    weight = (alignment > 0) ? 5.0 : (alignment == 0) ? 1.5 : 0.5
                case LABYRINTHINE:
                    weight = (alignment > 0) ? 2.0 : (alignment == 0) ? 2.0 : 1.5
                case BRANCHING:
                    weight = (alignment > 0) ? 3.0 : (alignment == 0) ? 2.5 : 0.5

            weights.append(weight)

    // Weighted random selection
    return weighted_pick(candidates, weights, rng)
```

**`select_chunk` — chunk selection with constraint relaxation:**

```
function select_chunk(chunk_lib, required_exits, constraints, rng):
    // required_exits is a bitmask: the chunk MUST have at least these exits

    // Pass 1: Full match (exits + category + difficulty)
    pool = filter(chunk_lib, where:
        (chunk.exit_config & required_exits) == required_exits  // has all required exits
        AND chunk.difficulty >= constraints.difficulty_min
        AND chunk.difficulty <= constraints.difficulty_max
        AND chunk.biome == constraints.biome OR chunk.biome == "neutral"
        AND chunk.category matches constraints  // weighted by combat_ratio, etc.
    )
    if pool is not empty:
        return pick_uniform(pool, rng)

    // Pass 2: Relax category (keep exits + difficulty)
    pool = filter(chunk_lib, where:
        (chunk.exit_config & required_exits) == required_exits
        AND chunk.difficulty >= constraints.difficulty_min
        AND chunk.difficulty <= constraints.difficulty_max
        AND chunk.biome == constraints.biome OR chunk.biome == "neutral"
    )
    if pool is not empty:
        return pick_uniform(pool, rng)

    // Pass 3: Relax difficulty (keep exits + biome)
    pool = filter(chunk_lib, where:
        (chunk.exit_config & required_exits) == required_exits
        AND chunk.biome == constraints.biome OR chunk.biome == "neutral"
    )
    if pool is not empty:
        return pick_uniform(pool, rng)

    // Pass 4: Exits only (last resort — any chunk with the right exits)
    pool = filter(chunk_lib, where:
        (chunk.exit_config & required_exits) == required_exits
    )
    if pool is not empty:
        return pick_uniform(pool, rng)

    // Pass 5: No match — the chunk library lacks a template for this exit config
    // This is a content gap. Log a warning, return NULL.
    return NULL
```

**Key invariant**: Connectivity constraints (exit bitmask) are **never** relaxed. Content constraints (category, difficulty, biome) are relaxed in order of importance. If no chunk in the entire library has the required exits, the walk must try a different direction. If all directions fail, the walk backtracks.

### Phase 2: Non-path fill

After the solution path is placed, remaining coarse grid cells are filled.

```
function fill_non_path(region, path_cells, chunk_lib, rng):
    for row in 0..region.rows:
        for col in 0..region.cols:
            if (col, row) in path_cells:
                continue  // already placed

            // Determine what exits this cell COULD have (based on adjacent path chunks)
            desired_exits = 0
            for dir in [LEFT, RIGHT, UP, DOWN]:
                neighbor = (col, row) + offset(dir)
                if neighbor is a path cell or filled cell:
                    neighbor_chunk = get_placed_chunk(neighbor)
                    neighbor_exit_toward_us = opposite_direction_exit(dir)
                    if (neighbor_chunk.exit_config & neighbor_exit_toward_us) != 0:
                        // Neighbor has an exit facing us — we SHOULD have a matching exit
                        // to create a connected side room (optional, not required)
                        desired_exits |= direction_to_exit(dir)

            // Decide: connected side room or sealed filler?
            // Controlled by region style:
            //   labyrinthine → 70% connected, 30% sealed
            //   linear → 20% connected, 80% sealed
            //   branching → 50% connected, 50% sealed
            connect_chance = style_to_connect_chance(region.style)

            if rng_float(rng) < connect_chance AND desired_exits != 0:
                // Connected room — must have matching exits toward neighbors
                chunk = select_chunk(chunk_lib, desired_exits, region.constraints, rng)
            else:
                // Sealed room — exit config NONE
                chunk = select_chunk(chunk_lib, EXIT_NONE, region.constraints, rng)

            if chunk == NULL:
                // Fallback: fill entire coarse cell with solid wall
                fill_solid(region, col, row)
                continue

            mirror = roll_mirror(chunk, rng)
            place_chunk(region, col, row, chunk, mirror, rng)
```

**Why sealed rooms exist**: Spelunky's Type 0 rooms serve a critical purpose — they create the illusion of a larger, more complex space. A sealed room adjacent to the path might share a thin wall with the corridor. The player can see or hear something on the other side but can't reach it. This creates mystery and drives exploration for secret passages (if we later add wall-breaking subroutines).

### Phase 3: Chunk stamping and stitching

Once all coarse grid cells have assigned chunks, stamp them into the map.

```
function stamp_all_chunks(region, placed_chunks, rng):
    chunk_size = CHUNK_SIZE  // e.g., 8

    for each (col, row, template, mirror) in placed_chunks:
        // Compute world-space origin of this chunk in the map grid
        origin_x = region.grid_x + col * chunk_size
        origin_y = region.grid_y + row * chunk_size

        // Resolve and stamp the chunk
        stamp_chunk(origin_x, origin_y, template, mirror, rng)

    // Stitch: ensure exits between adjacent chunks are physically open
    for each pair of adjacent placed chunks (A at col_a,row_a) and (B at col_b,row_b):
        if A has an exit facing B AND B has an exit facing A:
            stitch_exits(region, A, B, col_a, row_a, col_b, row_b)

    // Seal region boundary
    seal_region_border(region, placed_chunks)
```

**`stamp_chunk` — resolve template into map cells:**

```
function stamp_chunk(origin_x, origin_y, template, mirror, rng):
    // First pass: fill entire chunk bounds with default wall
    for y in 0..template.height:
        for x in 0..template.width:
            map_x = origin_x + apply_mirror_x(x, template.width, mirror)
            map_y = origin_y + apply_mirror_y(y, template.height, mirror)
            Map_set_cell(map_x, map_y, default_wall_type)

    // Second pass: carve empties (critical path)
    for each empty in template.empties:
        map_x = origin_x + apply_mirror_x(empty.x, template.width, mirror)
        map_y = origin_y + apply_mirror_y(empty.y, template.height, mirror)
        Map_clear_cell(map_x, map_y)

    // Third pass: resolve probabilistic cells
    for each maybe in template.maybes:
        if rng_float(rng) < maybe.probability:
            map_x = origin_x + apply_mirror_x(maybe.x, template.width, mirror)
            map_y = origin_y + apply_mirror_y(maybe.y, template.height, mirror)
            Map_set_cell(map_x, map_y, maybe.cell_type)
        // else: leave as whatever it is (wall from first pass, or empty from second pass)
        // Note: since maybe cells cannot overlap empty cells (loader validation),
        // a missed maybe roll leaves the default wall from pass 1. This is correct.

    // Fourth pass: resolve obstacle zones
    for each zone in template.obstacle_zones:
        if rng_float(rng) < zone.probability:
            block_name = pick_uniform(zone.pool, rng)
            if block_name != "empty":
                block = lookup_obstacle_block(block_name)
                stamp_obstacle(origin_x, origin_y, zone, block, mirror)

    // Fifth pass: resolve enemy spawn slots
    for each slot in template.spawn_slots:
        if rng_float(rng) < slot.probability:
            map_x = origin_x + apply_mirror_x(slot.x, template.width, mirror)
            map_y = origin_y + apply_mirror_y(slot.y, template.height, mirror)
            // Convert to world coords (map cell center)
            world_x = (map_x + 0.5) * CELL_SIZE
            world_y = (map_y + 0.5) * CELL_SIZE
            spawn_enemy(slot.enemy_type, world_x, world_y)
```

**Exit stitching:**

Two adjacent chunks may have exits facing each other, but the exit openings might not align perfectly (different offsets along the shared edge). Stitching ensures a physical connection:

```
function stitch_exits(region, chunk_a, chunk_b, col_a, row_a, col_b, row_b):
    // Determine shared edge and the two exit definitions
    exit_a = get_exit_on_side(chunk_a, direction_from_a_to_b)
    exit_b = get_exit_on_side(chunk_b, direction_from_b_to_a)

    // Both exits are expressed as (offset, width) along the shared edge.
    // Compute overlap range
    a_start = exit_a.offset
    a_end = exit_a.offset + exit_a.width
    b_start = exit_b.offset
    b_end = exit_b.offset + exit_b.width

    overlap_start = max(a_start, b_start)
    overlap_end = min(a_end, b_end)

    if overlap_end > overlap_start:
        // Exits overlap — clear the wall cells on the shared edge within the overlap
        // The shared edge is the last column of chunk_a / first column of chunk_b (for horizontal adjacency)
        clear_shared_edge(region, col_a, row_a, col_b, row_b, overlap_start, overlap_end)
    else:
        // Exits don't overlap — carve a short L-shaped or diagonal connector
        // Find the midpoint between the two exits and clear a 2-cell-wide path:
        //   from exit_a's center → shared edge → along edge → exit_b's center
        mid = (a_start + a_end + b_start + b_end) / 4
        carve_connector(region, col_a, row_a, exit_a, col_b, row_b, exit_b, mid)
```

For the initial implementation, I'd recommend a simpler constraint: **all chunks of the same width must use the same exit offset for a given side.** If all 8-wide chunks have their left exit at offset 3, width 2, then stitching is trivial — exits always align. This limits design variety slightly but eliminates the stitching problem entirely. You can relax this later when the content library grows.

### Backtracking and failure

The walk algorithm can get stuck (all neighbors visited, no valid chunks for required exits). The recovery strategy:

1. **Local retry**: Try all untried directions from the current cell before giving up.
2. **Backtrack**: Pop the last cell off the path, un-visit it, try alternate directions from the cell before it. Limit backtracking depth to 5 steps (avoids degenerate O(n!) search).
3. **Full restart**: If backtracking fails, discard the entire path and restart the walk with the PRNG advanced. Limit to 10 restarts.
4. **Hard fallback**: If 10 restarts fail, fill the entire region with a simple grid of corridor chunks connecting entry to exit in a straight line. This always works but produces a boring region. Log a warning — this indicates the chunk library is missing needed exit configs for this biome/difficulty.

In practice, with a reasonably stocked chunk library (5+ templates per common exit config), the walk succeeds on the first try >95% of the time.

---

## Layer 5: Enemy Population

### When chunk-level spawns aren't enough

Chunk spawn slots (Layer 2) handle per-room enemy placement. But the region needs global balancing — you don't want 50 mines in a region where the difficulty budget calls for 20.

**Two-pass enemy placement:**

**Pass 1 — Fixed spawns**: All `spawn` lines from the .zone file are placed first. These are hand-authored, non-negotiable.

**Pass 2 — Chunk slot resolution**: Each chunk's `spawn_slot` entries are resolved (probability roll), but subject to a **region budget**:

```
function populate_enemies(region, placed_chunks, fixed_spawns, rng):
    // Count total combat cells in region
    combat_cells = count_cells_in_chunks_with_category(placed_chunks, CHUNK_COMBAT)

    // Compute budget
    density = difficulty_to_density(region.difficulty_avg)  // cells per enemy
    budget = combat_cells / density

    // Subtract fixed spawns within this region's bounds
    fixed_in_region = count_fixed_spawns_in_bounds(fixed_spawns, region)
    remaining_budget = budget - fixed_in_region

    if remaining_budget <= 0:
        return  // Fixed spawns already exceed budget

    // Collect all spawn slots from placed chunks
    all_slots = []
    for each chunk in placed_chunks:
        for each slot in chunk.spawn_slots:
            all_slots.append(slot with world coords computed)

    // Shuffle slots (seeded), then resolve in order until budget exhausted
    shuffle(all_slots, rng)
    spawned = 0
    for each slot in all_slots:
        if spawned >= remaining_budget:
            break
        if rng_float(rng) < slot.probability:
            // Check minimum spacing (no enemy within N cells of another)
            if no_enemy_within(slot.world_pos, MIN_ENEMY_SPACING):
                spawn_enemy(slot.enemy_type, slot.world_pos)
                spawned++
```

### Danger budget (future refinement)

When multiple enemy types exist (beyond mines), each type carries a **danger rating**. The budget becomes danger-weighted:

```
budget_danger = max_danger_for_region(region)
for each slot:
    enemy_danger = danger_rating(slot.enemy_type)
    if current_danger + enemy_danger > budget_danger:
        skip  // would exceed budget
    current_danger += enemy_danger
    spawn(...)
```

### Pacing tags

The region's pacing tag modulates the density multiplier:

| Pacing | Density multiplier | Effect |
|--------|-------------------|--------|
| HOT | 1.5× | More enemies per cell — intense |
| NEUTRAL | 1.0× | Standard density |
| COOL | 0.5× | Sparse enemies — breathing room |

---

## Layer 6: Resource Distribution

After enemies are placed:
- **Fragment drops** are per-enemy-type, unchanged from current system
- **Bonus pickups** spawn in treasure-category chunks. Each treasure chunk has a `spawn_slot` for pickups with tuned probability.
- **Pickup density** inversely correlates with zone difficulty: `pickup_chance = base_chance * (1.0 - 0.15 * difficulty)`

This layer requires no new systems — it's tuning numbers on existing drop mechanics and adding spawn slots to treasure chunk templates.

---

## How This Maps to Our Architecture

### Existing systems and their roles

| System | Role in Procgen | Changes Needed |
|--------|----------------|----------------|
| Zone files (.zone) | Store fixed skeleton + region definitions | Add `region`, `connect`, `regionconfig` line types |
| Map grid (128×128) | Target grid the generator writes into | None — `Map_set_cell` / `Map_clear_cell` already exist |
| Cell types + cell pool | Chunks reference cell types by name | None |
| God mode | Author fixed skeletons and chunk templates | Eventually: chunk editor mode, region marker placement |
| Mine spawns in .zone | Fixed spawns respected by enemy budget | None |
| Fragment/progression | Resource layer | None |
| PRNG | Need a seedable, deterministic PRNG | New — C stdlib `rand()` is not seedable per-instance; need a simple xoshiro or PCG |

### New systems to build

| System | Files | Complexity | Description |
|--------|-------|-----------|-------------|
| Chunk loader | `chunk.c/h` | Medium | Parse .chunk files, build template library, validate exit traversability |
| Obstacle block loader | `obstacle.c/h` (or part of chunk.c) | Low | Parse obstacle definitions, store block library |
| Region parser | Extension to `zone.c` | Low | Parse `region`, `connect`, `regionconfig` lines |
| PRNG | `prng.c/h` | Low | Seedable 32-bit PRNG (xoshiro128** or PCG) |
| Solution path generator | `procgen.c/h` | High | Walk algorithm, direction biasing, chunk selection, backtracking |
| Non-path fill | Part of `procgen.c` | Medium | Fill remaining coarse cells, style-controlled connectivity |
| Chunk stamper | Part of `procgen.c` | Medium | Template resolution (maybe cells, obstacle zones, mirroring), Map_set_cell calls |
| Exit stitcher | Part of `procgen.c` | Medium | Align exits between adjacent chunks, carve connectors |
| Enemy populator | Part of `procgen.c` or `spawn.c` | Medium | Budget-capped, spacing-aware, pacing-modulated spawn resolution |
| Region border sealer | Part of `procgen.c` | Low | Wall off region boundaries |

### Generation call sequence

```
Zone_load("fire_zone.zone"):
    1. Parse fixed assets (cells, spawns, portals) → populate map + entity spawns as today
    2. Parse region definitions → build Region array
    3. Initialize PRNG with seed from zone file (required field)
    4. For each region:
        a. Compute coarse grid dimensions
        b. generate_solution_path(region, chunk_lib, rng) → path
        c. fill_non_path(region, path, chunk_lib, rng) → full coarse grid
        d. stamp_all_chunks(region, placed_chunks, rng) → writes to map + spawns enemies
    5. Normal gameplay begins — the map is fully populated
```

Total generation time for a 128×128 zone with 3 regions of ~25 chunks each: well under a frame at our data volumes. No async loading needed.

### Zone file format (complete)

```
# === FIXED SKELETON (existing format, unchanged) ===
name Fire Zone
size 128
celltype fire_wall 200 50 0 255 255 100 0 255 none
celltype fire_circuit 150 30 0 255 200 80 0 255 circuit
cell 64 64 fire_wall
cell 64 65 fire_wall
cell 64 66 fire_circuit
spawn mine 6400.0 6400.0
portal 0 64 zone_002 127 64

# === PROCEDURAL REGIONS (new) ===
# region <id> <grid_x> <grid_y> <width> <height>
region A 16 16 40 40
region B 64 16 32 48
region C 16 72 24 24

# connect <region_id> <entry|exit> <side> <offset> <width> <landmark_id>
connect A entry left 16 2 Portal_In
connect A exit right 20 2 Boss_Arena
connect B entry left 8 2 Boss_Arena
connect B exit bottom 16 2 Portal_Out
connect C entry top 8 2 region_A_branch
connect C exit none 0 0 none

# regionconfig <id> <min_chunks> <max_chunks> <combat_ratio> <diff_min> <diff_max> <style> <pacing>
regionconfig A 12 20 0.4 2 3 labyrinthine hot
regionconfig B 6 10 0.2 3 4 linear cool
regionconfig C 4 6 0.6 2 2 branching neutral

# chunk_size <cells>  (global for this zone, default 8)
chunk_size 8

# seed <uint32>  (required — deterministic generation, critical for multiplayer)
seed 48291
```

---

## Design Philosophy

### Why hybrid over pure procedural?

1. **Identity**: Players remember hand-crafted landmarks. "The diamond room in the fire zone" persists across every run. Pure procedural zones are forgettable.
2. **Progression integrity**: Ability gates and boss arenas must be precisely designed. A procedural boss arena is a bad boss arena.
3. **Pacing control**: The designer controls macro flow (portal → gauntlet → boss → reward). The generator controls micro flow (which corridors, which enemy combos, which cover layout).
4. **Authorial voice**: Zones should feel *designed*. Fixed assets carry intent; procedural fill adds replayability without diluting that intent.

### Why not just hand-author everything?

1. **Replayability**: A metroidvania you've memorized is a solved metroidvania. Procedural fill means you explore, not remember.
2. **Scale**: 12+ zones with rich interiors is an enormous authoring burden. Procedural generation is a force multiplier.
3. **Reactive gameplay**: Per Dead Cells — emphasize reactive play over learned patterns.

### The 70/30 split

- **~30% fixed** (skeleton, landmarks, bosses, gates, portals)
- **~70% procedural** (corridors, combat rooms, enemy placement, cover layout)

The fixed 30% does 90% of the work making the zone feel authored. The procedural 70% does 90% of the work making the zone feel fresh.

---

## Open Questions

1. **Chunk authoring workflow**: Separate `.chunk` files, or small `.zone` files loaded as templates? Separate format is semantically cleaner and avoids overloading zone parsing with chunk-specific fields. Reusing `.zone` is less new code but muddier. I lean toward separate `.chunk` files in a `resources/chunks/` directory, organized by biome subdirectories (`resources/chunks/fire/`, `resources/chunks/neutral/`, etc.).

2. **Region marker UX in god mode**: How does the designer define regions visually? Options:
   - (a) Edit the .zone file by hand (text editor) — simplest, no god mode changes
   - (b) Special "region marker" cell type rendered as a translucent overlay — place corners in god mode, system infers rectangle
   - (c) Rectangle drag tool in god mode — dedicated UI gesture

   For v1, option (a) is fine. The zone file is human-editable text. Region placement is a design-time decision, not a frequent edit.

3. **Generation timing and the endgame loop**: Since generation is seed-deterministic, the question is *when does the seed change?* Options:
   - **Seed persists per save file** (metroidvania model): Zone generates once on first visit, player maps it, layout is stable for that playthrough. New Game = new seed = new world.
   - **Seed changes per zone entry** (roguelike model): Every visit regenerates. More replayable but minimap/exploration memory resets. Could feel disorienting.
   - **Seed changes on a schedule** (endgame loop): After defeating all bosses / reaching max progression, the world "resets" with a new seed. Fixed landmarks persist, procedural fill regenerates. The maxed-out Hybrid replays a fresh world with all abilities unlocked — now it's about mastery and speed, not progression. Daily/weekly challenge seeds for leaderboard competition.
   - **Hybrid**: Normal playthrough uses a stable seed. Endgame "New Game+" or challenge mode uses rotating seeds.

   This is a major game feel decision. The seed architecture supports all of these — it's a question of when to call `prng_seed()` with a new value. Worth prototyping the simplest option first (seed per save file) and layering endgame rotation later.

4. **Difficulty gradient within a region**: Should chunk difficulty escalate from entry to exit (easier near portal, harder near boss)? Implementation: weight chunk selection by distance along the solution path. `effective_difficulty = base + (path_position / path_length) * gradient`. Adds tension curve at the cost of slightly more complex chunk selection.

5. **Chunk size uniformity**: Should all chunks be the same size (e.g., 8×8), or support mixed sizes (8×8 corridors, 16×16 combat arenas)? Uniform size massively simplifies the coarse grid and stitching. Mixed sizes require a more complex packing algorithm (bin packing on a 2D grid). **Strong recommendation: start with uniform chunk size.** The interior of an 8×8 chunk is 64 cells — plenty of room for interesting combat arenas. If 8×8 feels too cramped for boss-adjacent rooms, bump the global chunk size to 10×10 or 12×12 rather than mixing sizes.

6. **Chunk rotation vs. mirroring**: Full 90° rotation (not just mirroring) would give 8 orientations per template. But rotation changes horizontal corridors into vertical corridors, which changes the exit config. A LR chunk rotated 90° becomes a TB chunk. This could be useful (auto-generate vertical shaft chunks from horizontal corridor chunks) but adds complexity to the exit config matching. **Recommendation: mirroring only for v1.** Rotation can be added later if the chunk library feels thin.

7. **Failure recovery aggressiveness**: The spec defines a 4-level fallback (local retry → backtrack → restart → straight-line fallback). In practice, the straight-line fallback should almost never trigger. If it triggers frequently during development, it means the chunk library has gaps in exit config coverage for that biome/difficulty. Treat it as a content bug, not an algorithm bug.

8. **Exit alignment simplification**: The spec describes a general stitching algorithm for misaligned exits. For v1, enforce a per-chunk-size convention: all 8×8 chunks with a left exit place it at the same offset (e.g., offset 3, width 2). All right exits at the same offset. Top and bottom the same. This makes stitching a no-op (exits always align) and simplifies chunk authoring (you know exactly where the openings go). It does limit variety in exit placement, but for the first pass, correctness and simplicity trump variety.

9. **Chunk authoring in bulk**: What's the fastest way to author 50+ chunks? Possible workflows:
   - Text editor with a visual grid reference (graph paper approach)
   - Minimal chunk editor mode in god mode (edit 8×8 grid, toggle cells, mark exits, save)
   - Write a simple standalone tool that renders chunk files and lets you click-to-toggle cells
   - Start with a small set of ~10 hand-written chunks, build the procgen pipeline, then invest in better tooling once the authoring volume ramps up

   The tooling investment should be proportional to the authoring pain. If writing .chunk files by hand is tolerable for the first 20, don't build an editor until you need 50.
