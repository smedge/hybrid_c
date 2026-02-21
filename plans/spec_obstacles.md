# Spec: Obstacle Blocks

## Overview

Noise-generated terrain in moderate-to-sparse areas produces random-looking scattered wall cells — dots with no tactical meaning. Obstacle blocks replace that randomness with **authored micro-structures**: cover positions, pillar clusters, L-shaped walls, chokepoints, corridors, and random encounters. They're pre-authored patterns (3x3 up to 16x16) scattered by the procgen system into open terrain to give combat encounters spatial structure.

**What they are NOT**: Obstacle blocks are not landmarks (no influence field, no progression gating). They're small-scale terrain and encounter building blocks — wall geometry and optional enemy spawns that shape how the player navigates and fights in the spaces between landmarks.

## The Problem

Right now, the noise terrain generator creates two extremes:
1. **Dense areas** (near dense-influence landmarks): Labyrinthine walls with natural corridors — these feel great already
2. **Sparse/moderate areas**: Scattered individual wall cells that feel arbitrary — no tactical cover, no interesting geometry, just noise dots

Obstacle blocks fill the gap. They give moderate and sparse terrain **authored tactical character** without hand-placing every wall.

## Obstacle Block Format

Obstacle blocks reuse the existing `.chunk` file format from `chunk.c/h`. They're just small chunks — the same `wall`, `empty`, `spawn`, `size` directives, loaded by the same `Chunk_load()` function.

Three additions to the chunk format:
1. **`style`** — `structured` or `organic`. Controls which influence zones the block is placed in. Defaults to `organic` if omitted.
2. **`weight`** — Selection weight for weighted random picking. Higher weight = more likely to be chosen relative to other blocks in the same style pool. Default `1.0` if omitted.
3. **`spawn` probability** — Optional probability field (0.0–1.0) appended to `spawn` lines. Rolled independently per spawn when the obstacle is placed. Default `1.0` (guaranteed) if omitted.

### Examples

```
# Common small pillar — high weight, appears frequently
chunk pillar_2x2
size 2 2
style organic
weight 5.0

wall 0 0 0
wall 1 0 0
wall 0 1 0
wall 1 1 0
```

```
# L-shaped wall with a probable hunter — structured, moderate weight
chunk ambush_corner
size 5 5
style structured
weight 2.0

wall 0 0 0
wall 1 0 0
wall 0 1 0
wall 0 2 0
wall 0 3 0
spawn hunter 4 2 0.6
```

```
# Rare large encounter — low weight, spawns unlikely
chunk patrol_encounter
size 8 8
style organic
weight 0.5

spawn hunter 2 2 0.5
spawn hunter 6 6 0.3
spawn seeker 4 4 0.4
```

**Style values:**
- `structured` — Geometric, deliberate patterns. Symmetry, clean edges, recognizable shapes. Placed near landmarks where terrain feels architectural.
- `organic` — Random-looking, asymmetric clusters. Natural formations. Placed in the wilds, far from landmark influence.

**Weight examples:** A pool with blocks weighted 5.0, 2.0, 2.0, and 1.0 (total 10.0) picks them at 50%, 20%, 20%, and 10% respectively. This lets common small cover pieces dominate while rare large encounters stay special.

### Storage

Obstacle blocks live in `resources/obstacles/`. Separate directory from `resources/chunks/` (which holds landmark rooms and center anchors) to keep them organized.

```
resources/obstacles/
    pillar_2x2.chunk
    pillar_L.chunk
    ambush_corner.chunk
    wall_segment_h.chunk
    wall_segment_v.chunk
    corner_3x3.chunk
    rock_scatter.chunk
    rock_cluster.chunk
    patrol_encounter.chunk
    ...
```

## Authoring

Two workflows, same output:

1. **Godmode chunk export**: Enter godmode, paint walls and place enemies, select the region with Q/E to cycle to the appropriate mode. The godmode tool cycle gains a new **Obstacles** mode alongside the existing Chunks mode. Tab to export — Chunks mode writes to `resources/chunks/`, Obstacles mode writes to `resources/obstacles/`. Edit the exported file afterward to add `style`, `weight`, and probability values on spawn lines.

2. **Text editor**: Write directly — the format is trivial for small patterns.

The obstacle library starts small (10-15 blocks) and grows during Phase 6 content authoring. Each biome may eventually have its own obstacle set, but for now one shared library is fine.

## Zone File Integration

Two new parameters in the zone file control obstacle scatter:

```
obstacle_density 0.08
obstacle_min_spacing 8
```

- `obstacle_density` — Obstacles per 100x100 cell region of eligible terrain. Value is multiplied by 100 to get the count per region. Default: `0.08` (about 8 per region). On a 512x512 zone with ~50% eligible terrain, that produces ~104 obstacles. Crank to `0.20` for denser scatter (~260), drop to `0.03` for sparse (~40).
- `obstacle_min_spacing` — Minimum cell distance between obstacle block centers. Default: `8`. Prevents clumping.

**Budget formula:** `budget = (eligible_cells / 10000) * obstacle_density * 100`

**Example zone file** (procgen zone with obstacle settings):
```
name Sector 7G
size 512
seed 42
procgen true

# Procgen parameters
hotspot_count 8
wall_threshold 0.38
effect_threshold 0.55

# Obstacle scatter
obstacle_density 0.08
obstacle_min_spacing 8
```

## Scatter Algorithm

Obstacle scatter runs as a post-process step in `Procgen_generate()`, **after** noise terrain and landmark chunks are placed, **before** connectivity validation. **Deterministic**: The scatter RNG is seeded from the zone seed (same seed, same obstacle layout, every time). All random decisions — position, block selection, transforms, spawn probability rolls — go through this seeded RNG.

### Where obstacles go

Obstacles are placed in **moderate-to-sparse** terrain only:
- Skip cells inside landmark chunk footprints (already authored geometry)
- Skip cells in dense-influence zones (labyrinthine terrain doesn't need scatter fill)
- Target open areas where noise produced few/no walls — exactly where combat feels flat

### Style matching

The scatter algorithm picks obstacle style based on local influence strength at the placement position:

| Influence strength | Block pool |
|---|---|
| > 0.5 | Structured only |
| 0.2 — 0.5 | Blend (weighted random between both pools) |
| < 0.2 | Organic only |

This creates a natural visual gradient: geometric patterns near landmarks fade into organic formations in the wilds.

### Weighted block selection

Within a style pool, blocks are chosen by **weighted random selection**. Sum all weights in the pool, generate a random float in [0, total_weight), walk the list accumulating weights until the random value is exceeded. All through the seeded RNG.

### Algorithm

```
function scatter_obstacles(zone, terrain):
    rng = seed_rng(zone.seed)    // deterministic — same seed, same layout
    obstacles[] = load all .chunk files from resources/obstacles/
    structured[] = filter(obstacles, style == structured)
    organic[] = filter(obstacles, style == organic)

    // Precompute total weights per pool
    structured_total_weight = sum(b.weight for b in structured)
    organic_total_weight = sum(b.weight for b in organic)

    // Budget: how many obstacles to place
    eligible_area = count cells not in landmark footprints and not walls
    budget = (eligible_area / 10000) * obstacle_density * 100

    placed = []
    max_attempts = budget * 20   // cap attempts to avoid infinite loops

    for attempt in 0..max_attempts:
        if len(placed) >= budget:
            break

        // Random position within zone bounds
        x = rng_range(0, zone.size)
        y = rng_range(0, zone.size)

        // Skip if in a landmark chunk footprint
        if zone.cell_chunk_stamped[x][y]:
            continue

        // Skip if terrain is already a wall here
        if zone.cell_grid[x][y] >= 0:
            continue

        // Skip if too close to an already-placed obstacle
        if too_close(x, y, placed, obstacle_min_spacing):
            continue

        // Compute local influence strength
        strength = compute_influence_at(x, y)

        // Skip dense areas (influence strength > 0.7) — they have enough walls
        if strength > 0.7:
            continue

        // Pick style pool based on influence, then weighted-pick a block
        if strength > 0.5:
            block = pick_weighted(structured, structured_total_weight, rng)
        else if strength > 0.2:
            if rng_float() < strength:
                block = pick_weighted(structured, structured_total_weight, rng)
            else:
                block = pick_weighted(organic, organic_total_weight, rng)
        else:
            block = pick_weighted(organic, organic_total_weight, rng)

        // Check if block fits (no overlap with existing walls, landmarks, or map edge)
        if !block_fits(x, y, block, zone):
            continue

        // Stamp walls with random transform for variety
        transform = rng_range(0, TRANSFORM_COUNT)
        Chunk_stamp(block, zone, x, y, transform)

        // Roll spawn probabilities and create enemies
        for spawn in block.spawns:
            if rng_float() <= spawn.probability:
                create_enemy(spawn.type, x + spawn.x, y + spawn.y)

        placed.append({x, y})
```

### Neutral zone scatter

Areas with zero landmark influence (no man's land between landmark spheres) also get obstacles — organic blocks only. The algorithm above handles this naturally: zero influence strength falls into the `< 0.2` bucket, selecting from the organic pool.

## Integration Points

### procgen.c

Add obstacle scatter as a new step in `Procgen_generate()`:
1. *(existing)* Generate hotspots
2. *(existing)* Resolve landmarks, stamp chunks
3. *(existing)* Generate noise terrain with influence modulation
4. **NEW: Scatter obstacle blocks**
5. *(future)* Connectivity validation + corridor carving

### chunk.c / chunk.h

Extend `ChunkTemplate` with style, weight, and spawn probability:
```c
typedef enum {
    OBSTACLE_ORGANIC,      // Default
    OBSTACLE_STRUCTURED
} ObstacleStyle;

// Add to ChunkTemplate:
ObstacleStyle obstacle_style;  // default OBSTACLE_ORGANIC
float obstacle_weight;         // default 1.0

// Add to ChunkSpawn (or equivalent spawn struct):
float probability;             // 0.0–1.0, default 1.0
```

Parse `style`, `weight`, and spawn probability in `Chunk_load()`.

### Obstacle library loader

New function in procgen.c (or a small obstacle.c):
```c
void Obstacle_load_library(void);   // Load all .chunk files from resources/obstacles/
void Obstacle_cleanup_library(void);
int Obstacle_get_count(void);
const ChunkTemplate *Obstacle_get(int index);
```

Called once at startup or on first procgen zone load. The library persists across zone loads since obstacles are zone-agnostic (for now — biome-specific sets come later in Phase 6).

### zone.h

Add obstacle parameters to the Zone struct:
```c
float obstacle_density;      // default 0.08
int obstacle_min_spacing;    // default 8
```

Parse from zone file in `Zone_load()`.

## Influence Strength Query

The scatter algorithm needs to query influence strength at arbitrary grid positions. This already exists conceptually in the procgen influence system — it just needs to be exposed:

```c
float Procgen_get_influence_strength(int gx, int gy);
```

Returns 0.0 (no influence) to 1.0 (landmark center). Used by obstacle scatter for style matching, and useful for future systems (wall type refinement, enemy density).

## Godmode Visualization

In godmode, obstacle blocks are just regular wall cells and enemies — they show up on the existing map visualization and entity rendering. No special debug overlay needed beyond what's already there.

Optional future enhancement: highlight obstacle block footprints in a distinct color so the designer can see scatter distribution. Low priority.

## Edge Cases

- **Block doesn't fit**: If a block extends past the map edge or overlaps existing geometry, skip it and try the next random position. The attempt budget handles this gracefully.
- **No obstacles loaded**: If `resources/obstacles/` is empty or missing, scatter step is a no-op. No crash.
- **All structured / all organic**: If the library only contains one style, that style is used everywhere regardless of influence. Works fine — just less visual variation.
- **Empty style pool**: If the influence calls for structured but no structured blocks exist, fall back to organic (and vice versa). Never skip a placement just because one pool is empty.
- **Hand-placed cells**: `cell_hand_placed` cells are never overwritten. `Chunk_stamp` already respects `cell_chunk_stamped`. Obstacle stamps should also set `cell_chunk_stamped` so they don't get overwritten by later steps.
- **Enemy overflow**: Obstacle spawns are subject to the existing static array limits (`HUNTER_COUNT 128`, `SEEKER_COUNT 128`, etc.). If a spawn can't be created because the pool is full, it's silently skipped. Tune `obstacle_density` and spawn probabilities to keep totals reasonable.
- **Encounter-only blocks**: Blocks with spawns but no walls are valid. They place enemies in open terrain for random encounters without altering geometry.
- **Zero-weight blocks**: A block with `weight 0.0` is never selected. Valid but pointless — effectively disabled without removing the file.

## Not In Scope

- **Obstacle zone directive in chunks**: The main procgen spec describes an `obstacle_zone` directive inside landmark chunks that defines regions for style-specific obstacle fill. This is a landmark-interior feature and should ship with the landmark/chunk system, not with basic obstacle scatter.
- **Biome-specific obstacle sets**: Each biome having its own obstacle library. Deferred to Phase 6 content authoring.
- **Destructible obstacles**: Obstacles that can be broken by player weapons. Cool idea, separate feature.

## Done When

- 10-15 obstacle blocks authored (mix of structured and organic, varied weights, some with enemy spawns, some encounter-only)
- Obstacle library loads from `resources/obstacles/`
- Scatter algorithm places style-matched blocks in moderate-to-sparse terrain
- Weighted selection respects block weights within each style pool
- Structured blocks cluster near landmarks, organic blocks scatter in the wilds
- Minimum spacing prevents clumping
- Random transforms provide variety (rotations + mirrors)
- Scatter is deterministic — same seed produces same obstacle layout, same enemy spawns
- Spawn probabilities roll correctly — encounter-only blocks (no geometry) work
- Open combat areas have tactical cover instead of random noise dots
- Zone file `obstacle_density` and `obstacle_min_spacing` parameters work
- Obstacle blocks don't overlap landmarks, hand-placed cells, or map edges
- Godmode Obstacles export mode writes to `resources/obstacles/`
