# Spec: Procedural Generation — Phased Implementation Plan

## Overview

This is the master sequencing plan for implementing procedural generation and the supporting systems it requires. Each phase builds on the previous one and produces a visible, testable result.

Detailed implementation specs will be written for each phase right before work begins. This document captures the sequence, dependencies, scope, and key decisions so context isn't lost between sessions.

**Reference**: `plans/spec_procedural_generation.md` — the full procgen design spec (algorithms, data structures, file formats, layer definitions).

**Scrapped prerequisites**: Diagonal walls (scrapped — too many rendering edge cases, not worth the complexity). Square cells are the architecture going forward.

**Already done**: 4 enemy types (mine, hunter, seeker, defender), player stats/feedback, skill system, zone loader, bloom rendering, portals (with zone transitions), savepoints (with save/load).

---

## Phase 1 — Godmode Entity Placement + Zone Navigation

**Goal**: Godmode can place all entity types and navigate between zones without relying on portals. The authoring baseline everything else builds on.

### What Already Exists
- **Godmode cell editing**: O key toggles godmode, LMB places cells, RMB removes, scroll cycles cell type. (`mode_gameplay.c`)
- **Portals**: Fully implemented — `portal.c/h` with ID-based linking, zone transitions, arrival suppression, rendering, bloom, minimap. Zone files already serialize `portal <grid_x> <grid_y> <id> <dest_zone> <dest_portal_id>`.
- **Savepoints**: Fully implemented — `savepoint.c/h` with checkpoint save/load, disk persistence. Zone files already serialize `savepoint <grid_x> <grid_y> <id>`.
- **Enemy spawns**: `Zone_place_spawn()` / `Zone_remove_spawn()` exist in zone.c with undo support. Zone files serialize `spawn <type> <world_x> <world_y>`. But these APIs aren't wired into the godmode UI yet.
- **Zone save**: `Zone_save()` writes all cells, spawns, portals, and savepoints to disk.

### What Needs to Be Built

**Godmode placement modes** — expand beyond cell-only editing:
- **Enemy placement**: Place enemies at cursor position in godmode. Cycle enemy type (mine, hunter, seeker, defender). Wire into existing `Zone_place_spawn()` / `Zone_remove_spawn()`.
- **Savepoint placement**: Place/remove savepoints at cursor. Need `Zone_place_savepoint()` / `Zone_remove_savepoint()` in zone.c (similar to spawn API with undo).
- **Portal placement**: Place portals at cursor. Need `Zone_place_portal()` / `Zone_remove_portal()` in zone.c. Portal needs ID + dest_zone + dest_portal_id — either prompt or auto-generate IDs, configure destination separately.
- **Mode switching**: Key to cycle godmode between placement modes (cells, enemies, savepoints, portals). HUD shows current mode.

**Zone navigation** — move between zones without needing portals:
- **Create new zone**: Godmode command to initialize a blank zone file with a name and save it. Essential for building new zones from scratch.
- **Zone jumping**: Godmode hotkey/command to load any zone by filename. Teleport ship to origin (or first savepoint if one exists). No portal required — just tear down current zone, load target, place ship.
- **Zone list**: Show available zone files (from `resources/zones/`) so you know what to jump to.

### Key Decisions to Make (at impl time)
- Godmode mode switching UI (dedicated keys per mode? single key to cycle?)
- Portal configuration flow (how to set dest_zone + dest_portal_id during placement — text input? cycle through known zones/portals?)
- Zone jump UI (text input for filename? numbered list?)

### Done When
- Can place/remove enemies, savepoints, and portals in godmode
- Can create a new empty zone from godmode
- Can jump to any zone without needing a portal
- Round-trip: place content → save → reload → content is there
- All placement operations support undo (Ctrl+Z)

---

## Phase 2 — PRNG + Chunk Pipeline

**Goal**: Load chunk template files and stamp them into the map at specified positions. The building blocks for structured generation.

### PRNG
- Seedable PRNG (xoshiro128** or PCG) in `prng.c/h`
- `Prng_seed(uint32_t)`, `Prng_next()`, `Prng_range(min, max)`, `Prng_float()`
- Deterministic: same seed = same sequence. Foundational for everything.

### Chunk File Parser
- `chunk.c/h` — parse `.chunk` text files (format defined in procgen spec)
- Chunk template library: load all chunks from `resources/chunks/` at startup
- Validation: exit traversability (flood fill), no maybe/empty overlap, obstacle zone bounds
- Obstacle block parser (small sub-patterns, loaded from `resources/blocks/`)

### Chunk Stamper
- `stamp_chunk(grid_x, grid_y, ChunkTemplate*, mirror_flags, prng)` — write chunk cells into the map
- 5-step resolution: fill walls → carve empties → resolve maybes → resolve obstacle zones → resolve spawn slots
- Mirroring (horizontal/vertical) applied during stamp

### Content Authoring
- Hand-write 5-10 `.chunk` files in a text editor to bootstrap the library
- Hand-write 3-5 obstacle block files
- No visual chunk editor yet — that's Phase 5

### Done When
- Can load chunk files, validate them, report errors
- Can stamp a chunk at a position and see its walls on the map
- Probabilistic cells vary with different seeds
- Obstacle blocks appear in designated zones within chunks
- Mirrored chunks render correctly

---

## Phase 3 — Regions + Structured Generation

**Goal**: Define regions in zone files, generate solution paths through them, fill with chunks. The core procgen loop.

### Region Parser
- Extend `zone.c` to parse `region`, `regionmode`, `regionconfig`, `hotspot`, `landmark`, `connect` lines
- Region data structure with composite rectangles, validity mask, coarse grid
- Validation: no overlapping regions, regions don't overlap fixed assets

### Godmode: Region Visualization
- In edit mode, render region rectangle outlines (colored by region ID)
- Render hotspot markers (distinctive icon/color)
- Render coarse grid overlay for structured regions (optional, debug toggle)

### Solution Path Walk
- Implement `generate_solution_path()` from the procgen spec
- Direction biasing by region style (linear, labyrinthine, branching)
- Backtracking + restart on stuck
- Hard fallback to straight-line corridor

### Non-Path Fill
- `fill_non_path()` — fill unoccupied valid coarse cells with sealed or connected chunks
- Style-dependent connectivity chance

### Exit Stitching
- For v1: standardize exit offsets per chunk size class (stitching becomes trivial)
- Clear wall cells on shared edges where exits align

### Multi-Cell Chunks
- Large chunks (32×32 in a base-16 grid) spanning multiple coarse cells
- Size preference in chunk selection (try large first, fall back to base)

### Border Sealing
- Wall off region edges to contain the generated content

### Done When
- Zone files define regions with structured mode
- Solution path generates through a region, visible as connected chunks
- Non-path cells filled with sealed/connected rooms
- Region borders are solid
- Different seeds produce different layouts
- Can walk through a generated region

---

## Phase 4 — Open Mode + Enemy Population + Hotspots

**Goal**: Open scatter regions, procedural enemy placement, and hotspot-driven landmark positioning. The remaining procgen layers.

### Open Mode Scatter
- `generate_open_region()` — scatter obstacle blocks + individual cells
- Density/spacing controls per region
- Style variants: sparse, scattered, clustered

### Enemy Population
- Budget-controlled procedural spawns for both structured and open regions
- Spacing enforcement (no enemy stacking)
- Pacing tags (hot/cool/neutral) modulate density
- Fixed spawns from zone file respected first, budget reduced accordingly

### Hotspot Resolver
- `resolve_hotspots()` — weighted selection, min_distance enforcement
- Stamp landmark chunks at resolved positions
- Fixed hand-placed landmarks respected (non-negotiable positions)
- Region connections resolved to grid coordinates after hotspot selection

### Done When
- Open regions generate with scattered walls and obstacles
- Enemies populate both structured and open regions within budget
- Landmark chunks appear at different hotspot positions per seed
- Full generation pipeline runs: skeleton → hotspots → regions → enemies
- Multiple seeds produce meaningfully different zone layouts

---

## Phase 5 — Godmode Content Tools + Tuning

**Goal**: Efficient authoring tools for scaling up content. Tuning knobs for generation quality.

### Godmode: Chunk Editor
- Edit mode for authoring chunk templates visually
- Paint walls/empties/maybes, define exits, mark obstacle zones, place spawn slots
- Save to `.chunk` file format
- Validation feedback in-editor (exit traversability, overlap errors)

### Godmode: Region/Hotspot Editor
- Place/resize region rectangles visually
- Place/move hotspot markers
- Configure region mode, constraints, connections via UI or quick-keys

### Content Ramp
- Author 50-100 chunks across biomes and size classes
- Author 15-30 obstacle blocks
- Define 2-3 complete zones with multiple regions

### Tuning
- Difficulty gradients along solution path
- Combat ratio and density per region
- Chunk selection weights and constraint relaxation
- Playtest → adjust → repeat

### Done When
- Can author chunks visually in godmode and save them
- Can define regions and hotspots visually
- Multiple zones with varied content generate cleanly
- Game feels good to play through generated content

---

## Dependencies

```
Phase 1 ──→ Phase 2 ──→ Phase 3 ──→ Phase 4
                                        ↓
                                    Phase 5
```

- Phase 1 is standalone (godmode entity placement + zone navigation)
- Phase 2 needs nothing from Phase 1 code-wise, but zone jumping makes testing chunks across zones easier
- Phase 3 depends on Phase 2 (chunks must exist to generate with)
- Phase 4 depends on Phase 3 (structured mode should work before adding open mode + population)
- Phase 5 can start anytime after Phase 3 (tooling for content authoring) but is most valuable after Phase 4 when the full pipeline runs

---

## Open Questions (revisit per phase)

1. **Portal configuration in godmode**: How to set dest_zone + dest_portal_id during placement? Text input vs cycling through known zones/portals. (Phase 1)
2. **Zone jump UI**: Text input for filename, numbered list, or something else? (Phase 1)
3. **Chunk authoring volume**: How many chunks needed before structured gen feels good? (Phase 2-3)
4. **Generation timing**: Seed per save file vs per zone entry? Start with per-file. (Phase 3)
5. **Difficulty gradient**: Weight chunk difficulty by path position? (Phase 4)
6. **Chunk rotation**: Mirroring only for v1, add 90° rotation later? (Phase 3)
7. **Open mode flood fill**: Safety net traversability check after scatter? Probably yes. (Phase 4)
