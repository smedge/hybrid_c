# Phase 2 Implementation Spec: PRNG + Noise + Basic Terrain

## Goal

Seedable PRNG and simplex noise generate visible organic terrain across the whole map. A zone file with a `seed` directive produces two tile types — walls and empty space — from noise evaluation against a single configurable threshold. Same seed = same terrain, every time.

This is the foundation layer. No landmarks, no influence fields, no connectivity validation, no effect cells yet — just raw noise-driven terrain filling a 1024×1024 grid. Later phases modulate this with landmark influences, carve corridors, and populate enemies. Effect cells (traversable rendered cells for data traces and biome hazards) will be added when themed zones are implemented.

---

## New Files

### `prng.c/h` — Seedable PRNG (xoshiro128**)

Lightweight, fast, deterministic. Same seed = same sequence across platforms.

```c
typedef struct {
    uint32_t state[4];
} Prng;

void    Prng_seed(Prng *rng, uint32_t seed);
uint32_t Prng_next(Prng *rng);                        // Raw 32-bit output
int     Prng_range(Prng *rng, int min, int max);       // Inclusive range [min, max]
float   Prng_float(Prng *rng);                         // [0.0, 1.0)
double  Prng_double(Prng *rng);                        // [0.0, 1.0)
```

**xoshiro128** algorithm**: Well-studied, non-cryptographic, 128-bit state, passes BigCrush. Seeding uses SplitMix32 to expand a single uint32 seed into the 4-word state (avoids zero-state problem).

**Why a struct, not global state**: Multiple independent generators needed later (one for terrain, one for enemies, one for hotspots). Struct-based keeps them isolated. For Phase 2, a single instance is fine.

---

### `noise.c/h` — 2D Simplex Noise

Standard 2D simplex noise + multi-octave fBm wrapper.

```c
// Single-evaluation 2D simplex noise. Returns [-1.0, 1.0].
// Deterministic for same inputs — no internal state.
double Noise_simplex2d(double x, double y);

// Multi-octave fractal Brownian motion. Returns [-1.0, 1.0] (normalized).
// seed offsets each octave for per-seed uniqueness.
double Noise_fbm(double x, double y, int octaves, double frequency,
                 double lacunarity, double persistence, uint32_t seed);
```

**Simplex noise implementation**: Uses the standard Simplex Noise algorithm (skew to simplex grid, determine simplex, gradient contribution from 3 corners). Gradient table is static (12 gradient vectors for 2D). No internal state, no allocations.

**fBm wrapper**: Layers multiple octaves at increasing frequency and decreasing amplitude. Each octave is offset by seed-derived values (using Prng internally) so different seeds produce different terrain even at the same coordinates.

**Offset generation for fBm**:
```
For each octave i:
    offset_x[i] = Prng_double(&seed_rng) * 1000.0 - 500.0
    offset_y[i] = Prng_double(&seed_rng) * 1000.0 - 500.0
```
The ±500 range ensures octaves sample far-apart regions of the noise field.

---

### `procgen.c/h` — Terrain Generation

The main generation module. Phase 2 scope is noise-to-terrain conversion with a single threshold.

```c
// Generate terrain for the given zone. Fills zone->cell_grid using noise.
// Skips cells already placed by hand (cell_grid[x][y] != -1).
// Call after zone file parsing, before apply_zone_to_world().
void Procgen_generate(Zone *zone);
```

**Phase 2 algorithm** (simplified — no landmarks/influence yet):

```
function Procgen_generate(zone):
    if zone has no seed: return  // Not a procgen zone

    rng = Prng_seed(zone.seed)

    for y in 0..zone.size:
        for x in 0..zone.size:
            // Skip hand-placed cells
            if zone.cell_grid[x][y] != -1:
                continue

            // Evaluate noise at this grid position
            noise_val = Noise_fbm(x, y,
                                   zone.noise_octaves,
                                   zone.noise_frequency,
                                   zone.noise_lacunarity,
                                   zone.noise_persistence,
                                   zone.seed)

            // Two-tile-type thresholding
            if noise_val < zone.wall_threshold:
                // WALL — pick wall cell type
                zone.cell_grid[x][y] = pick_wall_type(zone, &rng)
            // else: EMPTY — leave as -1 (traversable space over cloudscape)
```

**Wall type selection** (`pick_wall_type`): For Phase 2, randomly pick among wall cell types defined in the zone. No influence-based refinement yet (that's Phase 3-4). If only one wall type exists, always use it. If both solid and circuit types exist, use a flat random split (e.g., 15% circuit, 85% solid — tunable).

---

## Modified Files

### `zone.h` — Zone Struct Extensions

Add noise parameters to the Zone struct:

```c
// Add to Zone struct:

// Procgen noise parameters
bool has_seed;                          // true if zone uses procedural generation
uint32_t seed;                          // PRNG seed for deterministic generation

int noise_octaves;                      // Number of noise layers (default 5)
double noise_frequency;                 // Base frequency (default 0.01)
double noise_lacunarity;                // Frequency multiplier per octave (default 2.0)
double noise_persistence;               // Amplitude multiplier per octave (default 0.5)
double noise_wall_threshold;            // Below this = wall (default -0.1)

// Wall type tracking — indices into cell_types[] that are wall types
// (all celltypes are wall types in Phase 2, but tracked explicitly
// for forward-compatibility when effect types are added later)
int wall_type_indices[ZONE_MAX_CELL_TYPES];
int wall_type_count;
```

### `zone.c` — Parser Extensions

Add parsing for new zone file directives:

```
seed <uint32>
noise_octaves <int>
noise_frequency <float>
noise_lacunarity <float>
noise_persistence <float>
noise_wall_threshold <float>
```

**`celltype` tracking**: When celltypes are parsed, also record their index in `wall_type_indices[]` so the generator can pick among wall types.

**Default values**: If a zone has a `seed` but doesn't specify noise params, use defaults:
- octaves: 5
- frequency: 0.01
- lacunarity: 2.0
- persistence: 0.5
- wall_threshold: -0.1

**Integration point**: After all lines are parsed, if `zone.has_seed` is true, call `Procgen_generate(&zone)` before `apply_zone_to_world()`.

### `zone.c` — Zone Save Extension

When saving a procgen zone, write the seed and noise parameters back to the zone file so they round-trip. The generated cells themselves are NOT saved — they're regenerated from the seed on load. Only hand-placed cells are persisted.

**What gets saved**:
```
seed <value>
noise_octaves <value>
noise_frequency <value>
noise_lacunarity <value>
noise_persistence <value>
noise_wall_threshold <value>
```

**What does NOT get saved**: Generated cell data. The cell_grid entries created by `Procgen_generate()` are transient — they exist in memory after generation but are not written as `cell` lines in the zone file. Only cells that were hand-placed (via godmode or authored in the zone file) are saved as `cell` lines.

This keeps zone files small and seed-deterministic. A procgen zone file is just the skeleton + seed + parameters.

---

## Zone File Format Example

A minimal procgen test zone:

```
name Procgen Test
size 1024

bgcolor 0 89 26 140
bgcolor 1 51 26 128
bgcolor 2 115 20 102
bgcolor 3 38 13 89

# Wall types
celltype solid 20 0 20 255 128 0 128 255 none
celltype circuit 10 20 20 255 64 128 128 255 circuit

# Procgen seed + noise parameters
seed 12345
noise_octaves 5
noise_frequency 0.01
noise_lacunarity 2.0
noise_persistence 0.5
noise_wall_threshold -0.1

# Hand-placed content (skipped by generator)
savepoint 512 512 1
portal 510 512 1 zone_001.zone 1

# Hand-placed enemies (fixed spawns)
spawn mine 0.0 0.0
```

---

## Generation Pipeline (Phase 2)

```
Zone_load("procgen_test.zone"):
    1. Parse zone file (existing parser + new directives)
       - Cell types (celltype)
       - Noise parameters (seed, octaves, frequency, etc.)
       - Hand-placed cells, spawns, portals, savepoints
    2. If zone.has_seed:
       a. Procgen_generate(&zone)
          - Init PRNG with seed
          - For each cell in 1024×1024 grid:
            - Skip if hand-placed (cell_grid != -1)
            - Evaluate Noise_fbm at grid position
            - noise < wall_threshold → pick wall cell type
            - noise ≥ wall_threshold → leave empty
    3. apply_zone_to_world()
       - Map_clear()
       - Iterate cell_grid, call Map_set_cell() for each occupied cell
       - Place portals, savepoints, spawn enemies
    4. Gameplay begins
```

**Key invariant**: Hand-placed content always wins. The generator never overwrites cells, spawns, portals, or savepoints that were defined in the zone file. This ensures the savepoint at center (512, 512) and any hand-placed cells are preserved.

---

## Distinguishing Hand-Placed vs Generated Cells

The generator needs to know which cells were hand-placed (from the zone file) so it skips them. After generation, the save system needs to know which cells are hand-placed so it only saves those.

**Approach**: Add a parallel boolean grid to track hand-placed status:

```c
// Add to Zone struct:
bool cell_hand_placed[MAP_SIZE][MAP_SIZE];  // true = authored in zone file, false = generated
```

- Zone parser sets `cell_hand_placed[x][y] = true` when parsing `cell` lines
- `Procgen_generate()` checks `cell_hand_placed` (not just `cell_grid != -1`) to skip hand-placed cells
- `Zone_save()` only writes `cell` lines for cells where `cell_hand_placed` is true
- Godmode cell placement sets `cell_hand_placed = true` (hand-placed by definition)
- Godmode cell removal sets `cell_hand_placed = true` and `cell_grid = -1` (explicit removal persists — prevents the generator from filling that spot on next load)

**Why not just save all cells?** A 1024×1024 zone could have 300K+ generated wall cells. Writing them all as `cell` lines would make zone files enormous and defeat the purpose of seed-based generation. The zone file should be a compact skeleton.

**Godmode removal persistence**: If you clear a generated cell in godmode, that removal should persist across save/load. This requires storing explicit "no cell here" markers for hand-cleared positions. Options:
- Save `clearcell <x> <y>` lines in the zone file for explicitly removed cells
- Or: save `cell <x> <y> empty` to mark intentional empties

Lean toward `clearcell <x> <y>` — it's clearer in intent.

---

## Performance Considerations

**1M cells × 5 octaves = ~5M simplex evaluations**. Each simplex2d is ~20-30 float ops. Total: ~100-150M float ops. At modern single-core throughput, this is well under 200ms. No threading needed for Phase 2.

**Memory**: No additional allocations beyond the existing cell pool. The noise function is stateless. The PRNG is 16 bytes. Zone struct extensions add the `cell_hand_placed` grid (1MB bool array) and a few hundred bytes of parameters.

**Cell pool pressure**: Worst case, noise generates walls for ~50% of cells = ~500K cells. Current `CELL_POOL_SIZE = MAP_SIZE * MAP_SIZE` (1M). Plenty of headroom even at high wall density.

---

## Tuning Parameters & Expected Ranges

| Parameter | Default | Low Extreme | High Extreme | Effect |
|-----------|---------|-------------|--------------|--------|
| `noise_octaves` | 5 | 1 (smooth blobs) | 8 (crunchy detail) | More octaves = more fine detail |
| `noise_frequency` | 0.01 | 0.003 (huge features) | 0.05 (tiny features) | Scale of terrain features |
| `noise_lacunarity` | 2.0 | 1.5 | 3.0 | How quickly detail scale changes |
| `noise_persistence` | 0.5 | 0.2 (smooth) | 0.8 (noisy) | How much detail contributes |
| `noise_wall_threshold` | -0.1 | -0.5 (very sparse) | 0.3 (very dense) | Wall density |

**Wall density**: Simplex noise is roughly centered around 0. A threshold of -0.1 means ~45% of cells become walls. A threshold of 0.0 means ~50%. A threshold of -0.3 means ~35%.

**Starting values**: The default threshold of -0.1 should produce roughly 40-45% walls and 55-60% empty space. This gives dense but navigable terrain with the cloudscape visible through open areas. Tune from there.

---

## Debug Overlay (Godmode)

**Noise heatmap toggle**: New godmode hotkey (suggest: H key) shows a color overlay on each cell based on raw noise value:
- Red = low noise (wall)
- Green = high noise (empty)
- Threshold line visible as the color transition

This is essential for tuning noise parameters. The overlay renders as a semi-transparent color on top of existing cell rendering.

**Implementation**: When enabled, iterate visible cells and draw a colored quad at each position using `Render_triangle` with alpha. Colors interpolated from noise value. Only evaluates noise for on-screen cells (camera viewport), not the full 1024×1024 grid.

**Seed cycling**: Godmode hotkey (suggest: Shift+H) re-rolls the seed (increment by 1) and regenerates terrain without reloading the zone file. This enables fast visual iteration: tap Shift+H repeatedly to see different terrain from the same noise parameters.

**Implementation for seed cycling**:
1. Increment `zone.seed`
2. Clear all non-hand-placed cells from cell_grid (reset to -1 where `cell_hand_placed` is false)
3. Call `Procgen_generate(&zone)` with new seed
4. Call `apply_zone_to_world()` to refresh the map

---

## Implementation Order

1. **`prng.c/h`** — Implement xoshiro128** with SplitMix32 seeding. Test determinism.
2. **`noise.c/h`** — Implement 2D simplex noise + fBm wrapper. Test with known values.
3. **`zone.h/zone.c`** — Add noise params + `cell_hand_placed` grid + `wall_type_indices` to Zone struct. Add `seed` + noise parameter parsing. Track wall type indices when parsing celltypes.
4. **`procgen.c/h`** — Implement `Procgen_generate()` — noise evaluation + single-threshold wall placement.
5. **`zone.c`** — Hook `Procgen_generate()` into `Zone_load()` pipeline (after parsing, before apply_zone_to_world).
6. **`zone.c`** — Update `Zone_save()` to write seed/noise params and only save hand-placed cells.
7. **Create test zone file** — Minimal procgen zone with seed + noise params + hand-placed savepoint/portal.
8. **Test** — Load zone, verify terrain appears, walk through it, change seed, verify different terrain.
9. **Debug overlay** — Noise heatmap + seed cycling in godmode.

---

## Risks & Open Questions

1. **Simplex noise patent**: The original Simplex Noise by Ken Perlin had a patent (now expired as of Jan 2022). We're clear. Alternatively, use OpenSimplex2 which was always patent-free. Either works — lean toward classic simplex for simplicity.

2. **Noise frequency for 1024 grid**: The default 0.01 means feature scale is ~100 cells. This should produce terrain blobs roughly 100×100 cells — large enough for navigable passages, small enough for interesting density variation. Needs visual validation.

3. **Wall threshold default**: At -0.1, roughly 40-45% of cells become walls. This might be too dense or too sparse — needs visual tuning once we can see the terrain. The debug overlay + seed cycling will make this easy to iterate on.

4. **Hand-placed cell tracking memory**: The `cell_hand_placed` grid is 1MB (1024×1024 bools). This is fine for a single zone in memory. If memory becomes a concern, could compress to a bitfield (128KB), but not worth the complexity now.

5. **Godmode editing in procgen zones**: When a player edits cells in godmode on a procgen zone, those edits are hand-placed and persist. But the surrounding generated terrain regenerates from seed on reload. This is correct behavior — edits are overlays on top of generated content. Worth noting in case it surprises anyone.

6. **Non-procgen zone compatibility**: Zones without a `seed` directive are unaffected. `Procgen_generate()` returns immediately if `has_seed` is false. Zero regressions on existing hand-authored zones (The Origin, Data Nexus, etc.).

---

## Future: Effect Cells (Phase 3 — Chunk-Authored, Not Noise-Generated)

Effect cells (traversable rendered cells) are deferred to Phase 3 when the chunk system is implemented. They will be **hand-authored inside chunks**, not randomly scattered by noise. This means:

- Effect cells appear only in designed locations — inside landmark chunks, encounter gates, boss arenas
- A fire zone boss arena has fire cells placed with intent at specific positions in the chunk
- An ice chokepoint has ice patches authored exactly where they create tactical decisions
- No noise threshold needed — chunks stamp effect cells directly, same as they stamp walls and empties

**Engine work required (Phase 3)**:
- New `effectCell` flag on MapCell (traversable but rendered)
- Rendering support: map renderer draws cells that are traversable + have visual data
- Border behavior between effect cells, walls, and empty space
- Stencil integration (effect cells probably don't get cloud reflection — they're floor-level)
- New cell type in chunk format: `effect <x> <y> <celltype_id>` alongside existing `wall` and `empty`

**Gameplay hooks (later, themed zones)**:
- Damage-over-time (fire), friction changes (ice), feedback drain (poison), etc.
- Player standing on effect cell triggers per-biome behavior
- Effect parameters defined per cell type in the zone file
- Enemies NOT affected by effect cells (they're security programs — they belong to the system)

This keeps effect cells as a **design tool** (placed where they matter for gameplay) rather than procedural decoration. The chunk system provides the authoring mechanism for free.

---

## Done When

- [ ] PRNG produces deterministic sequences (same seed = same output)
- [ ] Simplex noise generates smooth 2D fields (no grid artifacts, proper range)
- [ ] fBm wrapper layers octaves correctly (more octaves = more detail)
- [ ] Zone file parser reads `seed` and noise parameter directives
- [ ] Zone file save writes seed and noise parameters, only hand-placed cells
- [ ] Procgen generates wall terrain from noise + single threshold
- [ ] Two tile types visible: walls (solid) and empty space (cloudscape)
- [ ] Hand-placed cells preserved (generator skips them)
- [ ] Different seeds produce different terrain layouts
- [ ] Same seed produces identical terrain every time
- [ ] Varying noise params visibly changes terrain character (density, scale)
- [ ] Terrain feels organic, not grid-aligned (no visible rectangular patterns)
- [ ] Reasonable navigation — can walk through generated terrain at default threshold
- [ ] Non-procgen zones load and play with no regressions
- [ ] Debug overlay: noise heatmap visible in godmode
- [ ] Debug: seed cycling regenerates terrain in-place
