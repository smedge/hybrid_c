# Spec: Obstacle Blocks

## Overview

Noise-generated terrain in moderate-to-sparse areas produces random-looking scattered wall cells — dots with no tactical meaning. Obstacle blocks replace that randomness with **authored micro-structures**: cover positions, pillar clusters, L-shaped walls, chokepoints, and corridors. They're pre-authored wall patterns (3x3 up to 16x16) scattered by the procgen system into open terrain to give combat encounters spatial structure.

**What they are NOT**: Obstacle blocks are not landmarks (no influence field, no enemy spawns, no progression gating). They're purely physical terrain — wall geometry that shapes how the player navigates and fights in the spaces between landmarks.

## The Problem

Right now, the noise terrain generator creates two extremes:
1. **Dense areas** (near dense-influence landmarks): Labyrinthine walls with natural corridors — these feel great already
2. **Sparse/moderate areas**: Scattered individual wall cells that feel arbitrary — no tactical cover, no interesting geometry, just noise dots

Obstacle blocks fill the gap. They give moderate and sparse terrain **authored tactical character** without hand-placing every wall.

## Obstacle Block Format

Obstacle blocks reuse the existing `.chunk` file format from `chunk.c/h`. They're just small chunks — the same `wall`, `empty`, `size` directives, loaded by the same `Chunk_load()` function.

Two additions to the chunk format:
1. A `style` directive in the chunk header.
2. An optional **probability** field on `spawn` lines (0.0–1.0), rolled independently at placement time.

```
# A 4x4 L-shaped wall with a probable hunter — structured style
chunk ambush_corner
size 5 5
style structured

wall 0 0 0
wall 1 0 0
wall 0 1 0
wall 0 2 0
wall 0 3 0
spawn hunter 4 2 0.6
```

```
# A 3x3 organic cluster — organic style (placed in the wilds)
chunk rock_scatter
size 3 3
style organic

wall 0 0 0
wall 2 0 0
wall 1 1 0
wall 0 2 0
```

```
# An encounter-only block — no geometry, just a chance of enemies
chunk patrol_encounter
size 8 8
style organic

spawn hunter 2 2 0.5
spawn hunter 6 6 0.3
spawn seeker 4 4 0.4
```

Spawn probabilities are rolled independently per spawn line when the obstacle is placed. A block with three 0.5-probability spawns might produce zero, one, two, or three enemies. If the probability field is omitted, the spawn is guaranteed (probability 1.0).

**Style values:**
- `structured` — Geometric, deliberate patterns. Symmetry, clean edges, recognizable shapes. Placed near landmarks where terrain feels architectural.
- `organic` — Random-looking, asymmetric clusters. Natural formations. Placed in the wilds, far from landmark influence.

If `style` is omitted, the block defaults to `organic`.

### Storage

Obstacle blocks live in `resources/obstacles/`. Separate directory from `resources/chunks/` (which holds landmark rooms and center anchors) to keep them organized.

```
resources/obstacles/
    pillar_2x2.chunk
    pillar_L.chunk
    wall_segment_h.chunk
    wall_segment_v.chunk
    corner_3x3.chunk
    rock_scatter.chunk
    rock_cluster.chunk
    ...
```

## Authoring

Two workflows, same output:

1. **Godmode chunk export**: Enter godmode, paint walls and place enemies, select the region with Q/E to cycle to the appropriate mode. The godmode tool cycle gains a new **Obstacles** mode alongside the existing Chunks mode. Tab to export — Chunks mode writes to `resources/chunks/`, Obstacles mode writes to `resources/obstacles/`. Edit the exported file afterward to add `style structured` or `style organic` and probability values on spawn lines.

2. **Text editor**: Write directly — the format is trivial for small patterns.

The obstacle library starts small (10-15 blocks) and grows during Phase 6 content authoring. Each biome may eventually have its own obstacle set, but for now one shared library is fine.

## Zone File Integration

Two new parameters in the zone file control obstacle scatter:

```
obstacle_density 0.08
obstacle_min_spacing 8
```

- `obstacle_density` — Target obstacle count per 100x100 cell area (approximate). Default: `0.08` (about 8 obstacles per 100x100 region of eligible terrain).
- `obstacle_min_spacing` — Minimum cell distance between obstacle block centers. Default: `8`. Prevents clumping.

## Scatter Algorithm

Obstacle scatter runs as a post-process step in `Procgen_generate()`, **after** noise terrain and landmark chunks are placed, **before** connectivity validation.

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

### Algorithm

```
function scatter_obstacles(zone, terrain, rng):
    obstacles[] = load all .chunk files from resources/obstacles/
    structured[] = filter(obstacles, style == structured)
    organic[] = filter(obstacles, style == organic)

    placed = []
    max_attempts = zone.size * zone.size * obstacle_density * 20

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

        // Pick block based on style matching
        if strength > 0.5:
            block = pick_random(structured, rng)
        else if strength > 0.2:
            if rng_float() < strength:
                block = pick_random(structured, rng)
            else:
                block = pick_random(organic, rng)
        else:
            block = pick_random(organic, rng)

        // Check if block fits (no overlap with existing walls, landmarks, or map edge)
        if !block_fits(x, y, block, zone):
            continue

        // Stamp with random transform for variety
        transform = rng_range(0, TRANSFORM_COUNT)
        Chunk_stamp(block, zone, x, y, transform)
        placed.append({x, y})

    // Budget comes from eligible area * density
    // but we cap attempts to avoid infinite loops in dense maps
```

### Neutral zone scatter

Areas with zero landmark influence (no man's land between landmark spheres) also get obstacles — organic blocks only. The algorithm above handles this naturally: zero influence strength falls into the `< 0.2` bucket, selecting organic blocks.

## Integration Points

### procgen.c

Add obstacle scatter as a new step in `Procgen_generate()`:
1. *(existing)* Generate hotspots
2. *(existing)* Resolve landmarks, stamp chunks
3. *(existing)* Generate noise terrain with influence modulation
4. **NEW: Scatter obstacle blocks**
5. *(future)* Connectivity validation + corridor carving

### chunk.c / chunk.h

Extend `ChunkTemplate` with a `style` field:
```c
typedef enum {
    OBSTACLE_ORGANIC,      // Default
    OBSTACLE_STRUCTURED
} ObstacleStyle;

// Add to ChunkTemplate:
ObstacleStyle obstacle_style;
```

Parse the `style` directive in `Chunk_load()`.

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

In godmode, obstacle blocks are just regular wall cells — they show up on the existing map visualization. No special debug overlay needed beyond what's already there.

Optional future enhancement: highlight obstacle block footprints in a distinct color so the designer can see scatter distribution. Low priority.

## Edge Cases

- **Block doesn't fit**: If a block extends past the map edge or overlaps existing geometry, skip it and try the next random position. The attempt budget handles this gracefully.
- **No obstacles loaded**: If `resources/obstacles/` is empty or missing, scatter step is a no-op. No crash.
- **All structured / all organic**: If the library only contains one style, that style is used everywhere regardless of influence. Works fine — just less visual variation.
- **Hand-placed cells**: `cell_hand_placed` cells are never overwritten. `Chunk_stamp` already respects `cell_chunk_stamped`. Obstacle stamps should also set `cell_chunk_stamped` so they don't get overwritten by later steps.
- **Enemy overflow**: Obstacle spawns are subject to the existing static array limits (`HUNTER_COUNT 128`, `SEEKER_COUNT 128`, etc.). If a spawn can't be created because the pool is full, it's silently skipped. Tune `obstacle_density` and spawn probabilities to keep totals reasonable.

## Not In Scope

- **Obstacle zone directive in chunks**: The main procgen spec describes an `obstacle_zone` directive inside landmark chunks that defines regions for style-specific obstacle fill. This is a landmark-interior feature and should ship with the landmark/chunk system, not with basic obstacle scatter.
- **Biome-specific obstacle sets**: Each biome having its own obstacle library. Deferred to Phase 6 content authoring.
- **Destructible obstacles**: Obstacles that can be broken by player weapons. Cool idea, separate feature.

## Done When

- 10-15 obstacle blocks authored (mix of structured and organic, some with enemy spawns)
- Obstacle library loads from `resources/obstacles/`
- Scatter algorithm places style-matched blocks in moderate-to-sparse terrain
- Structured blocks cluster near landmarks, organic blocks scatter in the wilds
- Minimum spacing prevents clumping
- Random transforms provide variety (rotations + mirrors)
- Open combat areas have tactical cover instead of random noise dots
- Zone file `obstacle_density` and `obstacle_min_spacing` parameters work
- Obstacle blocks don't overlap landmarks, hand-placed cells, or map edges
- Spawn probabilities roll correctly — encounter-only blocks (no geometry) work
- Godmode Obstacles export mode writes to `resources/obstacles/`
