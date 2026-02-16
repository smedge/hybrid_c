# Spec: Procedural Level Generation — A Hybrid Approach

## Summary & Implementation Reality

### What this system does

You author a minimal zone skeleton — a center anchor (savepoint + entry portal), landmark definitions with terrain influence tags, and global tuning parameters. The generator procedurally builds the entire 1024×1024 zone around that skeleton: terrain, corridors, open areas, enemy placements, and landmark positioning. Every generation run is **seed-deterministic**: same zone + same seed = identical world. This is a hard requirement — foundational for multiplayer, shared seeds, reproducible bugs, and daily/weekly challenge runs.

**The key innovation**: the zone has no fixed regions, no fixed quadrants, no predetermined layout structure. Instead:

1. **Hotspot positions are generated per seed** — candidate landmark locations are scattered across the map procedurally, constrained by minimum separation and edge margins.
2. **Each landmark carries a designer-assigned terrain influence** — any landmark can be configured as dense (labyrinthine), moderate (mixed), or sparse (open battlefield). A boss designed for a claustrophobic fight gets dense. A boss designed for an open arena brawl gets sparse. A safe zone could be sparse for breathing room or moderate for a defensible hideout. The terrain *character* is entirely up to the designer per landmark, and it's an emergent property of where the landmarks land.
3. **Noise-driven base terrain** covers the entire map — 2D simplex noise creates organic wall patterns. Landmark influences warp the noise density, creating natural transitions between dense and sparse areas.
4. **Center anchor rotation/mirroring** — the hand-placed center geometry (savepoint, entry portal, surrounding structure) is rotated in 90° increments and/or mirrored before generation begins, so even the starting area feels different per seed.

Players can't memorize layouts. The labyrinth isn't always in the upper-left. The boss isn't always through the same corridor. The world organically reorganizes itself around wherever the landmarks land.

### Who does what

**Jason (designer) is responsible for:**
- Authoring the center anchor chunk (savepoint + entry portal + surrounding geometry)
- Authoring landmark chunk templates (boss arenas, portal rooms, safe zones — hand-tuned internal design)
- Defining each landmark's terrain influence (dense/sparse/moderate, influence radius, enemy bias)
- Authoring chunk templates for structured sub-areas within influence zones (reusable building blocks)
- Authoring obstacle blocks (small sub-patterns for scatter and fill)
- Tuning: base noise parameters, influence strengths, enemy budgets, difficulty curves
- Defining per-zone parameters: biome, base density, enemy composition, difficulty range

**Bob (code) is responsible for:**
- Seedable PRNG (xoshiro128** or PCG)
- Simplex noise generator (2D, multi-octave)
- Center anchor rotation/mirror system
- Hotspot position generator (per-seed, constrained scatter)
- Hotspot resolver (assign landmark types to positions, stamp landmark chunks)
- Influence field system (blend terrain character from landmark positions)
- Noise-to-terrain converter (threshold noise + influences into wall/empty decisions)
- Wall type refiner (influence-proximity circuit vs solid assignment)
- Connectivity validator (flood fill between landmarks, corridor carving if needed)
- Gate enforcer (gate-aware flood fill, alternate path detection, chokepoint sealing)
- Chunk stamper (stamp landmark chunks + structured sub-areas)
- Obstacle block scatter (in open areas)
- Enemy population system (budget-capped, spacing-aware, influence-biased)
- Integration into Zone_load() call sequence

### Prerequisites (implement before procgen)

1. **God mode editing tools** — Authoring tools for center anchor, landmark chunks, and obstacle blocks. Separate spec (Phase 1).
2. **Portals and zone transitions** — Already implemented.
3. **At least one active enemy type** — Already have mines, hunters, seekers, defenders.

Diagonal walls are **scrapped** — square cells are the architecture going forward.

### Implementation order (incremental, tight feedback loop)

| Step | What | Visible result |
|------|------|---------------|
| 1 | PRNG + simplex noise | Can generate and visualize a noise field |
| 2 | Noise-to-terrain conversion | See organic wall patterns across the whole map |
| 3 | Center anchor system | Center chunk placed with per-seed rotation/mirror |
| 4 | Hotspot position generator | See hotspot markers at different positions per seed |
| 5 | Hotspot resolver + landmark stamping | See landmark chunks placed at hotspot positions |
| 6 | Influence field system | Terrain density varies around landmarks |
| 7 | Connectivity validation + corridor carving | All landmarks reachable |
| 7.5 | Wall type refinement | Circuit tiles near landmarks, organic scatter in wilds |
| 7.6 | Gate enforcement | Gated landmarks only reachable through their gates |
| 8 | Obstacle block scatter | Style-matched sub-structures (structured/organic) |
| 9 | Chunk sub-areas in dense zones | Structured rooms within labyrinthine terrain |
| 10 | Enemy population with budget system | Enemies distributed through generated content |
| 11 | Tuning pass | Tweak noise, influences, density, enemy balance |

### Complexity assessment

| Component | Difficulty | Risk | Notes |
|-----------|-----------|------|-------|
| PRNG | Trivial | None | ~10 lines of C |
| Simplex noise | Easy-Medium | Low | Well-documented algorithm, ~100 lines |
| Center rotation/mirror | Easy | None | Transform cell coords before stamping |
| Hotspot position generator | Easy | None | Constrained random scatter |
| Hotspot resolver | Easy | None | Weighted pick + distance check |
| Influence field system | Medium | Low | Distance-weighted blending, straightforward math |
| Noise-to-terrain conversion | Medium | Low | Threshold + influence modulation |
| Connectivity validation | Medium | Low | BFS/flood fill between landmarks |
| Corridor carving | Medium | Medium | A* or direct pathing, carve through terrain |
| Gate enforcement | Medium | Medium | Gate-aware flood fill, chokepoint sealing, iterative verification |
| Chunk stamper | Easy | Low | Map_set_cell in a loop |
| Wall type refinement | Easy | None | Influence-proximity check + edge detection |
| Obstacle scatter | Easy-Medium | Low | Random placement with spacing + style matching |
| Enemy population | Medium | Low | Arithmetic on existing spawn code + budget cap |

---

## Inspiration

Two key references, extended with our own innovations:

1. **Dead Cells** — [Building the Level Design of a Procedurally Generated Metroidvania](https://www.gamedeveloper.com/design/building-the-level-design-of-a-procedurally-generated-metroidvania-a-hybrid-approach-). Six-layer hybrid: fixed world framework → hand-designed room templates → conceptual level graphs → algorithmic room placement → enemy population → loot distribution. Key takeaway: algorithms serve hand-crafted constraints, not the other way around.

2. **Spelunky** — [Part 1: Solution Path](https://tinysubversions.com/spelunkyGen/), [Part 2: Room Templates](https://tinysubversions.com/spelunkyGen2/). Key takeaways:
   - **Solution path first**: Build the critical path before filling around it. Reachability by construction, not validation.
   - **Hierarchical templates**: Room → probabilistic tiles → obstacle blocks. Multiple randomization layers within a single template.
   - **Mirroring**: Flip rooms for free variety.

**Our innovations beyond the references:**
- **Noise-driven whole-map terrain**: No fixed regions or rectangular zones. The entire map is one continuous generated environment with organic density variation.
- **Influence fields from landmarks**: Terrain character emerges from where landmarks land. Each landmark's influence type (dense/moderate/sparse) is designer-configured — a boss can be surrounded by labyrinth OR open terrain depending on the encounter design. The "zones" exist but they're soft-edged and move per seed.
- **Hotspot position generation**: Landmark candidates are generated per seed (not hand-placed), so landmark positions are truly unpredictable within constraints.
- **Center anchor rotation**: Even the starting area varies per seed through 90° rotation and mirroring.
- **Two terrain modes blend organically**: Dense areas use chunk-based structured fill, sparse areas use scatter-based obstacle placement, and the transition between them is driven by the continuous influence field — no hard boundaries.

---

## Definitions

| Term | Definition |
|------|-----------|
| **Zone** | A complete playable map. 1024×1024 grid cells of 100 world units each. One .zone file per zone. |
| **Map cell** | A single cell in the zone grid. 100×100 world units. Coordinates are 0-indexed. |
| **Seed** | A uint32 stored in the zone file, initializing the PRNG. Same zone + same seed = identical output. Required. The PRNG call order must be deterministic — no conditional rolls based on runtime state. |
| **Center anchor** | The hand-authored geometry at the zone center: savepoint, entry portal, and surrounding structure. Rotated/mirrored per seed before generation begins. |
| **Hotspot** | A procedurally generated candidate position where a landmark might be placed. Positions vary per seed. |
| **Landmark** | A key location (boss arena, encounter gate, portal room, safe zone) defined by a chunk template + terrain influence. Assigned to hotspot positions by the generator. |
| **Progression gate** | A landmark flagged as a mandatory chokepoint. The generator guarantees that certain other landmarks are ONLY reachable by passing through the gate landmark's chunk. Defined via `gate` lines in the zone file. This is the metroidvania's core progression enforcement — difficulty gates that demand specific loadouts. |
| **Terrain influence** | A property of a landmark that affects surrounding terrain density and character. Radiates outward from the landmark position, blending with the base noise and other influences. |
| **Influence field** | The combined effect of all landmark influences across the map. A continuous 2D field that modulates noise thresholds. |
| **Base noise** | 2D simplex noise covering the entire map. Multiple octaves at different frequencies create both macro and micro terrain variation. Seeded. |
| **Wall threshold** | The noise value below which a cell becomes a wall. Lower threshold = fewer walls (sparser). Higher threshold = more walls (denser). The influence field modulates this threshold across the map. |
| **Effect threshold** | A second noise threshold above which a cell stays empty. Cells between the wall threshold and effect threshold become **effect cells** — traversable cells with visual properties and optional gameplay effects. This creates the "three tile types" that make terrain feel alive. |
| **Effect cell** | A traversable map cell with visual appearance and optional gameplay effects. The third tile type between walls (solid) and empty space. In generic zones: data traces (subtle glowing circuit patterns, purely atmospheric). In themed zones: biome hazards (fire damage, ice friction, poison DOT, etc.). Placed by the noise generator in the middle band between wall and empty thresholds. |
| **Chunk** | A reusable room/structure template. Used for landmark rooms and structured sub-areas within dense terrain. Contains cell data, exits, probabilistic elements. |
| **Obstacle block** | A small pre-authored wall pattern (3×3, 4×4, etc.) scattered in moderate-to-sparse terrain for visual interest and tactical cover. |
| **Corridor** | A carved path connecting disconnected landmark areas. Generated only when the connectivity validator detects unreachable landmarks. Feels like intentional data conduits in cyberspace. |

---

## Layer 1: Zone Skeleton (Hand-Authored)

The designer authors a zone's **skeleton**: the minimal elements that define the zone's identity.

### Zone definition (in the .zone file)

| Element | Purpose |
|---------|---------|
| **Zone parameters** | Name, size (1024), biome, base density, difficulty range |
| **Seed** | PRNG seed for deterministic generation |
| **Center anchor chunk** | The starting area geometry (rotated/mirrored per seed) |
| **Cell type definitions** | Visual/physical cell types available for this zone's biome |
| **Landmark definitions** | What landmarks exist (boss arenas, encounter gates, portals, safe zones) + their terrain influences |
| **Progression gates** | Which landmarks are mandatory chokepoints and what they block access to |
| **Global tuning** | Noise parameters, influence strengths, enemy budgets, hotspot constraints |
| **Fixed spawns** | Any hand-placed enemy spawns that always appear regardless of generation |

### What the designer does NOT author

- **Region rectangles** — eliminated. No fixed spatial zones.
- **Hotspot positions** — generated per seed. The designer defines constraints (count, separation, edge margin), not positions.
- **Terrain layout** — entirely procedural. No hand-placed walls outside the center anchor and landmark chunks.

### Center anchor

The center anchor is a small hand-authored chunk placed at the zone center. It contains the entry savepoint, the zone's entry portal(s), and a recognizable starting structure.

**Per-seed transformation**: Before the center anchor is stamped, the generator applies one of 8 transformations (identity, 3 rotations, 4 mirrors) selected by the seed. This means even veteran players who recognize "this is The Origin's center" see it at a different orientation each run.

```c
typedef enum {
    TRANSFORM_IDENTITY,       // No change
    TRANSFORM_ROT90,          // 90° clockwise
    TRANSFORM_ROT180,         // 180°
    TRANSFORM_ROT270,         // 270° clockwise (90° counter-clockwise)
    TRANSFORM_MIRROR_H,       // Horizontal mirror
    TRANSFORM_MIRROR_V,       // Vertical mirror
    TRANSFORM_MIRROR_H_ROT90, // Mirror + 90°
    TRANSFORM_MIRROR_V_ROT90, // Mirror + 90° (equivalent to diagonal mirror)
    TRANSFORM_COUNT
} ChunkTransform;
```

The center anchor is always centered on the map (grid cell 512, 512). Its size should be modest (32×32 to 64×64) — large enough to be a recognizable starting area, small enough that it doesn't dominate the zone.

---

## Layer 2: Hotspot Generation & Landmark Placement

### Hotspot position generation

Instead of hand-placing hotspot markers, the generator creates hotspot candidate positions per seed. This ensures landmark positions are truly unpredictable.

**Constraints:**
- **Count**: Configurable per zone (e.g., 8-12 hotspots for a zone with 4-5 landmarks)
- **Edge margin**: Minimum distance from map edges (e.g., 100 cells / 10% of map size)
- **Center exclusion**: Minimum distance from center anchor (don't place a boss arena overlapping the start)
- **Separation**: Minimum distance between any two hotspots
- **Distribution**: Hotspots should be reasonably spread — no cluster of 5 in one corner

**Algorithm:**

```
function generate_hotspots(zone_params, rng):
    hotspots = []
    margin = zone_params.hotspot_edge_margin
    center_exclusion = zone_params.center_exclusion_radius
    min_sep = zone_params.hotspot_min_separation
    target_count = zone_params.hotspot_count

    attempts = 0
    while len(hotspots) < target_count and attempts < target_count * 50:
        x = rng_range(margin, zone_size - margin, rng)
        y = rng_range(margin, zone_size - margin, rng)

        // Check center exclusion
        if distance(x, y, center_x, center_y) < center_exclusion:
            attempts++
            continue

        // Check separation from existing hotspots
        too_close = false
        for each h in hotspots:
            if distance(x, y, h.x, h.y) < min_sep:
                too_close = true
                break
        if too_close:
            attempts++
            continue

        hotspots.append({x, y, weight: 1.0})
        attempts = 0  // Reset on success

    return hotspots
```

If the generator can't place enough hotspots (very constrained parameters), it logs a warning and works with what it has.

### Landmark definitions

Each landmark definition specifies what it is, what chunk template to use, and what terrain influence it carries. **Landmarks are the designer's primary tool for scripted content within procedural terrain.** They're not just "big important locations" — they're any curated encounter, difficulty gate, or set piece the designer wants placed at a generated position.

Common landmark types:
- **Boss arenas** — hand-crafted geometry with the boss spawn inside the chunk
- **Encounter gates** — scripted enemy compositions designed as difficulty gates (e.g., 3 swarmers + 2 defenders in a medium room). These are the metroidvania's progression walls — near-impossible without the right loadout, manageable once you have it.
- **Exit portals** — zone transition points to other zones
- **Safe zones** — savepoints and breathing room
- **Secret rooms** — hidden rewards, optional challenges
- **Any other curated encounter** — the type is a freeform label, not a fixed enum

Each landmark chunk includes `spawn_slot` lines for guaranteed enemy placement (probability 1.0) or probabilistic spawns (< 1.0). The enemies inside the chunk ARE the difficulty gate. The terrain influence around the landmark shapes the approach — dense labyrinth makes you fight through narrow corridors to reach the encounter, sparse open terrain lets you see it coming from far away.

```c
typedef struct {
    char type[32];                // Freeform label: "boss_arena", "swarmer_gate", "sniper_nest", etc.
    char chunk_file[64];          // Landmark chunk template file
    int priority;                 // Resolution order (lower = placed first)

    // Terrain influence
    TerrainInfluence influence;
} LandmarkDef;

typedef struct {
    InfluenceType type;           // INFLUENCE_DENSE, INFLUENCE_MODERATE, INFLUENCE_SPARSE
    float radius;                 // How far the influence extends (in grid cells)
    float strength;               // How strongly it overrides base noise (0.0-1.0)
    float falloff;                // How quickly influence fades with distance (1.0 = linear, 2.0 = quadratic)

    // Enemy composition bias (optional)
    char enemy_bias[32];          // "stalker_heavy", "swarmer_heavy", "mixed", etc.
    float enemy_density_mult;     // Multiplier on base enemy density within influence
} TerrainInfluence;

typedef enum {
    INFLUENCE_DENSE,              // Low noise threshold = lots of walls, labyrinthine
    INFLUENCE_MODERATE,           // Medium threshold = mixed terrain
    INFLUENCE_SPARSE,             // High threshold = few walls, open battlefield
    INFLUENCE_STRUCTURED          // Dense + use chunk templates for structured rooms
} InfluenceType;
```

### Landmark resolution

Weighted-pick algorithm operating on generated hotspots, with gate-aware placement constraints:

```
function resolve_landmarks(hotspots, landmark_defs, gate_defs, rng):
    placed = []

    // Sort landmark defs by priority (lower = placed first)
    // IMPORTANT: Gate landmarks should have lower priority than the landmarks
    // they gate, so gates are placed first and gated landmarks placed "behind" them.
    sort(landmark_defs, by priority)

    for each def in landmark_defs:
        // Filter hotspots: far enough from all placed landmarks
        candidates = []
        for each hotspot in hotspots:
            if not used(hotspot):
                too_close = false
                for each p in placed:
                    if distance(hotspot, p) < landmark_min_separation:
                        too_close = true
                        break
                if not too_close:
                    candidates.append(hotspot)

        if candidates is empty:
            // Fallback: pick unused hotspot maximizing min distance from placed
            candidates = unused_hotspots
            sort by max_min_distance, descending

        // Gate-aware placement constraint:
        // If this landmark is gated (must be reached through a gate), prefer
        // hotspots that are FARTHER from center than its gate landmark.
        // If this landmark IS a gate, prefer hotspots BETWEEN center and the
        // average position of already-placed landmarks it doesn't gate.
        gate_info = find_gate_for(def.type, gate_defs)
        if gate_info and gate_info.gate_landmark is placed:
            // This landmark is gated — prefer hotspots behind its gate
            gate_pos = position_of(gate_info.gate_landmark)
            weight_candidates_by_behind_gate(candidates, center, gate_pos)

        chosen = weighted_pick(candidates, rng)
        stamp_landmark_chunk(chosen.x, chosen.y, def.chunk_file, rng)
        placed.append({chosen.x, chosen.y, def})
        mark_used(chosen)

    return placed
```

**Placement heuristic for gates**: The `weight_candidates_by_behind_gate()` function weights hotspots higher when they're farther from center along the center→gate vector. This creates a natural spatial progression: center → gate → gated landmarks. It's a soft preference (weighted pick), not a hard constraint — the generator works with whatever hotspots are available but nudges gated landmarks to land behind their gate.

### Progression gates

Progression gates are the metroidvania's core enforcement mechanism. A `gate` line in the zone file declares that certain landmarks are ONLY reachable by passing through a specific gate landmark:

```
# gate <gate_landmark_type> <gated_landmark_type> [<gated_landmark_type> ...]
gate swarmer_gate boss_arena exit_portal_2
gate sniper_nest  safe_zone
```

This means:
- `boss_arena` and `exit_portal_2` are only reachable by passing through `swarmer_gate`
- `safe_zone` is only reachable by passing through `sniper_nest`
- All other landmarks are freely accessible from center

The generator enforces gates in two phases:
1. **Placement**: Gate landmarks placed first (lower priority), gated landmarks placed behind them relative to center (see landmark resolution above)
2. **Validation + sealing**: After terrain generation, the connectivity validator verifies each gate is a true chokepoint and seals any alternate paths (see Layer 6)

```c
typedef struct {
    char gate_type[32];           // The gate landmark's type label
    char gated_types[8][32];      // Landmarks that require passing through this gate
    int gated_count;              // Number of gated landmarks
} GateDef;
```

### Hotspot-carried influences

This is the key insight: when a landmark is placed at a hotspot, its terrain influence radiates from that position. The influence field is the sum of all landmark influences across the map.

Since hotspot positions change per seed, the influence field — and therefore the terrain character — reorganizes with every seed. A dense-configured landmark in the northeast means labyrinth in the northeast. Next seed it's in the southwest, and the labyrinth follows. A sparse-configured boss arena creates an open battlefield wherever it lands.

---

## Layer 3: Noise-Driven Terrain Generation

### Base noise

2D simplex noise covers the entire 1024×1024 map. Multiple octaves create terrain at different scales:

```c
typedef struct {
    int octaves;                  // Number of noise layers (4-6 typical)
    float base_frequency;         // Starting frequency (0.005-0.02 typical for 1024 maps)
    float lacunarity;             // Frequency multiplier per octave (2.0 typical)
    float persistence;            // Amplitude multiplier per octave (0.5 typical)
    float wall_threshold;         // Noise below this = wall (default -0.1)
    float effect_threshold;       // Noise between wall and effect = effect cell (default 0.15)
                                  // Noise above effect_threshold = empty
} NoiseParams;
```

**The three tile types**: The two thresholds divide the noise range into three bands:
- `noise < wall_threshold` → **Wall cell** (solid, blocks movement and LOS)
- `wall_threshold <= noise < effect_threshold` → **Effect cell** (traversable, visual + optional gameplay effect)
- `noise >= effect_threshold` → **Empty** (pure traversable space, grid over background cloudscape)

The middle band naturally concentrates at terrain transitions — the edges where dense walls give way to open space. This is exactly where visual interest matters most. Effect cells form organic fringes and scattered patches that break up both the density of walls and the emptiness of open ground.

**Multi-octave noise evaluation:**

```
function noise_at(x, y, params, rng_seed):
    value = 0.0
    amplitude = 1.0
    frequency = params.base_frequency
    max_amplitude = 0.0

    for octave in 0..params.octaves:
        // Offset each octave by seed-derived values for uniqueness
        ox = x * frequency + seed_offset_x[octave]
        oy = y * frequency + seed_offset_y[octave]
        value += simplex2d(ox, oy) * amplitude
        max_amplitude += amplitude
        amplitude *= params.persistence
        frequency *= params.lacunarity

    return value / max_amplitude  // Normalize to [-1, 1]
```

### Influence field

The influence field modulates **both** noise thresholds across the map. Each landmark's influence adjusts the local wall and effect thresholds, controlling not just wall density but also how much effect cell coverage appears:

- **INFLUENCE_DENSE**: Raises wall threshold (more walls) + narrows effect band → labyrinthine with thin effect fringes
- **INFLUENCE_SPARSE**: Lowers wall threshold (fewer walls) + widens effect band → open terrain with generous effect cell coverage
- **INFLUENCE_MODERATE**: Slight adjustments → balanced mix of all three tile types
- **INFLUENCE_STRUCTURED**: Like dense, but flags the area for chunk-based fill instead of pure noise

```
function compute_thresholds(grid_x, grid_y, base_wall_thresh, base_effect_thresh, placed_landmarks):
    wall_thresh = base_wall_thresh
    effect_thresh = base_effect_thresh

    for each landmark in placed_landmarks:
        dist = distance(grid_x, grid_y, landmark.x, landmark.y)
        inf = landmark.def.influence

        if dist > inf.radius:
            continue

        // Falloff: 1.0 at landmark position, 0.0 at radius edge
        t = 1.0 - (dist / inf.radius)
        falloff_weight = pow(t, inf.falloff) * inf.strength

        switch inf.type:
            INFLUENCE_DENSE:
                wall_thresh += falloff_weight * DENSE_SHIFT     // More walls
                effect_thresh += falloff_weight * DENSE_SHIFT * 0.5  // Narrow effect band
            INFLUENCE_SPARSE:
                wall_thresh -= falloff_weight * SPARSE_SHIFT    // Fewer walls
                effect_thresh += falloff_weight * SPARSE_SHIFT * 0.3 // Wider effect band
            INFLUENCE_MODERATE:
                wall_thresh += falloff_weight * MODERATE_SHIFT  // Slight wall increase
                // Effect threshold unchanged — natural band width
            INFLUENCE_STRUCTURED:
                wall_thresh += falloff_weight * DENSE_SHIFT
                effect_thresh += falloff_weight * DENSE_SHIFT * 0.5
                // Also flag for structured fill (see Layer 4)

    wall_thresh = clamp(wall_thresh, -0.8, 0.8)
    effect_thresh = clamp(effect_thresh, wall_thresh + 0.05, 0.95)  // Always some gap
    return {wall_thresh, effect_thresh}
```

### Noise-to-terrain conversion

For each cell in the map, the noise value is compared against two thresholds to produce one of three tile types:

```
function generate_terrain(zone_params, placed_landmarks, rng):
    noise_params = zone_params.noise

    for grid_y in 0..zone_size:
        for grid_x in 0..zone_size:
            // Skip cells occupied by center anchor or landmark chunks
            if cell_is_fixed(grid_x, grid_y):
                continue

            noise_val = noise_at(grid_x, grid_y, noise_params, seed)
            thresholds = compute_thresholds(grid_x, grid_y,
                                            noise_params.wall_threshold,
                                            noise_params.effect_threshold,
                                            placed_landmarks)

            if noise_val < thresholds.wall:
                // WALL — solid, blocks movement and LOS
                cell_type = pick_wall_type(zone_params.biome, rng)
                Map_set_cell(grid_x, grid_y, cell_type)

            else if noise_val < thresholds.effect:
                // EFFECT CELL — traversable, visual + optional gameplay effect
                cell_type = pick_effect_type(zone_params.biome, rng)
                Map_set_cell(grid_x, grid_y, cell_type)

            // else: EMPTY — pure traversable space (grid over background cloudscape)
```

**Initial wall type assignment**: During terrain generation, `pick_wall_type()` assigns a preliminary wall type (e.g., solid). The wall type refinement pass (Layer 3.5) then selectively upgrades walls to circuit type based on influence proximity and edge exposure.

This produces organic terrain with three distinct tile types that:
- Varies per seed (noise is seeded)
- Is denser near dense-influence landmarks with thin effect fringes
- Is sparser near sparse-influence landmarks (safe zones) with generous effect cell scatter
- Transitions smoothly between areas — effect cells naturally concentrate at terrain edges
- Has no visible boundaries or rectangular regions

### Terrain character notes

**Dense areas** (near INFLUENCE_DENSE landmarks): Wall threshold is high, so most cells become walls. The result is labyrinthine — narrow passages winding through dense walls. Effect cells appear as thin fringes along corridor walls, adding subtle visual texture to tight spaces. Feels like navigating compressed data structures.

**Sparse areas** (near INFLUENCE_SPARSE landmarks): Wall threshold is low, so few cells become walls. The wider effect band means more effect cells are scattered through open space, breaking up the emptiness with subtle visual interest. The background cloudscape is still the dominant visual — effect cells are an accent layer, not a replacement. Feels like an open cyberspace battlefield with traces of active data.

**Moderate areas** (between influences, or near INFLUENCE_MODERATE landmarks): Balanced mix of all three tile types. Walls provide structure, effect cells add texture at transitions, empty space lets the cloudscape breathe. Natural connective tissue between the extremes.

**The transition** between dense and sparse is gradual and organic — the influence falloff creates a smooth density gradient. Effect cells emerge naturally in the transition band, visually marking the boundary between terrain types without hard edges.

### Effect cell types per biome

The effect cell is the same system everywhere — a traversable cell with visual properties defined in the zone's cell type definitions. What changes per biome is the visual appearance and optional gameplay behavior:

**Generic zones (Origin, Generic A/B/C) — Data Traces**:
- Faint glowing circuit-like patterns on the grid — subtle, atmospheric
- Complement the zone's background cloudscape palette (e.g., faint cyan-white against purple clouds)
- Render through foreground bloom for soft glow
- **No gameplay effect** — purely visual texture that makes the world feel alive and active
- Varying visual density adds atmosphere: some areas feel more "active" than others without telling the player why

**Themed zones — Biome Hazards** (examples, not final):
- **Fire zone**: Ember cells — faint orange glow, deal damage-over-time while standing on them
- **Ice zone**: Frost cells — pale blue shimmer, reduce friction (momentum/sliding effect)
- **Poison zone**: Toxic cells — sickly green pulse, tick feedback while standing on them (slows feedback decay)
- **Blood zone**: Drain cells — dark red veins, slowly drain integrity while standing on them
- **Electric zone**: Charge cells — crackling blue-white, periodic damage pulses at intervals
- **Void zone**: Distortion cells — visual warping/glitch effect, interfere with minimap rendering

Same noise generation, same two thresholds, same influence system. The only difference is what `pick_effect_type()` returns per biome and what gameplay behavior (if any) the cell type triggers. Themed zone hazards create terrain-reading gameplay — players think about WHERE they fight, not just how. Kiting a seeker through fire cells versus clean ground changes the positioning calculus.

---

## Layer 3.5: Wall Type Refinement

After terrain generation assigns walls, a post-processing pass refines which wall cells use the **solid** type versus the **circuit** type. This creates visual variety that's modulated by landmark influence — geometric near landmarks, organic in the wilds.

### Two wall types

The zone's cell type definitions include both solid and circuit wall types (as already exists in The Origin):
- **Solid**: The base wall type. Opaque fill, no internal pattern.
- **Circuit**: Internal circuit-trace pattern. Visually distinctive, implies structure and purpose.

### Influence-proximity-based placement

The key insight: circuit tiles feel geometric and intentional near landmarks, but scattered and organic far from them. This is controlled by the local influence strength at each wall cell.

**Near landmarks (high influence strength)** — Edge detection mode:
- Circuit placement uses **edge detection**: walls with at least one exposed face (adjacent to empty or effect cell) get circuit type
- This creates geometric, circuit-lined edges around landmark structures — feels deliberate and architectural
- Interior walls (surrounded by other walls on all sides) stay solid — they're hidden anyway
- Probability of edge-detected walls becoming circuit: 80-95%

**In the wilds (low/no influence strength)** — Organic scatter mode:
- Circuit placement uses **random scatter**: any wall has a low base probability of becoming circuit
- Creates the organic, random-looking circuit appearance seen in The Origin's southern terrain
- No relationship to edges — circuit tiles appear anywhere, breaking up solid monotony
- Base probability: 10-20% (tunable per zone)

**Transition**: The influence falloff creates a smooth gradient between the two modes. At moderate influence, it's a blend — some edge-detected circuits plus some random scatter.

```
function refine_wall_types(terrain, placed_landmarks, zone_params, rng):
    for each wall cell (grid_x, grid_y):
        // Compute max influence strength at this cell
        max_influence = 0.0
        for each landmark in placed_landmarks:
            dist = distance(grid_x, grid_y, landmark.x, landmark.y)
            inf = landmark.def.influence
            if dist < inf.radius:
                t = 1.0 - (dist / inf.radius)
                strength = pow(t, inf.falloff) * inf.strength
                max_influence = max(max_influence, strength)

        // Determine circuit probability
        is_edge = has_exposed_face(grid_x, grid_y, terrain)

        if max_influence > 0.3:
            // Near landmark: edge detection mode
            // Blend toward edge-detection as influence increases
            edge_weight = smoothstep(0.3, 0.7, max_influence)
            if is_edge:
                circuit_prob = lerp(zone_params.wild_circuit_prob,
                                    zone_params.landmark_circuit_prob,
                                    edge_weight)
            else:
                circuit_prob = zone_params.wild_circuit_prob * (1.0 - edge_weight * 0.5)
        else:
            // In the wilds: organic scatter
            circuit_prob = zone_params.wild_circuit_prob

        if rng_float(rng) < circuit_prob:
            set_cell_type(grid_x, grid_y, zone_params.circuit_type)
        // else: stays solid (default from terrain generation)
```

### Zone file parameters

```
# Wall type refinement
circuit_prob_landmark 0.9      # Probability of edge-detected walls becoming circuit near landmarks
circuit_prob_wild 0.15         # Base probability of any wall becoming circuit in the wilds
```

### Design rationale

This mirrors the hand-crafted feel of The Origin. Around the portal and savepoint, circuit tiles form deliberate geometric patterns along edges — they look placed with purpose. Moving south into the open terrain, circuit tiles scatter more randomly among the solid walls, creating an organic, lived-in feel. The influence system automates this transition: landmarks are the "purpose" that circuit patterns gather around, and the wilds are the raw digital frontier where patterns emerge randomly.

---

## Layer 4: Structured Sub-Areas

Within dense influence zones (INFLUENCE_STRUCTURED), pure noise terrain can be replaced or augmented with chunk-based structured rooms. This gives the designer control over room shapes and combat encounters in high-density areas while keeping the organic feel of noise-generated corridors between rooms.

### When to use structured fill

The INFLUENCE_STRUCTURED type is for areas where the designer wants hand-crafted room geometry — boss approach corridors, puzzle rooms, specific combat arenas. It's optional. INFLUENCE_DENSE with pure noise produces perfectly playable labyrinthine terrain without any chunks.

### Structured fill algorithm

Within an INFLUENCE_STRUCTURED zone (defined by the influence radius of a landmark):

1. **Identify the structured area**: All cells within the influence radius where the influence strength exceeds a threshold (e.g., 0.5 — the inner portion of the influence)
2. **Compute a coarse grid**: Divide the structured area into chunk-sized cells (e.g., 16×16)
3. **Generate a solution path**: Spelunky-style walk from the area's edge toward the landmark, ensuring connectivity
4. **Fill with chunks**: Path cells get traversable chunks, non-path cells get sealed or connected filler chunks
5. **Blend at edges**: Cells at the structured area's boundary blend into the surrounding noise terrain — use noise to decide whether edge chunks have exits to the open terrain

This preserves the Spelunky chunk system for areas that benefit from it, while the rest of the map uses organic noise terrain. The structured area is embedded within the noise terrain and connected to it, not isolated in a rectangle.

### Chunk templates (unchanged from original spec)

```c
typedef struct {
    char name[64];
    int width, height;           // Grid dimensions (8-64)

    ChunkCategory category;      // CHUNK_COMBAT, CHUNK_CORRIDOR, CHUNK_JUNCTION, etc.
    char biome[32];
    int difficulty;
    uint8_t flags;               // CHUNK_NO_HMIRROR, CHUNK_NO_VMIRROR

    ExitConfig exit_config;      // Bitmask: EXIT_LEFT | EXIT_RIGHT | EXIT_TOP | EXIT_BOTTOM
    Exit exits[4];
    int exit_count;

    ChunkWall walls[MAX_CHUNK_CELLS];
    int wall_count;
    ChunkEmpty empties[MAX_CHUNK_CELLS];
    int empty_count;
    ChunkMaybe maybes[MAX_CHUNK_CELLS];
    int maybe_count;

    ObstacleZone obstacle_zones[MAX_OBSTACLE_ZONES];
    int obstacle_zone_count;

    ChunkSpawnSlot spawn_slots[MAX_CHUNK_SPAWNS];
    int spawn_slot_count;
} ChunkTemplate;
```

### Chunk file format (unchanged)

**Generic chunk** (used in structured sub-areas):
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

wall 0 0 solid
empty 0 6
maybe 4 4 solid 0.5

obstacle_zone 6 2 4 4 pillar_cluster,wall_segment,empty 0.7
spawn_slot 8 8 mine 0.3
```

**Encounter landmark chunk** (scripted difficulty gate — 3 swarmers + 2 defenders):
```
chunk swarmer_gate_01
size 24 24
category combat
biome neutral
difficulty 3

exits LRTB
exit left 10 4
exit right 10 4
exit top 4 10
exit bottom 4 10

# Arena walls — open interior with pillar cover
wall 0 0 circuit
wall 23 0 circuit
wall 0 23 circuit
wall 23 23 circuit
empty 12 12

# Guaranteed enemy spawns — this IS the difficulty gate
spawn_slot 6 12 swarmer 1.0
spawn_slot 12 6 swarmer 1.0
spawn_slot 18 12 swarmer 1.0
spawn_slot 8 18 defender 1.0
spawn_slot 16 18 defender 1.0
```
The `1.0` probability means these enemies always spawn. This encounter is designed to demand AoE subroutines — single-target builds will be overwhelmed by the swarm while defenders heal them.

---

## Layer 5: Obstacle Block Scatter

In moderate-to-sparse terrain, individual noise-generated walls can feel too random — just scattered dots. Obstacle blocks add tactical structure: cover positions, chokepoints, pillar clusters.

### Scatter algorithm

The scatter algorithm selects obstacle blocks based on local influence strength. Near landmarks, it picks from the **structured** pool. In the wilds, it picks from the **organic** pool. At moderate influence, it blends between both pools.

```
function scatter_obstacles(zone_params, placed_landmarks, terrain, rng):
    obstacle_lib = load_obstacle_blocks(zone_params.biome)
    structured_blocks = filter(obstacle_lib, style == OBSTACLE_STRUCTURED)
    organic_blocks = filter(obstacle_lib, style == OBSTACLE_ORGANIC)

    for each placed landmark:
        inf = landmark.def.influence
        if inf.type == INFLUENCE_DENSE or inf.type == INFLUENCE_STRUCTURED:
            continue  // Dense areas don't need scattered obstacles

        // Scatter within this landmark's influence radius
        area = influence_area(landmark)
        budget = area * inf.obstacle_density
        placed = []

        attempts = 0
        while len(placed) < budget and attempts < budget * 20:
            x = rng_range(landmark area bounds, rng)
            y = rng_range(landmark area bounds, rng)

            if not in_influence(x, y, landmark):
                attempts++
                continue

            // Pick block style based on local influence strength
            local_strength = compute_influence_strength(x, y, placed_landmarks)
            if local_strength > 0.5:
                block = pick_uniform(structured_blocks, rng)
            else if local_strength > 0.2:
                // Blend zone — either pool
                if rng_float(rng) < local_strength:
                    block = pick_uniform(structured_blocks, rng)
                else:
                    block = pick_uniform(organic_blocks, rng)
            else:
                block = pick_uniform(organic_blocks, rng)

            if too_close(x, y, placed, min_spacing):
                attempts++
                continue

            if overlaps_fixed(x, y, block):
                attempts++
                continue

            stamp_obstacle_block(x, y, block, rng)  // May mirror
            placed.append({x, y, block})
            attempts = 0

    // Also scatter in "no man's land" — organic blocks only
    scatter_neutral_obstacles(zone_params, placed_landmarks, terrain,
                              organic_blocks, rng)
```

### Obstacle blocks

```c
typedef enum {
    OBSTACLE_STRUCTURED,   // Geometric, deliberate patterns — placed near landmarks
    OBSTACLE_ORGANIC        // Random-looking, natural patterns — placed in the wilds
} ObstacleStyle;

typedef struct {
    char name[64];
    int width, height;
    ObstacleStyle style;   // Determines where the block gets placed
    ChunkWall cells[MAX_OBSTACLE_CELLS];
    int cell_count;
} ObstacleBlock;
```

Small pre-authored patterns (3×3, 4×4, etc.) that get stamped into terrain. Mirrored randomly for variety.

**Structured blocks** feature deliberate geometric patterns — symmetry, circuit-heavy edges, recognizable shapes. They're placed near landmarks where the terrain feels architectural and purposeful.

**Organic blocks** feature random-looking, asymmetric wall clusters — the kind of terrain that looks naturally formed rather than designed. They're placed in the wilds, far from landmark influence, where terrain should feel raw and untamed.

The designer flags each obstacle block with its style when authoring. The scatter algorithm selects blocks based on local influence strength at the placement position.

---

## Layer 6: Connectivity Validation, Gate Enforcement & Corridor Carving

After terrain generation, the map must be validated for two properties:
1. **Basic connectivity**: All landmarks reachable from center anchor
2. **Gate enforcement**: Gated landmarks reachable ONLY through their gate landmark

This is the metroidvania's progression guarantee. If the zone file says `gate swarmer_gate boss_arena`, then there is NO path from center to boss_arena that doesn't pass through swarmer_gate. Period.

### Phase 1: Basic connectivity validation

```
function validate_connectivity(center, placed_landmarks, terrain):
    // Flood fill from center anchor
    reachable = flood_fill(center.x, center.y, terrain)

    unreachable = []
    for each landmark in placed_landmarks:
        if not reachable[landmark.x][landmark.y]:
            unreachable.append(landmark)

    return unreachable
```

### Phase 2: Corridor carving (for unreachable landmarks)

For each unreachable landmark, carve a corridor from the nearest reachable point:

```
function carve_corridors(center, unreachable_landmarks, terrain, rng):
    for each landmark in unreachable_landmarks:
        // Find the nearest reachable cell to this landmark
        target = nearest_reachable_cell(landmark, reachable_set)

        // A* pathfind from landmark to target through walls
        // Cost: empty cell = 1, wall cell = 5 (prefer existing open space)
        path = astar(landmark, target, terrain, wall_cost=5)

        // Carve the path — clear walls along it
        // Width: 2-3 cells for a corridor feel (not just 1-cell tunnels)
        for each cell in path:
            clear_corridor_cells(cell, corridor_width=2, terrain)

        // Add some noise to the corridor edges for organic feel
        roughen_corridor_edges(path, rng)

        // Re-flood-fill to update reachable set
        reachable_set = flood_fill(center, terrain)
```

**Corridor aesthetics**: Carved corridors are intentional game geometry — in the fiction, they're data conduits or network pathways through dense digital terrain. They should feel deliberate:
- 2-3 cells wide (not cramped single-cell tunnels)
- Slightly rough edges (noise perturbation) so they don't look laser-cut
- Optional: different cell type for corridor walls (to visually distinguish carved paths)

### Phase 3: Gate enforcement

After all landmarks are reachable, verify that gated landmarks are ONLY reachable through their designated gate. If alternate paths exist, seal them.

```
function enforce_gates(center, placed_landmarks, gate_defs, terrain, rng):
    for each gate_def in gate_defs:
        gate_landmark = find_placed(gate_def.gate_type, placed_landmarks)
        gate_cells = get_chunk_cells(gate_landmark)  // All cells in the gate chunk

        for each gated_type in gate_def.gated_types:
            gated_landmark = find_placed(gated_type, placed_landmarks)

            // Flood fill from center, treating gate chunk cells as WALLS
            // (impassable). If the gated landmark is still reachable,
            // there's an alternate path that bypasses the gate.
            reachable_without_gate = flood_fill_excluding(
                center.x, center.y, terrain, gate_cells)

            if reachable_without_gate[gated_landmark.x][gated_landmark.y]:
                // Alternate path exists — seal it
                seal_alternate_paths(center, gate_landmark, gated_landmark,
                                     gate_cells, terrain, rng)

function seal_alternate_paths(center, gate, gated, gate_cells, terrain, rng):
    // Strategy: find the alternate path and fill it with walls.
    // Iterate until no alternate path exists.
    max_iterations = 10
    for i in 0..max_iterations:
        reachable = flood_fill_excluding(center, terrain, gate_cells)

        if not reachable[gated.x][gated.y]:
            break  // Gate is now enforced

        // A* from gated landmark to center, avoiding gate chunk.
        // This finds the alternate path.
        alt_path = astar(gated, center, terrain,
                         wall_cost=1, gate_cells=impassable)

        // Find the narrowest point of the alternate path — the best
        // place to seal it. Sealing at a chokepoint minimizes wall fill.
        chokepoint = find_narrowest_point(alt_path, terrain)

        // Fill walls across the chokepoint (perpendicular to path direction)
        seal_chokepoint(chokepoint, terrain, seal_width=3)

    if i == max_iterations:
        log_warning("Could not enforce gate %s after %d iterations",
                    gate.type, max_iterations)
```

**Why this works**: Gate landmarks naturally use dense influence, which creates labyrinthine terrain around them. This means very few alternate paths exist — typically 0-2 small gaps in the wall of dense terrain. The sealing pass closes those gaps surgically. The result is a terrain barrier that looks organic (it's 95% noise-generated walls) with a few small fills that are invisible among the dense terrain.

**Sealing aesthetics**: Sealed chokepoints should use the same wall types as surrounding terrain so they're invisible. The `seal_width=3` creates a small wall patch that blends with the dense terrain around the gate. If the gate landmark's influence is dense enough, the sealed cells are surrounded by other walls and completely invisible to the player.

### Phase 4: Final validation

After gate enforcement, re-validate that ALL landmarks are still reachable from center (sealing might have disconnected something). If so, carve a corridor through the gate chunk to restore connectivity:

```
function final_validation(center, placed_landmarks, gate_defs, terrain, rng):
    unreachable = validate_connectivity(center, placed_landmarks, terrain)
    if unreachable is not empty:
        // Sealing disconnected something — carve corridors THROUGH gates
        // (not around them) to restore connectivity
        for each landmark in unreachable:
            gate = find_gate_for(landmark, gate_defs)
            if gate:
                // Carve: center-side → gate entrance → gate exit → gated landmark
                carve_through_gate(center, gate, landmark, terrain, rng)
            else:
                // Not gated, just disconnected — normal corridor carving
                carve_corridor(center, landmark, terrain, rng)
```

### Gate enforcement summary

The full connectivity + gate enforcement sequence:
1. Flood fill from center — find unreachable landmarks
2. Carve corridors to unreachable landmarks
3. For each gate: verify gated landmarks are only reachable through the gate
4. Seal alternate paths that bypass gates
5. Final validation — ensure everything is still reachable (carve through gates if needed)

---

## Layer 7: Enemy Population

### Two-pass placement

**Pass 1 — Fixed spawns**: `spawn` lines from the .zone file placed first. Non-negotiable.

**Pass 2 — Budget-controlled procedural spawns**:

```
function populate_enemies(zone_params, placed_landmarks, terrain, rng):
    // Global budget based on zone difficulty
    total_open_cells = count_open_cells(terrain)
    base_density = difficulty_to_density(zone_params.difficulty_avg)
    global_budget = total_open_cells * base_density
    global_budget -= count_fixed_spawns(zone_params)

    if global_budget <= 0: return

    spawned = 0
    attempts = 0

    while spawned < global_budget and attempts < global_budget * 20:
        // Pick random open cell
        x = rng_range(0, zone_size, rng)
        y = rng_range(0, zone_size, rng)

        if cell_is_wall(x, y) or cell_is_fixed(x, y):
            attempts++
            continue

        if enemy_within(world_pos(x, y), MIN_ENEMY_SPACING):
            attempts++
            continue

        // Determine enemy type based on local influence
        enemy_type = pick_enemy_type_for_position(
            x, y, placed_landmarks, zone_params, rng)

        spawn_enemy(enemy_type, world_pos(x, y))
        spawned++
        attempts = 0
```

### Influence-biased enemy selection

Each landmark's `enemy_bias` field weights enemy type selection within its influence radius:

```
function pick_enemy_type_for_position(x, y, landmarks, zone_params, rng):
    // Blend enemy weights from all active influences at this position
    weights = default_enemy_weights(zone_params)

    for each landmark in landmarks:
        dist = distance(x, y, landmark.x, landmark.y)
        inf = landmark.def.influence
        if dist > inf.radius:
            continue

        t = 1.0 - (dist / inf.radius)
        blend = pow(t, inf.falloff) * inf.strength

        bias_weights = get_bias_weights(inf.enemy_bias)
        // Blend: weights = lerp(weights, bias_weights, blend)
        for each enemy_type:
            weights[enemy_type] = lerp(weights[enemy_type],
                                       bias_weights[enemy_type], blend)

    return weighted_pick(enemy_types, weights, rng)
```

This means stalker-heavy areas naturally form around landmarks tagged with `"stalker_heavy"`, swarmer swarms cluster near swarmer-biased landmarks, etc. The enemy composition varies organically across the map, driven by landmark influences.

### Density modulation

Enemy density also varies by influence. Dense labyrinthine areas might have fewer but deadlier enemies (tight corridors amplify danger). Open areas might have more enemies spread across the field. The `enemy_density_mult` field on each influence controls this:

```
local_density = base_density
for each landmark influence at (x, y):
    local_density *= lerp(1.0, inf.enemy_density_mult, blend_weight)
```

### Danger budget (future)

When multiple enemy types exist with varying difficulty, each carries a danger rating. Budget becomes danger-weighted — a single stalker "costs" more than a mine.

---

## Layer 8: Resource Distribution

- **Fragment drops**: Per-enemy-type, unchanged from current system.
- **Bonus pickups**: Spawned near obstacle clusters and in cleared areas of dense terrain.
- **Pickup density**: Inversely correlates with zone difficulty.

No new systems needed — tuning numbers on existing mechanics.

---

## Architecture Integration

### Existing systems

| System | Role | Changes |
|--------|------|---------|
| Zone files (.zone) | Zone skeleton + parameters | Add new line types for procgen config |
| Map grid | Target for generator writes | None — Map_set_cell / Map_clear_cell exist |
| Cell types + cell pool | Wall + effect cell types per biome | Add traversable effect cell category + per-biome effect types |
| God mode | Author content | New editing tools (separate spec) |
| Entity spawns | Fixed spawns respected by budget | None |
| Fragment/progression | Resource layer | None |

### New systems

| System | Files | Complexity | Notes |
|--------|-------|-----------|-------|
| PRNG | `prng.c/h` | Trivial | xoshiro128** or PCG |
| Simplex noise | `noise.c/h` | Easy-Medium | 2D simplex, multi-octave wrapper |
| Influence field | `procgen.c/h` | Medium | Distance-weighted blending from landmarks |
| Hotspot generator | `procgen.c/h` | Low | Constrained random scatter |
| Landmark resolver | `procgen.c/h` | Low-Medium | Weighted assignment + chunk stamping |
| Terrain generator | `procgen.c/h` | Medium | Noise + dual threshold + influence modulation → 3 tile types |
| Wall type refiner | `procgen.c/h` | Low | Influence-proximity edge detection + random scatter |
| Connectivity validator | `procgen.c/h` | Medium | Flood fill + A* corridor carving |
| Gate enforcer | `procgen.c/h` | Medium | Gate-aware flood fill + chokepoint sealing |
| Chunk loader | `chunk.c/h` | Medium | Parse, validate, build library |
| Obstacle loader | Part of chunk.c | Low | Parse obstacle block files |
| Structured sub-area gen | `procgen.c/h` | Medium-High | Spelunky walk within influence zones |
| Enemy populator | `procgen.c/h` | Medium | Budget, spacing, influence-biased selection |
| Center anchor system | `procgen.c/h` | Low | Rotation/mirror + stamp |

### Generation call sequence

```
Zone_load("fire_zone.zone"):
    1. Parse zone skeleton (cell types, fixed spawns, center anchor, landmark defs,
       gate defs, params)
    2. Initialize PRNG with seed
    3. Apply center anchor with per-seed rotation/mirror
    4. Generate hotspot positions (constrained scatter)
    5. Resolve landmarks → gate-aware assignment to hotspots, stamp landmark chunks
    6. Compute influence field from placed landmarks
    7. Generate base terrain (noise + dual thresholds + influence → walls, effect cells, empty)
    7.5. Refine wall types (influence-proximity circuit vs solid assignment)
    8. Structured sub-area fill (within INFLUENCE_STRUCTURED zones)
    9. Scatter obstacle blocks (style-matched: structured near landmarks, organic in wilds)
    10. Validate connectivity (flood fill from center)
    11. Carve corridors to unreachable landmarks
    12. Enforce progression gates (verify gate chokepoints, seal alternate paths)
    13. Final connectivity validation (ensure sealing didn't disconnect anything)
    14. Populate enemies (budget-controlled, influence-biased)
    15. Distribute resources
    16. Gameplay begins
```

### Zone file format (complete example)

```
name Fire Zone
size 1024
biome fire
seed 48291

# Cell type definitions — walls
celltype fire_wall 200 50 0 255 255 100 0 255 none
celltype fire_circuit 150 30 0 255 200 80 0 255 circuit

# Cell type definitions — effect cells (traversable, tagged with "effect" flag)
# effecttype <id> <primary_rgba> <outline_rgba> <pattern> [gameplay_effect] [effect_params]
effecttype fire_ember 180 40 0 180 200 60 0 120 ember dot 5
#                                                        ^effect ^param (5 DPS)

# Per-zone background colors
bgcolor 0 200 50 0
bgcolor 1 150 30 0
bgcolor 2 255 100 0
bgcolor 3 100 20 0

# Music playlist
music ./resources/music/fire_zone_1.mp3
music ./resources/music/fire_zone_2.mp3

# Center anchor (rotated/mirrored per seed)
center_anchor fire_center.chunk

# Fixed enemy spawns (always present)
spawn mine 50000.0 50000.0

# === PROCEDURAL GENERATION CONFIG ===

# Noise parameters (two thresholds for three tile types)
noise_octaves 5
noise_frequency 0.01
noise_lacunarity 2.0
noise_persistence 0.5
noise_wall_threshold -0.1
noise_effect_threshold 0.15

# Hotspot generation constraints
hotspot_count 10
hotspot_edge_margin 80
hotspot_center_exclusion 120
hotspot_min_separation 150

# Landmark definitions
# landmark <type> <chunk_file> <priority> <influence_type> <radius> <strength> <falloff> [enemy_bias] [density_mult]
#
# type = freeform label (boss_arena, encounter_gate, safe_zone, exit_portal, etc.)
# influence_type = dense | moderate | sparse | structured (terrain character around the landmark)
# Chunks contain spawn_slot lines for scripted enemy placement inside the landmark

# Boss: labyrinth approach, structured rooms around the encounter
landmark boss_arena boss_fire.chunk 1 structured 120 0.9 1.5 stalker_heavy 0.8

# Zone exit portals: moderate terrain, balanced approach
landmark exit_portal_1 portal_exit_fire.chunk 2 moderate 80 0.6 2.0 mixed 1.0
landmark exit_portal_2 portal_exit_fire.chunk 3 moderate 80 0.6 2.0 mixed 1.0

# Safe zone: sparse terrain, breathing room
landmark safe_zone safe_fire.chunk 4 sparse 100 0.8 1.0 none 0.3

# Encounter gates: scripted difficulty gates with specific enemy compositions
# These are the metroidvania progression walls — designed to demand specific loadouts
# Gate landmarks should use dense influence (creates natural wall barrier around chokepoint)
landmark swarmer_gate swarmer_gate_fire.chunk 5 dense 90 0.7 1.5 swarmer_heavy 1.5
landmark sniper_nest hunter_nest_fire.chunk 6 dense 60 0.9 2.0 hunter_heavy 1.2
landmark defender_line defender_gate_fire.chunk 7 dense 70 0.6 1.5 mixed 1.0

# Landmark minimum separation (grid cells)
landmark_min_separation 120

# === PROGRESSION GATES ===
# gate <gate_landmark_type> <gated_landmark_type> [<gated_landmark_type> ...]
# "You must pass through X to reach Y"
# The generator guarantees these are true chokepoints — no alternate paths.
gate swarmer_gate boss_arena exit_portal_2
gate sniper_nest safe_zone

# Enemy population
enemy_budget_base 0.003
enemy_min_spacing 15
difficulty_min 2
difficulty_max 4

# Wall type refinement
circuit_prob_landmark 0.9
circuit_prob_wild 0.15

# Obstacle scatter (blocks flagged as structured or organic)
obstacle_density 0.08
obstacle_min_spacing 10
```

**Generic zone example** (The Origin — data traces, no gameplay effect):

```
name The Origin
size 1024
biome neutral
seed 12345

celltype solid 20 0 20 255 128 0 128 255 none
celltype circuit 10 20 20 255 64 128 128 255 circuit

# Data trace effect cells — traversable, purely atmospheric
# effecttype <id> <primary_rgba> <outline_rgba> <pattern>
effecttype data_trace 10 60 80 140 30 120 160 100 circuit
#          faint cyan-white glow against the purple cloudscape — no gameplay effect

bgcolor 0 89 26 140
bgcolor 1 51 26 128
bgcolor 2 115 20 102
bgcolor 3 38 13 89

center_anchor origin_center.chunk
noise_octaves 5
noise_frequency 0.01
noise_wall_threshold -0.1
noise_effect_threshold 0.15
# ... (rest of procgen config)
```

---

## Design Philosophy

### Why noise + influences over fixed regions?

1. **Unpredictability**: No fixed spatial zones means no memorizable layout patterns. The world reorganizes every seed.
2. **Organic transitions**: Influence falloff creates smooth density gradients — no hard "you entered the labyrinth" boundaries. Feels like exploring continuous cyberspace, not entering rooms.
3. **Emergent character**: The terrain's personality is tied to what's there, not where it is. Terrain density is a designed choice per landmark — dense terrain could mean a labyrinth boss, sparse terrain could mean an arena boss. Players learn to read the environment, but they can't assume "dense = boss" or "open = safe."
4. **Minimal authoring**: The designer defines landmarks and their influences. The generator does the heavy lifting across 1024×1024 cells.
5. **Open-world feel**: Players can go in any direction from the center. There's no predetermined path through regions. Exploration is genuine.

### Why three tile types?

The "three tile strategy" — wall, effect, empty — is a proven approach to creating terrain that feels alive with minimal complexity. Binary terrain (wall or empty) creates maps that are either dense corridors or featureless voids with nothing in between. The third tile type fills that gap:

1. **Transition texture**: Effect cells naturally concentrate at terrain edges (the noise middle band), giving organic visual fringes where walls meet open space instead of hard cutoffs.
2. **Atmospheric depth**: In generic zones, data traces add visual activity to traversable space without competing with the background cloudscape. They're an accent layer — subtle glowing patterns that make areas feel "active" or "alive."
3. **Biome identity with zero AI cost**: Themed zones swap the effect cell from visual-only (data traces) to gameplay hazard (fire, ice, poison) with no AI code changes. Same generation, same noise, different cell type per biome. Each zone feels mechanically distinct through terrain alone.
4. **Tactical terrain**: In themed zones, effect cells create a positioning layer. Players think about WHERE they fight — avoiding fire cells, using ice cells to kite enemies, managing poison exposure. This depth comes from the world itself.
5. **Procgen for free**: Two noise thresholds instead of one. Trivial code change, massive output quality improvement.

### Why keep chunks at all?

Chunks serve two purposes in this architecture:

1. **Landmark rooms**: Boss arenas, portal rooms, safe zones need hand-crafted geometry. These are the recognizable set pieces players remember across runs. Noise can't design a good boss arena.
2. **Structured sub-areas** (optional): Within INFLUENCE_STRUCTURED zones, chunk-based rooms provide curated combat encounters and navigational puzzles that pure noise can't create. This is an optional layer for areas that benefit from hand-crafted room design.

Everything else is noise-generated. Chunks are precision tools used where they matter, not the default for all terrain.

### The endgame loop

Seed-based generation + noise-driven terrain + influence-randomized landmarks = a game that stays fresh after mastery. A maxed-out Hybrid doesn't replay memorized rooms — the world regenerates. Daily/weekly challenge seeds, leaderboard runs on shared seeds, multiplayer on deterministic worlds — all fall out of this architecture.

### Center anchor rotation — why it matters

Even the starting area is the most-seen geometry in the game. Without rotation/mirroring, players would see the exact same center structure every run. With 8 possible transformations × the surrounding terrain changing due to different influence positions, the starting experience feels substantially different each seed. The center is recognizable (same chunk layout) but disorienting enough to prevent autopilot.

---

## Open Questions

1. **Noise algorithm**: Simplex vs OpenSimplex vs Perlin? Simplex is fastest and has the best visual properties (no grid artifacts). Lean toward simplex.

2. **Influence blending**: When two landmark influences overlap, how do they combine? Current spec uses additive threshold adjustment. Could also use max-wins or weighted average. Additive means two dense influences create extra-dense terrain (interesting). Start with additive, tune later.

3. **Structured sub-area triggers**: What influence strength threshold triggers chunk-based fill instead of pure noise? Needs tuning. Start with 0.5 (inner half of INFLUENCE_STRUCTURED radius).

4. **Corridor aesthetics**: Should carved corridors use a different cell type than noise walls? Would create visual "data conduit" corridors that look intentional. Probably yes — adds to the fiction.

5. **Generation timing**: Seed per save file (metroidvania — stable layout per playthrough), seed per zone entry (roguelike — fresh every visit), or seed changes at endgame milestones? Start with seed per save file.

6. **Center anchor size**: 32×32 vs 64×64? Larger = more recognizable starting area but less terrain variation near center. Recommend 48×48 as a compromise.

7. **Hotspot count vs landmark count**: How many surplus hotspots (hotspot_count - landmark_count) gives good variety without wasting generation effort? At least N+3 surplus recommended.

8. **Obstacle density in neutral zones**: Areas not strongly influenced by any landmark should still have some structure. What's the right baseline obstacle scatter density? Needs playtesting.

9. **Multi-zone connectivity**: Portals between zones are in landmark chunks. How do we ensure the zone graph is consistent? (Zone A's exit portal chunk matches zone B's entry portal chunk.) Probably: portal IDs are authored once in the zone skeleton, chunk templates reference them.

10. **Effect cell threshold tuning**: What wall_threshold / effect_threshold values produce the right balance? Too narrow a band = barely any effect cells. Too wide = effect cells dominate and dilute walls. Needs visual iteration. Start with wall=-0.1, effect=0.15 (roughly 25% of noise range produces effect cells).

11. **Effect cell collision model**: Effect cells are traversable — do they block LOS? Probably not (they're ground-level features, not walls). But themed zone variants could have LOS-blocking versions (e.g., void distortion cells that block vision).

12. **Effect cell rendering**: Should effect cells render through foreground bloom like walls, or as a separate subtle layer? Data traces should be subtle — maybe lower bloom intensity than walls. Themed hazards should be more visible (player needs to see fire cells to avoid them).

13. **Multiple effect types per zone**: Should zones have more than one effect cell type? E.g., fire zone has both "ember" (mild DOT) and "inferno" (severe DOT, rarer). Could use a second noise layer or probability split within the effect band. Start with one type per zone, add variety later if needed.
