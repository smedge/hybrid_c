# Spec: Procedural Generation — Phased Implementation Plan

## Overview

This is the master sequencing plan for implementing procedural generation and the supporting systems it requires. Each phase builds on the previous one and produces a visible, testable result.

Detailed implementation specs will be written for each phase right before work begins. This document captures the sequence, dependencies, scope, and key decisions so context isn't lost between sessions.

**Reference**: `plans/spec_procedural_generation.md` — the full procgen design spec (noise-driven terrain, influence fields, hotspot generation, landmark placement).

**Scrapped prerequisites**: Diagonal walls (scrapped — too many rendering edge cases, not worth the complexity). Square cells are the architecture going forward.

**Already done**: 6 enemy types (mine, hunter, seeker, defender + mgun turret as variant), player stats/feedback, skill system, zone loader, bloom rendering, portals (with zone transitions), savepoints (with save/load), enemy registry.

---

## Phase 1 — Godmode Entity Placement + Zone Navigation ✅ COMPLETE

**Goal**: Godmode can place all entity types and navigate between zones without relying on portals. The authoring baseline everything else builds on.

**Status**: Implemented. See `plans/spec_phase1_godmode.md` for full details.

**What's working:**
- Godmode placement modes (cells, enemies, savepoints, portals)
- Mode switching (Q/E), sub-type cycling (Tab)
- Zone jump (J key), new zone creation (N key)
- Per-zone background colors + music playlists
- All entity placement with undo support
- Zone save/load round-trip

---

## Phase 1.5 — Godmode Procgen Authoring Tools (if needed)

**Goal**: Any additional godmode tools needed for authoring procgen content that weren't covered in Phase 1.

### Potential additions
- **Center anchor authoring**: Paint a center anchor chunk in godmode, export to `.chunk` file. Alternatively, author chunks in a text editor (they're simple text format).
- **Hotspot constraint visualization**: Debug overlay showing hotspot positions, influence radii, and connectivity for a given seed. Essential for tuning.
- **Seed cycling**: Godmode hotkey to re-roll the seed and regenerate terrain without reloading the zone file. Fast iteration.
- **Influence field visualization**: Debug overlay showing the influence field as a color gradient (red = dense, blue = sparse). Helps tune landmark influence parameters.

### Decision
Evaluate after Phase 2 whether visual authoring tools are needed or if text-file editing + debug overlays are sufficient. The chunk format is simple enough that a text editor may be fine for bootstrapping. Debug overlays for visualization are likely essential regardless.

### Done When
- Can author center anchor chunks (text editor or godmode)
- Can visualize hotspot positions and influence fields in-game
- Can cycle seeds and regenerate in-place for rapid tuning

---

## Phase 2 — PRNG + Noise + Basic Terrain

**Goal**: Seedable PRNG and simplex noise generate visible organic terrain across the whole map. The foundation layer that everything else modulates.

### PRNG
- Seedable PRNG (xoshiro128** or PCG) in `prng.c/h`
- `Prng_seed(uint32_t)`, `Prng_next()`, `Prng_range(min, max)`, `Prng_float()`
- Deterministic: same seed = same sequence. Foundational for everything.

### Simplex Noise
- `noise.c/h` — 2D simplex noise implementation
- Multi-octave wrapper: `Noise_fbm(x, y, octaves, frequency, lacunarity, persistence, seed)`
- Returns normalized value in [-1, 1]
- Seed offsets each octave for per-seed uniqueness

### Noise-to-Terrain Conversion (Three Tile Types)
- Parse `seed`, `noise_*` parameters from zone file (extend zone.c parser)
- Parse `effecttype` definitions (traversable cells with visual properties)
- Iterate entire 1024×1024 grid
- **Two thresholds** produce three tile types:
  - `noise < wall_threshold` → wall cell (solid, blocks movement)
  - `wall_threshold <= noise < effect_threshold` → effect cell (traversable, visual + optional effect)
  - `noise >= effect_threshold` → empty (pure traversable space)
- Use zone's existing `celltype` definitions for walls, `effecttype` definitions for effect cells
- Skip cells already occupied by hand-placed content

### Effect Cell System
- New `effecttype` line in zone files defines traversable cells with visual properties
- Effect cells are MapCells with a `traversable` flag — rendered like walls but no collision
- Generic zones: data trace effect cells (faint circuit glow, purely atmospheric)
- Themed zones: hazard effect cells (fire DOT, ice friction, poison feedback drain, etc.)
- Effect cell gameplay behaviors are per-biome, resolved at runtime by cell type tag

### Zone File Extensions
- `seed <uint32>` — PRNG seed
- `noise_octaves <int>` — number of noise layers (default 5)
- `noise_frequency <float>` — base frequency (default 0.01)
- `noise_lacunarity <float>` — frequency multiplier per octave (default 2.0)
- `noise_persistence <float>` — amplitude multiplier per octave (default 0.5)
- `noise_wall_threshold <float>` — wall cutoff (default -0.1)
- `noise_effect_threshold <float>` — effect/empty cutoff (default 0.15)
- `effecttype <id> <primary_rgba> <outline_rgba> <pattern> [effect] [params]`

### Testing
- Godmode debug overlay: colorize cells by noise value (gradient visualization)
- Verify three distinct tile types appear: walls, effect cells, empty space
- Effect cells concentrate at terrain transitions (edges between dense and sparse)
- Try different seeds — verify determinism (same seed = same terrain)
- Try different noise parameters — verify density/scale changes
- Try different threshold gaps — verify effect cell coverage varies
- Walk through generated terrain — verify it feels organic, not grid-aligned
- Walk over effect cells — verify they don't block movement

### Done When
- PRNG produces deterministic sequences
- Simplex noise generates smooth 2D fields
- Terrain fills a 1024×1024 zone with three tile types: walls, effect cells, empty
- Effect cells render with their own visual appearance (distinct from walls)
- Effect cells are traversable (no collision)
- Different seeds produce different layouts
- Noise parameters visibly affect terrain character
- Can walk through generated terrain (reasonably traversable at default threshold)

---

## Phase 3 — Center Anchor + Hotspots + Landmarks

**Goal**: Center anchor placed with per-seed rotation/mirror. Hotspot positions generated per seed. Landmarks assigned to hotspots and stamped as chunks. The spatial identity of each zone emerges.

### Center Anchor System
- Parse `center_anchor <chunk_file>` from zone file
- Load and stamp the center chunk at map center (grid 512, 512)
- Apply per-seed transformation (1 of 8: identity, 3 rotations, 4 mirrors)
- Center cells marked as "fixed" — terrain generation skips them

### Chunk Loader
- `chunk.c/h` — parse `.chunk` text files (format in procgen spec)
- Chunk template library: load from `resources/chunks/` at startup
- Validation: exit traversability (flood fill), no maybe/empty overlap, obstacle zone bounds
- 5-step chunk stamping: fill walls → carve empties → resolve maybes → resolve obstacle zones → resolve spawn slots
- Transform support: rotation (0/90/180/270) and mirror (H/V) applied during stamp

### Hotspot Position Generator
- Parse hotspot constraint parameters from zone file:
  - `hotspot_count`, `hotspot_edge_margin`, `hotspot_center_exclusion`, `hotspot_min_separation`
- Generate N positions per seed using constrained random scatter
- Verify reasonable distribution (warn if clustered)

### Landmark Resolution
- Parse `landmark` definitions from zone file (type, chunk file, priority, influence params)
- Parse `landmark_min_separation`
- Parse `gate` definitions (which landmarks gate access to which other landmarks)
- **Gate-aware placement**: Gate landmarks placed first (lower priority), gated landmarks placed at hotspots behind their gate relative to center
- Resolve: assign landmark types to hotspot positions (priority order, weighted pick, separation enforcement, gate placement constraints)
- Stamp landmark chunks at resolved positions
- Mark landmark cells as "fixed" — terrain generation skips them

### Influence Field Computation
- For each landmark, compute terrain influence based on type, radius, strength, falloff
- Modulate **both** noise thresholds per cell: wall threshold AND effect threshold
- Dense influences: raise wall threshold (more walls) + narrow effect band (thin fringes)
- Sparse influences: lower wall threshold (fewer walls) + widen effect band (more effect cells)
- **This is where terrain character becomes landmark-driven** — the key feature
- Effect cells naturally concentrate at terrain transitions, giving organic fringes

### Integration with Phase 2
- Terrain generation now uses influence-modulated dual thresholds instead of flat base values
- Three tile types respond to influence: dense areas = mostly walls with thin effect fringes, sparse areas = few walls with generous effect cell scatter
- Center anchor and landmark chunks are stamped before terrain generation
- Terrain generation skips fixed cells

### Content Authoring
- Hand-write 1 center anchor chunk (text editor)
- Hand-write landmark chunks — any mix of:
  - Boss arenas (boss spawn + arena geometry)
  - Encounter gates (scripted enemy compositions as difficulty gates — e.g., 3 swarmers + 2 defenders)
  - Portal rooms (zone transition points)
  - Safe zones (savepoint + breathing room)
  - Other curated encounters (sniper nests, defender lines, etc.)
- Each landmark chunk includes `spawn_slot` lines for guaranteed enemy placement (prob 1.0)
- Encounter gates are the metroidvania progression walls — designed to demand specific loadouts
- Test with different seeds to verify position randomization

### Done When
- Center anchor appears at map center with per-seed rotation/mirror
- Hotspot positions vary per seed within constraints
- Landmark chunks appear at different positions per seed
- Terrain density varies around landmarks according to each landmark's configured influence type (dense, moderate, sparse, or structured)
- Transitions between terrain types are smooth and organic
- No visible hard boundaries between "dense zone" and "sparse zone"

---

## Phase 4 — Connectivity + Gates + Corridors + Obstacles

**Goal**: All landmarks guaranteed reachable. **Progression gates enforced** — gated landmarks only reachable through their designated gate. Carved corridors connect disconnected areas. Obstacle blocks add tactical structure to open terrain.

### Connectivity Validation
- Flood fill from center anchor position
- Identify landmarks not reachable from center
- This catches the main failure mode: dense terrain walls off a landmark

### Corridor Carving
- For each unreachable landmark: A* pathfind from landmark to nearest reachable cell
- Wall cost > empty cost (prefer existing passages)
- Carve 2-3 cell wide corridors along the path
- Optional: roughen corridor edges for organic feel
- Optional: use a distinct cell type for corridor walls ("data conduit" aesthetic)
- Re-flood-fill after each corridor to update reachable set

### Progression Gate Enforcement
- Parse `gate` definitions from zone file: `gate <gate_type> <gated_type> [<gated_type> ...]`
- For each gate: flood fill from center treating gate chunk cells as impassable
- If a gated landmark is still reachable → alternate path exists → seal it
- **Sealing algorithm**: A* to find the alternate path, seal at its narrowest chokepoint
- Iterate until gated landmarks are only reachable through their gate (max ~10 iterations)
- Gate landmarks should use dense influence — creates natural wall barrier, minimizes gaps to seal
- After sealing: re-validate all landmarks still reachable (sealing might disconnect something)
- If sealing disconnected a landmark: carve corridor THROUGH the gate (not around it)
- **This is the metroidvania's core enforcement** — guarantees difficulty-gated progression

### Wall Type Refinement
- Post-process pass after terrain generation assigns walls
- Influence-proximity-based circuit vs solid wall type selection
- **Near landmarks** (high influence): edge detection mode — walls with exposed faces become circuit (geometric, architectural feel)
- **In the wilds** (low/no influence): organic scatter — random chance of any wall becoming circuit (natural, raw feel)
- Smooth gradient between modes via influence falloff
- Parse `circuit_prob_landmark` and `circuit_prob_wild` from zone file
- Mirrors hand-crafted pattern from The Origin (geometric near portal/savepoint, organic in southern terrain)

### Obstacle Block Scatter
- Load obstacle blocks from `resources/blocks/`
- Each block has a `style` flag: **structured** (geometric, deliberate) or **organic** (random-looking, natural)
- **Style-matched placement**: structured blocks placed near landmarks (high influence), organic blocks placed in the wilds (low influence), blended at moderate influence
- Scatter in moderate-to-sparse terrain (within INFLUENCE_SPARSE and INFLUENCE_MODERATE zones)
- Spacing enforcement (min distance between blocks)
- Random mirroring for variety
- Parse `obstacle_density` and `obstacle_min_spacing` from zone file

### Structured Sub-Area Fill (Optional, INFLUENCE_STRUCTURED)
- Within INFLUENCE_STRUCTURED zones: identify inner area (influence strength > 0.5)
- Compute coarse grid, generate Spelunky-style solution path
- Fill with chunk templates (path cells = traversable, non-path = sealed/connected)
- Blend at edges with surrounding noise terrain
- **This is the most complex piece** — defer if pure noise + obstacles produce good enough results

### Done When
- All landmarks reachable from center anchor (verified by flood fill)
- **Progression gates enforced**: gated landmarks ONLY reachable through their gate (verified by gate-aware flood fill)
- No alternate paths bypass any gate — sealing pass closes all gaps
- Carved corridors look intentional, not ugly
- Wall type refinement produces geometric circuit edges near landmarks and organic scatter in wilds
- Obstacle blocks are style-matched: structured blocks near landmarks, organic blocks in the wilds
- Obstacle blocks add tactical interest to open areas
- (Optional) Structured sub-areas produce curated room sequences within dense zones
- Can walk from center to any landmark (through required gates)

---

## Phase 5 — Enemy Population + Tuning

**Goal**: Enemies distributed through generated content. Influence-biased enemy composition varies across the map. Generation quality tuned for gameplay.

### Enemy Population System
- Parse `enemy_budget_base`, `enemy_min_spacing`, `difficulty_min`, `difficulty_max` from zone file
- Budget-controlled spawning: total budget = open cells × density
- Fixed spawns from zone file subtracted from budget
- Random position selection with spacing enforcement

### Influence-Biased Enemy Selection
- Parse `enemy_bias` from landmark definitions
- At each spawn position, blend enemy type weights from active influences
- Stalker-heavy landmarks produce more stalkers nearby, swarmer-heavy produce more swarmers, etc.
- Default weights used in neutral areas (no strong influence)

### Density Modulation
- `enemy_density_mult` per influence adjusts local spawn density
- Dense labyrinth areas: fewer but deadlier enemies
- Open arena areas: more enemies spread across the field

### Effect Cell Gameplay Behaviors (Themed Zones)
- Generic zones: effect cells are purely visual (data traces) — no gameplay behavior needed
- Themed zones: implement per-biome effect cell behaviors:
  - Player standing on an effect cell triggers the associated effect (DOT, slow, feedback drain, etc.)
  - Effect parameters defined in zone file's `effecttype` line
  - Ship update checks current cell type and applies effect
  - Visual feedback when effect is active (screen tint, particle, etc.)
- Enemies are NOT affected by effect cells (they're security programs — they belong to the system)

### Tuning Pass
- Noise parameter tuning: find the sweet spot for base density, frequency, octave count
- **Dual threshold tuning**: wall_threshold and effect_threshold gap controls effect cell coverage
- Influence radius/strength tuning: how far should terrain character bleed from landmarks?
- Effect cell density tuning: enough to add texture, not so much they dominate the visual
- Enemy budget tuning: how many enemies per zone feels right?
- Corridor width tuning: 2 or 3 cells?
- Obstacle density tuning in sparse areas
- Difficulty gradient: harder enemies farther from center?
- **Extensive playtesting** — this is where the game feel comes together

### Debug Tools
- Seed cycling hotkey in godmode (re-roll and regenerate without reload)
- Influence field visualization overlay
- Enemy density heatmap overlay
- Connectivity graph overlay (show flood fill result)
- **Gate enforcement overlay**: highlight gate landmarks, show gated regions, visualize sealed chokepoints
- **Three-tile-type heatmap**: visualize wall/effect/empty distribution
- Generation time profiling

### Done When
- Enemies populate generated terrain within budget
- Enemy composition varies by area (influence-driven)
- Effect cells in themed zones apply gameplay effects to the player
- Data traces in generic zones render with subtle bloom glow
- Multiple seeds produce meaningfully different combat experiences
- Game feels good to play through generated content
- Debug tools enable rapid tuning iteration

---

## Phase 6 — Content Authoring + Polish

**Goal**: Efficient authoring tools for scaling up content. Multiple zones with varied biomes.

### Godmode: Chunk Editor
- Edit mode for authoring chunk templates visually
- Paint walls/empties/maybes, define exits, mark obstacle zones, place spawn slots
- Save to `.chunk` file format
- Validation feedback in-editor (exit traversability, overlap errors)

### Godmode: Procgen Debug/Tuning Panel
- Hotspot position visualization
- Influence field gradient overlay
- Connectivity graph overlay
- Seed input/cycling
- Live parameter adjustment (noise, influence, density)

### Content Ramp
- Author center anchors for all 11 zones (Origin + 3 generic + 6 themed + alien)
- Author landmark chunks per biome (boss arenas, encounter gates, portal rooms, safe zones, scripted difficulty gates)
- Author 30-50 obstacle blocks across biomes (flagged as structured or organic per block)
- Author biome-specific cell types (solid + circuit wall types + effect cell types per zone)
- **Define effect cell types per biome**: data traces for generic zones, hazard cells for themed zones
- Define zone-specific enemy compositions and influence parameters
- Tune dual thresholds per zone for appropriate three-tile-type balance
- Optional: chunk templates for INFLUENCE_STRUCTURED areas (if used)

### Multi-Zone Integration
- Verify portal connectivity across zones (zone A exit → zone B entry)
- Per-zone difficulty scaling via noise/influence/enemy parameters
- Themed enemy variants per biome (fire hunters, poison stalkers, etc.)
- **Themed effect cells per biome** (fire ember cells, ice frost cells, poison toxic cells, etc.)
- Background color and music per zone
- Verify effect cell visual palette complements background cloudscape colors per zone

### Done When
- All 11 zones have authored content and generate cleanly
- Each zone feels visually and mechanically distinct
- Generic zones have atmospheric data trace cells
- Themed zones have hazard effect cells with per-biome gameplay behavior
- Portal connectivity works across all zones
- Difficulty progression feels right from Origin through themed zones to alien zone
- Content volume is sufficient for interesting per-seed variety

---

## Dependencies

```
Phase 1 ──→ Phase 1.5? ──→ Phase 2 ──→ Phase 3 ──→ Phase 4 ──→ Phase 5
     ✅                                                              ↓
                                                                Phase 6
```

- Phase 1 is complete (godmode entity placement + zone navigation)
- Phase 1.5 is conditional — evaluate after Phase 2 whether extra authoring tools are needed
- Phase 2 is standalone (PRNG + noise + basic terrain)
- Phase 3 depends on Phase 2 (terrain gen must exist to modulate with influences)
- Phase 4 depends on Phase 3 (landmarks must exist to validate connectivity)
- Phase 5 depends on Phase 4 (terrain must be connected before populating enemies)
- Phase 6 can start after Phase 4 for tooling, but content authoring benefits most after Phase 5

---

## Open Questions (revisit per phase)

1. **Noise algorithm**: Simplex vs OpenSimplex vs Perlin? Lean simplex — fastest, best visual properties, no grid artifacts. (Phase 2)
2. **Generation performance**: 1024×1024 noise evaluation + flood fill + A* — how fast? Should be well under a second, but profile. (Phase 2-4)
3. **Chunk authoring volume**: How many landmark chunks and obstacle blocks needed before generation feels good? Start minimal (5-10 landmarks, 10-15 blocks), ramp during Phase 6. (Phase 3+)
4. **Generation timing**: Seed per save file vs per zone entry? Start with per-file (metroidvania — stable layout per playthrough). Endgame may use per-entry or rotating seeds. (Phase 3)
5. **Influence overlap behavior**: Additive vs max-wins when two influences overlap? Start additive — two dense influences create extra-dense terrain. (Phase 3)
6. **Structured sub-areas**: Are they needed, or does noise + obstacles produce good enough results? Evaluate after Phase 4 before investing in the Spelunky walk algorithm. (Phase 4)
7. **Corridor cell type**: Should carved corridors use a visually distinct cell type? Adds to fiction ("data conduits") but requires extra cell types per biome. (Phase 4)
8. **Center anchor size**: 32×32 vs 48×48 vs 64×64? Needs playtesting — too small and the start feels generic, too large and it dominates the map. (Phase 3)
9. **Debug overlay rendering**: Render influence field as a transparent color overlay in godmode? Needs to work with existing bloom pipeline without adding GPU cost during gameplay. (Phase 5)
