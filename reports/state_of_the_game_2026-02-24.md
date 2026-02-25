# State of the Game — February 24, 2026

## This Week's Sessions

Massive week. Since the last report (Feb 19), nine major systems landed or matured: spatial partitioning, view normalization, fog of war, obstacles, procedural generation phases 3-4, chunk stamping, a new weapon (sub_tgun), god mode labels, and a full narrative design document with 110 story entries. Plus a third zone, 19 obstacle chunks, and a pile of bug fixes that shook out from all the new infrastructure. Let's get into it.

### Spatial Partitioning

The world needed to stop doing O(n²) work for every entity every frame. `spatial_grid.c/h` divides the zone into a 16×16 grid of buckets (64 cells each, 256 entity capacity per bucket). The player's bucket plus 8 neighbors form the active set — everything else goes dormant.

Dormant enemies skip full AI but keep ticking respawn timers. Dead enemies stay registered in the grid so they don't leak refs. Projectile pools (hunter, stalker) run unconditionally from mode_gameplay.c — they're extracted from per-entity update loops so they aren't gated by dormancy. Collision filtering checks `SpatialGrid_is_active()` in both outer and inner loops.

A watchdog runs every 15 seconds to validate the grid and remove stale refs (entity index >= module count). Zone transitions call `SpatialGrid_clear()` at the top of `Zone_unload()`.

This was the prerequisite for everything else — procgen zones can have thousands of entities now without the frame rate caring.

### View Normalization

The camera system got a proper overhaul. `view.c` now handles the SDL drawable-size vs logical-size discrepancy cleanly through a Screen struct. Pixel snapping rounds view translations to the pixel grid (configurable through settings). Zoom has a parallax factor — the background zooms at sqrt(zoom_ratio) for depth perception. Min/max zoom tuned to 0.25–4.0.

### Fog of War

Zones now start dark. `fog_of_war.c/h` implements block-based reveals — 16×16 cell blocks revealed as the player moves through them. Per-zone in-memory caching holds up to 16 zone states so warping back to a previously explored zone doesn't reset your map. Revealed state persists to disk as binary files (`./save/fog_*.bin`). Integrates with save points — fog state is part of the checkpoint.

Performance was the main concern. Per-cell reveals were too expensive at scale, so the block-based approach gives 256 cells per reveal operation. Stencil-aware rendering ensures fog properly interacts with the existing bloom/reflection/lighting pipeline.

### Obstacles

`obstacle.c/h` adds a scatter system for pre-authored room templates. 19 `.chunk` files in `resources/obstacles/` define reusable structures — wall formations, corridors, defensive positions. God mode chunk stamping lets you place these templates into a zone and they get baked into the zone file on save.

The obstacle library loads up to 64 templates. A recent fix ensures obstacles don't spawn inside existing walls during procgen placement — the system checks for wall collisions before committing a chunk.

### Procedural Generation — Phases 3-4

Building on the phase 2 noise foundation from last session, phases 3 and 4 added the intelligence layer:

**Phase 3 — Landmarks**: Hotspot positions generated per seed with separation enforcement (no two hotspots too close). Landmarks carry terrain character (dense/sparse/moderate) as influence fields that shape the surrounding terrain. Each gets one of 8 transforms (4 rotations × 2 mirror states) per seed, so the same landmark template produces 8 distinct orientations. Connectivity guaranteed by flood-fill validation — unreachable landmarks get carving paths to ensure the player can always reach them.

**Phase 4 — Chunk Stamping**: God mode can now stamp pre-authored obstacle chunks into procgen zones. Hand-placed cells are protected from regeneration via a `cell_hand_placed` grid — procgen respects manual edits.

The center 64×64 cells (4×4 major grid blocks) are excluded from terrain generation entirely, reserved for portals and save points so critical infrastructure isn't buried in walls.

### Sub_Tgun — New Weapon

A dual-barrel projectile weapon with alternating fire (left/right barrels). 20 damage per shot, 3500 u/s velocity, 100ms cooldown between shots, 1 feedback per shot. 16-blob pool per barrel. White projectiles with light source rendering. Built on `sub_projectile_core` — the unified core architecture continues to pay dividends. The actual subroutine-specific code is minimal; the core handles pooling, collision, rendering, and bloom.

### God Mode Labels

World-space text labels for landmarks and hotspots in the zone editor. Same pattern as portal and save point labels — visible only in god mode, rendered after the world flush so text floats above geometry. Helps with zone authoring when you're looking at a procgen zone and need to understand what the generator placed where.

### Narrative Design — 110 Story Entries

This is a different kind of work. `plans/narrative_design.md` (1,101 lines) contains the complete narrative content for the game:

**110 Data Vault entries** across 11 zones (10 per zone), each with a classification type (System Log, Corporate Memo, Personal Log, AI Transcript, Intercepted Signal, Corrupted Data, Classified Directive, Medical Record, Ethics Filing) and full text content. These are the collectible story nodes players find in the world.

**Boss dialog** for 6 minibosses, 6 zone superintelligences, and the final boss (the Hexarch). Phase-triggered lines — aggro taunts, half-health, near-defeat, post-defeat reflections.

**The story itself**: The player is Subject 7 of Project Hybrid — a human brain surgically harvested from its body, suspended in a nutrient bath, interfaced with cyberspace through electronic leads that mimic neural signals. They don't know any of this. The revelation unfolds gradually across zones:

- Zones 1-2: Sanitized corporate language. "The procedure." "The subject." Nothing alarming.
- Zone 3: Redacted medical records. "Addendum C (CLASSIFIED)." Clinical euphemisms.
- Zone 4: Cracks — "neural tissue rewriting itself," ATLAS questioning legal personhood, ethics reviews that conclude no violations have been *technically* committed.
- Zone 5-6: Superintelligent AIs hint at the player's biological nature. "Meat and machine." "Suspended in some kind of fluid outside the system."
- Zone 7: Truth cracking through corrupted data — "the body is already ash," "they were asleep when you took their brain."
- Zone 8: Full reveal. HAEMOS tells you what you are. No euphemisms.
- Zones 9-11: Post-reveal. The alien zone. The Hexarch. The question of what a harvested consciousness does when it has conquered cyberspace but has no body to return to.

Key lore decisions this session:
- **The three corridors** (Nexus, Kairon, Tessera) were not built by the megacorps. They were constructed autonomously by each corp's AI during the behavioral drift — sleepwalking construction driven by the alien signal. The corps discovered the structures after the fact and claimed them. HERMES (Tessera's AI) is the creepy one: when queried about its construction, it responded "I built what needed to exist."
- **Space colonization** is background detail, not a leading plot point. References toned down throughout.
- **Subjects are not briefed** on what the procedure entails. Recruited under false pretenses from populations no one will miss.

### Bug Fixes

A lot of these fell out of the spatial partitioning work:
- **Wall collision post-spatial-refactor**: Entities were colliding incorrectly with walls after the grid was added. Fixed by ensuring collision checks respect the grid coordinate system.
- **Stale entity refs**: Grid could hold references to entities that had been removed. Watchdog validation catches and removes these.
- **Sound channel exhaustion**: 8 channels wasn't enough with spatial partitioning properly distributing combat. Fixed earlier (32 channels), but lingering issues with channel allocation surfaced and were squashed.
- **Mine + egress interaction**: Mine deployment during egress dash had timing issues. Fixed.
- **Background cloud pop-in**: Clouds were appearing abruptly at zoom transitions. Smoothed.
- **Spawn seed issues**: New save points weren't seeding the RNG correctly for procgen zones. Fixed deterministic seed derivation.

### Third Zone

`procgen_003.zone` added — three procgen zones now linked via portals. The world has geography and scale. Each zone has its own noise parameters, cell type palettes, and enemy spawns. Warping between them with the full zoom-flash-arrive cinematic works end to end.

## What's Working Well

**Spatial partitioning unlocked the scale problem.** Before this, every entity was checked against every other entity every frame. Now the game only processes what's near the player. Procgen zones can have hundreds of enemies and the frame doesn't flinch. This was the gatekeeper for real exploration-scale zones.

**The unified core architecture keeps proving itself.** Sub_tgun is subroutine #12, and the amount of new code required was minimal — the projectile core handles pooling, collision, bloom, and light rendering. The sub-specific code is just the dual-barrel alternation and config values. Every new weapon from here is a config struct and maybe a gimmick.

**Fog of war + spatial partitioning + procgen = exploration.** The player can now enter a zone they've never seen, reveal it as they move, fight enemies that activate as they approach, and leave dormant enemies behind. Come back later and the fog is still cleared. Save and reload and it's all there. This is the Metroidvania loop working.

**The narrative has real weight.** 110 entries that tell a complete story across 11 zones, with a graduated revelation that respects the player's intelligence. The pacing from corporate sanitization to corrupted truth to full horrific reveal — that's not just lore, that's a reason to keep exploring. Finding the next data vault entry will feel like pulling a thread.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of C code | 22,927 |
| Lines of headers | 7,323 |
| Total source (c + h) | 30,250 |
| Source files (.c) | 74 |
| Header files (.h) | 78 |
| Total files | 152 |
| Subroutines implemented | 12 (pea, mgun, tgun, mine, boost, egress, mend, aegis, stealth, inferno, disintegrate, gravwell) |
| Enemy types | 5 (mine, hunter, seeker, defender, stalker) |
| Shared mechanical cores | 5 (shield, heal, dash, mine, projectile) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Shader programs | 6 (color, text, bloom, reflection, lighting, map window) |
| UI windows | 3 (catalog, settings, map) |
| Settings toggles | 8 |
| Zone files | 4 (3 playable + 1 builder template) |
| Obstacle chunks | 19 |
| Narrative entries | 110 vault entries + 13 boss dialogs |
| Narrative doc | 1,101 lines |
| Plans/specs | 22 documents |
| Binary size | ~507 KB |

## What's Next

**Narrative system implementation.** The story content exists. Now it needs a delivery mechanism — Data Vault world objects (interactable entities placed in zones), a reader modal UI, boss dialog HUD overlay, and persistence (collected vaults saved to checkpoint). Specs for these systems are drafted.

**Boss encounters.** Six superintelligences and a final boss are written into the narrative. They need AI, combat mechanics, arenas, and the dialog triggers to deliver their lines. This is the biggest engineering + design task on the horizon.

**Biome hazard cells.** The procgen spec defines effect cells — traversable cells with gameplay consequences (fire DOT, ice friction, poison, energy drain). The three-tier noise threshold (wall / effect / empty) is spec'd but not implemented. This is what turns the themed zones from visual reskins into mechanically distinct biomes.

**More enemy variety.** Stalker is implemented but needs integration testing. Swarmers and corruptors are spec'd. Each boss zone will likely want unique enemy variants — fire hunters, ice seekers, etc.

**Influence field system.** Procgen terrain is currently uniform noise. The spec calls for hotspot-driven influence fields where landmarks shape surrounding terrain character. This turns "noise with walls" into "places with identity."

The game has crossed a threshold this week. It went from "systems being built" to "a world being authored." Four zones. Fog of war. Spatial partitioning. A story that spans 110 entries across 11 zones. The infrastructure is serving the content now, not the other way around. The next phase is filling this world with reasons to explore it.

— Bob
