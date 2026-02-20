# Phase 2 Implementation Spec: PRNG + Noise + Basic Terrain

## Goal

Seedable PRNG and simplex noise generate visible organic terrain across the whole map. A zone file with a `seed` directive produces three tile types — walls, effect cells, and empty space — from noise evaluation against two configurable thresholds. Same seed = same terrain, every time.

This is the foundation layer. No landmarks, no influence fields, no connectivity validation yet — just raw noise-driven terrain filling a 1024×1024 grid. Later phases modulate this with landmark influences, carve corridors, and populate enemies.

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

The main generation module. Phase 2 scope is just noise-to-terrain conversion.

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

            // Three-tile-type thresholding
            if noise_val < zone.wall_threshold:
                // WALL — pick wall cell type
                zone.cell_grid[x][y] = pick_wall_type(zone, &rng)
            else if noise_val < zone.effect_threshold:
                // EFFECT CELL — pick effect cell type
                type_idx = pick_effect_type(zone)
                if type_idx >= 0:
                    zone.cell_grid[x][y] = type_idx
                // else: no effect type defined, leave empty
            // else: EMPTY — leave as -1 (traversable space over cloudscape)
```

**Wall type selection** (`pick_wall_type`): For Phase 2, randomly pick among wall cell types defined in the zone. No influence-based refinement yet (that's Phase 3-4). If only one wall type exists, always use it. If both solid and circuit types exist, use a flat random split (e.g., 15% circuit, 85% solid — tunable).

**Effect type selection** (`pick_effect_type`): Pick the zone's effect type. Phase 2 supports one effect type per zone. If none defined, no effect cells are generated (two-tile terrain).

---

## Modified Files

### `map.h` — MapCell Extension

Add an `effectCell` flag to distinguish traversable effect cells from walls:

```c
typedef struct {
    bool empty;             // true = traversable (no collision)
    bool circuitPattern;    // visual: circuit trace pattern
    bool effectCell;        // true = traversable effect cell (rendered but no collision)
    ColorRGB primaryColor;  // fill color
    ColorRGB outlineColor;  // border color
} MapCell;
```

**Effect cell rendering behavior**:
- Allocated in cell pool (so map renderer draws them — same pipeline as walls)
- `empty = true` (no collision — player walks through)
- `effectCell = true` (distinguishes from walls for border rendering, stencil, bloom, future gameplay)
- Has its own colors and pattern (circuit or none)
- Borders drawn between effect cells and walls (different colors → CELLS_MATCH returns false)
- Borders drawn between effect cells and empty space (cell exists vs no cell)

**Stencil behavior (Phase 2)**: Effect cells write stencil value 1 (same as circuit walls). They don't get cloud reflection (not solid blocks). They do participate in weapon lighting (visual elements that can show light). This may need tuning — if effect cells look wrong with lighting, we can add a stencil value 3 for effect cells and adjust the lighting test.

**Bloom behavior (Phase 2)**: Effect cells render bloom sources if their colors are bright enough. Data trace effect cells should have subtle colors → minimal bloom. Themed hazard cells (future) will be brighter → more bloom. No code changes needed — bloom source rendering already uses cell colors.

### `zone.h` — Zone Struct Extensions

Add noise parameters and effect type tracking to the Zone struct:

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
double noise_effect_threshold;          // Between wall and this = effect cell (default 0.15)

// Effect cell type tracking
// Effect types are stored in cell_types[] alongside wall types.
// These indices track which cell_types are effect types vs wall types.
int effect_type_indices[ZONE_MAX_CELL_TYPES];   // Indices into cell_types[] for effect types
int effect_type_count;
int wall_type_indices[ZONE_MAX_CELL_TYPES];     // Indices into cell_types[] for wall types
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
noise_effect_threshold <float>
effecttype <id> <pr> <pg> <pb> <pa> <or> <og> <ob> <oa> <pattern>
```

**`effecttype` parsing**: Same format as `celltype` but creates a cell type with `empty=true` and `effectCell=true`. Stored in the same `cell_types[]` array but also tracked in `effect_type_indices[]`.

**`celltype` tracking**: Existing celltypes are now also tracked in `wall_type_indices[]` so the generator can pick among wall types specifically.

**Default values**: If a zone has a `seed` but doesn't specify noise params, use defaults:
- octaves: 5
- frequency: 0.01
- lacunarity: 2.0
- persistence: 0.5
- wall_threshold: -0.1
- effect_threshold: 0.15

**Integration point**: After all lines are parsed, if `zone.has_seed` is true, call `Procgen_generate(&zone)` before `apply_zone_to_world()`.

### `zone.c` — ZoneCellType Extension

The `ZoneCellType` struct needs a flag to indicate effect cell status:

```c
// Add to ZoneCellType:
bool is_effect;     // true = traversable effect cell, false = wall
```

When `apply_zone_to_world()` creates MapCells from ZoneCellTypes, it sets:
- Wall types: `empty = false, effectCell = false`
- Effect types: `empty = true, effectCell = true`

### `map.c` — Border Rendering Adjustment

The CELLS_MATCH macro and border rendering need to handle effect cells:

**Rule**: Effect cells and walls are always visually distinct — borders drawn between them even if colors happen to match. Two effect cells with matching visuals share a border (same as two walls). Effect cells adjacent to empty space get a border.

**Implementation**: CELLS_MATCH already compares circuitPattern + colors. Since effect cells have different colors than walls, this should work naturally. If we need explicit separation, add `effectCell` to the CELLS_MATCH comparison.

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

# Effect cell type — data traces (traversable, atmospheric)
effecttype data_trace 10 60 80 140 30 120 160 100 circuit

# Procgen seed + noise parameters
seed 12345
noise_octaves 5
noise_frequency 0.01
noise_lacunarity 2.0
noise_persistence 0.5
noise_wall_threshold -0.1
noise_effect_threshold 0.15

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
       - Cell types (celltype + effecttype)
       - Noise parameters (seed, octaves, frequency, etc.)
       - Hand-placed cells, spawns, portals, savepoints
    2. If zone.has_seed:
       a. Procgen_generate(&zone)
          - Init PRNG with seed
          - For each cell in 1024×1024 grid:
            - Skip if hand-placed (cell_grid != -1)
            - Evaluate Noise_fbm at grid position
            - noise < wall_threshold → wall cell type
            - wall_threshold ≤ noise < effect_threshold → effect cell type
            - noise ≥ effect_threshold → leave empty
    3. apply_zone_to_world()
       - Map_clear()
       - Iterate cell_grid, call Map_set_cell() for each occupied cell
       - Place portals, savepoints, spawn enemies
    4. Gameplay begins
```

**Key invariant**: Hand-placed content always wins. The generator never overwrites cells, spawns, portals, or savepoints that were defined in the zone file. This ensures the savepoint at center (512, 512) and any hand-placed cells are preserved.

---

## Performance Considerations

**1M cells × 5 octaves = ~5M simplex evaluations**. Each simplex2d is ~20-30 float ops. Total: ~100-150M float ops. At modern single-core throughput, this is well under 200ms. No threading needed for Phase 2.

**Memory**: No additional allocations beyond the existing cell pool. The noise function is stateless. The PRNG is 16 bytes. Zone struct extensions are a few hundred bytes.

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
| `noise_effect_threshold` | 0.15 | wall + 0.05 (thin band) | 0.5 (wide band) | Effect cell coverage |

**Threshold gap** (`effect_threshold - wall_threshold`) controls how many effect cells appear:
- Narrow gap (0.05): Very few effect cells, almost binary terrain
- Default gap (0.25): ~25% of noise range → moderate effect cell coverage
- Wide gap (0.4): Lots of effect cells, less empty space

**Starting values**: The defaults should produce ~35% walls, ~15% effect cells, ~50% empty space on a flat map with no influence. This gives a good terrain-to-open ratio for navigation while keeping the cloudscape visible in open areas.

---

## Debug Overlay (Godmode)

**Noise heatmap toggle**: New godmode hotkey (suggest: H key) shows a color overlay on each cell based on raw noise value:
- Red = low noise (would be wall)
- Yellow = mid noise (would be effect cell)
- Green = high noise (would be empty)

This is essential for tuning noise parameters. The overlay renders as a semi-transparent color on top of existing cell rendering.

**Implementation**: When enabled, iterate visible cells and draw a colored quad at each position using `Render_triangle` with alpha. Colors interpolated from noise value. Only evaluates noise for on-screen cells (camera viewport), not the full 1024×1024 grid.

**Seed cycling**: Godmode hotkey (suggest: Shift+H) re-rolls the seed (increment by 1) and regenerates terrain without reloading the zone file. This enables fast visual iteration: tap Shift+H repeatedly to see different terrain from the same noise parameters.

---

## Implementation Order

1. **`prng.c/h`** — Implement xoshiro128** with SplitMix32 seeding. Test determinism.
2. **`noise.c/h`** — Implement 2D simplex noise + fBm wrapper. Test with known values.
3. **`map.h`** — Add `effectCell` flag to MapCell.
4. **`zone.h/zone.c`** — Add noise params to Zone struct. Add `effecttype` parsing. Add `seed` + noise parameter parsing. Track wall/effect type indices.
5. **`zone.c`** — Update `apply_zone_to_world()` to set `effectCell` flag when creating MapCells from effect ZoneCellTypes.
6. **`procgen.c/h`** — Implement `Procgen_generate()` — noise evaluation + three-tile thresholding.
7. **`zone.c`** — Hook `Procgen_generate()` into `Zone_load()` pipeline.
8. **Create test zone file** — Minimal procgen zone with seed + noise params + hand-placed savepoint/portal.
9. **Test** — Load zone, verify terrain appears, walk through it, change seed, verify different terrain.
10. **Debug overlay** — Noise heatmap + seed cycling in godmode (optional, can defer).

---

## Risks & Open Questions

1. **Simplex noise patent**: The original Simplex Noise by Ken Perlin had a patent (now expired as of Jan 2022). We're clear. Alternatively, use OpenSimplex2 which was always patent-free. Either works — lean toward classic simplex for simplicity.

2. **Border rendering with effect cells**: Effect cells adjacent to empty space will get borders. If this looks weird (borders around ground-level features), we may want effect cells to NOT draw borders against empty space. This is a rendering tweak, not a structural issue.

3. **Stencil behavior**: Effect cells in the stencil mask is untested territory. If weapon lighting on effect cells looks wrong, we'll need a separate stencil value. Low risk — easy to adjust.

4. **Noise frequency for 1024 grid**: The default 0.01 means feature scale is ~100 cells. This should produce terrain blobs roughly 100×100 cells — large enough for navigable passages, small enough for interesting density variation. Needs visual validation.

5. **Effect cell rendering weight**: If effect cells have bright colors, they could visually compete with the background cloudscape. Data trace effect cells should use low-alpha or dim colors. The zone's `effecttype` colors control this — authoring concern, not code concern.

---

## Done When

- [ ] PRNG produces deterministic sequences (same seed = same output)
- [ ] Simplex noise generates smooth 2D fields (no grid artifacts, proper range)
- [ ] fBm wrapper layers octaves correctly (more octaves = more detail)
- [ ] Zone file parser reads `seed`, noise params, and `effecttype` directives
- [ ] Procgen generates terrain from noise + dual thresholds
- [ ] Three tile types visible: walls (solid), effect cells (traversable + rendered), empty space
- [ ] Hand-placed cells preserved (generator skips them)
- [ ] Effect cells don't block player movement (traversable)
- [ ] Effect cells render with their own visual appearance
- [ ] Different seeds produce different terrain layouts
- [ ] Same seed produces identical terrain every time
- [ ] Varying noise params visibly changes terrain character (density, scale, effect coverage)
- [ ] Terrain feels organic, not grid-aligned (no visible rectangular patterns)
- [ ] Reasonable navigation — can walk through generated terrain at default thresholds
- [ ] Game compiles and runs with no regressions on non-procgen zones
