# Hybrid — Product Requirements Document

## Setting & Fiction

**Hybrid** takes place in a digital world where a human brain has been interfaced with a computer system for the first time. The player character is the "Hybrid" — a fusion of human mind and AI consciousness navigating cyberspace.

- **Rebirth**: The game begins with a "rebirth" sequence — what was once a human is being reborn as a brain connected to a network, with the structure of that network projected into its consciousness. This is the player's first moment of awareness in cyberspace.
- **The Grid**: Represents the fabric of cyberspace. Movement across it shows traversal through digital space.
- **Map Cells / Walls**: Give the system topography — structural geometry the player must navigate around. The architecture of the digital system.
- **Mines**: Security intrusion detection devices. Part of the system's defenses against the Hybrid.
- **Hunters**: Active security programs that patrol, chase, and fire machine-gun bursts at the player. Orange triangles.
- **Seekers**: Predatory security programs that stalk the player, orbit at dash range, then execute high-speed charge attacks. Green needles.
- **Defenders**: Support security programs that heal wounded hunters/seekers and shield themselves with aegis when threatened. Light blue hexagons.
- **Viruses**: Planned as tools the player eventually uses *against* the system.
- **The Alien Entity**: The ultimate antagonist — a foreign alien intelligence projecting into human Earth cyberspace via a network connection. Its presence warps the final zone into something alien and unrecognizable.

## Genre

Bullet hell shooter × Metroidvania

- **Bullet hell**: Dense enemy patterns, projectile-heavy combat, reflexive gameplay
- **Metroidvania**: Interconnected world, ability-gated progression, exploration with fog of war, boss encounters

## Core Mechanics

### The Ship (Player Avatar)

The player's representation in cyberspace. A red triangle that moves with WASD, aims with mouse.

- **Movement**: WASD directional, with speed modifiers (Shift = fast, Ctrl = warp)
- **Death**: All damage funnels through Integrity. Walls and mines instantly zero Integrity via `PlayerStats_force_kill()`. Enemy projectiles reduce Integrity via `PlayerStats_damage()`. When Integrity reaches 0, the ship is destroyed. Respawns at last save point (or origin) after 3 seconds.
- **Death FX**: White diamond spark flash + explosion sound
- **On respawn**: All enemies (hunters, seekers, defenders, mines) silently reset to full health at their spawn points. This is seamless — the player doesn't see the reset happen.

#### Integrity (Health)

The ship's structural health. Starts at 100, regens at 5/sec after 2 seconds without taking damage (10/sec when Feedback is at 0 — rewards discipline). At 0, the ship is destroyed. Damaged by feedback spillover, enemy projectiles, and environmental hazards (walls, mines). All damage sources funnel through the Integrity system — there is one unified death path.

#### Feedback (Overload Meter)

Feedback accumulates from subroutine usage and represents connection strain. Decays at 15/sec after a 500ms grace period. When feedback is full (100) and a subroutine is used, the excess feedback spills over as direct Integrity damage — the damage equals the feedback the action would have added. This creates a resource management layer: sustained combat has consequences, and players must pace their ability usage.

**Feedback costs per subroutine**:

| Subroutine | Feedback Cost |
|------------|--------------|
| sub_pea | 1 per shot |
| sub_mgun | 2 per shot |
| sub_mine | 15 per mine explosion |
| sub_egress | 25 per dash |
| sub_mend | 20 per heal |
| sub_aegis | 30 per activation |
| sub_boost | None |

**Spillover example**: Feedback is at 95, sub_mine adds 15. Feedback caps at 100, the remaining 10 spills over as 10 Integrity damage.

**Damage feedback**: When spillover damage occurs, a bright red border flashes around the screen (fades over 200ms) and the samus_hurt sound plays. This gives clear visual/audio feedback that you're burning health.

**HUD**: Two horizontal bars in the top-left corner, with labels and numeric values to the left of each bar:
- **Integrity bar**: Green at full → yellow when low → red when critical
- **Feedback bar**: Cyan when low → yellow at mid → magenta when high. Flashes at 4Hz when full to warn of impending spillover damage.

### Subroutines (sub_ system)

Subroutines are abilities the Hybrid AI can execute to interact with digital space. They are the core progression mechanic.

**Architecture**:
- Each subroutine has a **type**: `projectile`, `movement`, `shield`, etc.
- Only one subroutine of each type can be **active** at a time
- 10 equippable HUD slots (keys 1-9, 0)
- Pressing a slot's key activates that subroutine and deactivates any other of the same type
- Example: sub_pea (projectile) in slot 1, sub_plasma (projectile) in slot 2 — pressing 2 deactivates pea, activates plasma. LMB now fires plasma.

**Known Subroutines**:

| Subroutine | Type | Description | Unlocked By | Status |
|------------|------|-------------|-------------|--------|
| sub_pea | projectile | Basic projectile weapon. Fires white dots toward cursor. 500ms cooldown, 1000ms TTL, up to 8 simultaneous. 50 damage per shot, 1 feedback per shot. | Default | Implemented |
| sub_mgun | projectile | Machine gun. Fires white dots toward cursor. 200ms cooldown, 1000ms TTL, up to 8 simultaneous. 20 damage per shot, 2 feedback per shot. Same DPS as sub_pea but easier to aim, burns feedback 5x faster. | 3 hunter kills | Implemented |
| sub_egress | movement | Shift-tap dash burst in WASD/facing direction. 150ms dash at 5x speed, 2s cooldown. 25 feedback per dash. | 3 seeker kills | Implemented |
| sub_boost | movement (elite) | Hold shift for unlimited speed boost. No cooldown, no feedback cost. Elite subroutine (gold border). | Elite fragment | Implemented |
| sub_mine | deployable | Deployable mine. 3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light. 15 feedback on explosion. | 5 mine kills | Implemented |
| sub_mend | healing | Instant heal. Restores 50 integrity. 10s cooldown. 20 feedback. Activated with E key. | 5 defender kills (mend fragments) | Implemented |
| sub_aegis | shield | Damage shield. Invulnerable to all damage for 10 seconds. 30s cooldown. 30 feedback. Activated with Q key. Cyan ring visual. | 5 defender kills (aegis fragments) | Implemented |

**Each enemy type has a corresponding subroutine** unlocked by defeating enough of that enemy. This creates a progression loop: encounter enemy → learn its patterns → kill it → gain its ability.

### Progression System

Enemies drop **fragments** (small colored binary glyph collectibles) when destroyed by the player (only if the associated subroutine isn't already unlocked). Fragments last 10 seconds (fade begins at 8s), attract toward the player when nearby, and are collectible for their full lifetime. Collecting fragments from a specific enemy type progresses toward unlocking the subroutine associated with that enemy. Fragment colors match their enemy type.

| Fragment Type | Color | Source | Unlocks | Threshold |
|---------------|-------|--------|---------|-----------|
| Mine | Magenta | Mine kills | sub_mine | 5 |
| Hunter | Red-orange | Hunter kills | sub_mgun | 3 |
| Seeker | Green | Seeker kills | sub_egress | 3 |
| Mend | Light blue | Defender kills (50% drop) | sub_mend | 5 |
| Aegis | Light cyan | Defender kills (50% drop) | sub_aegis | 5 |
| Elite | Gold | Special encounters | sub_boost | 1 |

- The **Catalog Window** (P key) shows progression and allows equipping — see Catalog Window section below

### Security Programs (Enemies)

All security programs share common behaviors:
- **Line of sight**: Enemies require unobstructed LOS through map cells to detect the player. Walls block vision.
- **Near-miss aggro**: Player projectiles passing within 200 units of an idle enemy will trigger aggro, even without a direct hit.
- **De-aggro on player death**: All active enemies immediately return to idle when the player dies. Their in-flight projectiles are cleared.
- **Reset on player respawn**: All enemies (except bosses) silently reset to full health at their original spawn points when the player respawns. This happens during the death timer so it's invisible to the player.
- **Hit feedback**: Player projectiles hitting an enemy produce a spark + samus_hurt sound for clear damage confirmation.

#### Mines (Intrusion Detection Devices)

Passive proximity threats. Dark squares with a blinking red dot.

| Property | Value |
|---|---|
| Behavior | Passive — arms on player proximity, explodes after 2s fuse |
| HP | N/A (destroyed by any player projectile hit) |
| Damage | Instant kill (contact during explosion) |
| Respawn | 10 seconds |
| Drops | Mine fragments (magenta) |

#### Hunters

Active ranged combatants. Orange triangles that patrol, chase, and shoot.

| Property | Value |
|---|---|
| Speed | 400 u/s (half player speed) |
| Aggro range | 1600 units (16 cells), requires LOS |
| De-aggro range | 3200 units (32 cells, 2 major grid squares) |
| Wander radius | 400 units around spawn |
| HP | 100 |
| Attack | 3-shot burst (100ms between shots), 1.5s cooldown between bursts |
| Projectile speed | 2000 u/s |
| Projectile damage | 15 per shot (45 per burst) |
| Projectile TTL | 800ms |
| Respawn | 30 seconds |
| Drops | Hunter fragments (red-orange) |

**State machine**: IDLE (random drift) → CHASING (move toward player) → SHOOTING (stop, fire burst, resume chase) → DYING (200ms flash) → DEAD (30s respawn)

**Damage to kill**: 2 sub_pea shots (50 each) or 5 sub_mgun shots (20 each) or 1 sub_mine

#### Seekers

Predatory dash-kill enemies. Green elongated diamonds (needles).

| Property | Value |
|---|---|
| Speed (stalking) | 300 u/s |
| Speed (orbit) | 500 u/s |
| Aggro range | 1000 units, requires LOS |
| De-aggro range | 3200 units |
| HP | 60 (glass cannon) |
| Orbit radius | 750 units (matches dash range) |
| Dash speed | 5000 u/s |
| Dash duration | 150ms |
| Dash damage | 80 (near-lethal) |
| Respawn | 30 seconds |
| Drops | Seeker fragments (green) |

**State machine**: IDLE (random drift) → STALKING (approach player) → ORBITING (circle at dash range) → WINDING_UP (300ms telegraph, red glow) → DASHING (150ms charge through player) → RECOVERING (500ms pause) → DYING (200ms flash) → DEAD (30s respawn)

**Damage to kill**: 2 sub_pea shots (50 each) or 3 sub_mgun shots (20 each) or 1 sub_mine

#### Defenders

Support security programs. Light blue hexagons that heal wounded hunters/seekers and shield themselves.

| Property | Value |
|---|---|
| Speed (normal) | 250 u/s |
| Speed (fleeing) | 400 u/s |
| Aggro range | 1200 units, requires LOS |
| De-aggro range | 3200 units |
| Heal range | 800 units |
| Heal amount | 50 HP per heal |
| Heal cooldown | 4 seconds |
| HP | 80 |
| Aegis (self-shield) | 10s duration, 30s cooldown, absorbs all damage |
| Respawn | 30 seconds |
| Drops | Mend OR Aegis fragments (random 50/50) |

**State machine**: IDLE (random drift) → SUPPORTING (heal allies, stay near them) → FLEEING (run from player if within 400 units) → DYING (200ms flash) → DEAD (30s respawn)

**Aegis shield**: Activated when taking damage or player gets close. All incoming damage absorbed for 10 seconds. Bright pulsing hexagon ring visual. Creates a timing puzzle — wait for shield to expire, or kill allies first.

**Heal beam**: Brief light-blue line connecting defender to healed target. Heals the most wounded alive hunter or seeker within range every 4 seconds.

**Damage to kill**: 2 sub_pea shots, 4 sub_mgun shots, or 1 sub_mine (when not shielded)

### Damage Model

| Weapon | Damage/Shot | Fire Rate | DPS |
|---|---|---|---|
| sub_pea | 50 | 2/sec (500ms) | 100 |
| sub_mgun | 20 | 5/sec (200ms) | 100 |
| Hunter burst shot | 15 | 3-shot burst, 1.5s between | ~30 avg |
| Seeker dash | 80 | ~every 4s | ~20 avg |

### World Design

**Structure**:
- Each zone is a complete map — grid size varies by zone (up to 1024×1024, 100-unit cells)
- Zones are always accessible (no hard locks) except the **final zone** which requires all normal bosses defeated
- Zones are **difficulty-gated**: without the right subroutines, areas are effectively impossible to survive
- This creates natural metroidvania progression — unlock abilities, access harder zones, unlock more abilities
- Portals connect zones — stepping on a portal unloads the current zone and loads the destination

**Zones** (12+ planned):

| Zone | Theme | Description |
|------|-------|-------------|
| Starting Zone | Neutral/tutorial | Player spawn, safe structures, introduction to mechanics |
| 10 Themed Zones | Fire, Ice, Poison, Blood, etc. | Each themed visually with custom cell types and patterns. Each contains 1 major boss + mini bosses. |
| Final Zone | Alien | Alien entity's domain — visually alien and unrecognizable. Locked until all 10 zone bosses defeated. |

**Bosses**:
- Each themed zone has 1 major boss and several mini bosses
- Final boss is the alien entity in the final zone
- Boss encounters drive major progression milestones

**Cell types per zone**: Each zone defines its own visual theme through custom cell type definitions. The engine provides global defaults (solid, circuit) but zones can override colors, patterns, and define zone-specific types (fire walls, ice barriers, poison pools, etc.).

### Procedural Level Generation

Zones use a **hybrid approach** to level generation: the designer hand-authors a zone's identity (landmark rooms, anchor walls, ability gates), and the algorithm procedurally generates the connective terrain between those landmarks (corridors, combat rooms, open battlefields, enemy placements). Every generation run is **seed-deterministic** — same zone + same seed = identical world. This is foundational for multiplayer, shared seeds, and reproducible runs.

**Detailed spec**: `plans/spec_procedural_generation.md`

**Key concepts**:

- **Hotspot system**: The designer places generic candidate positions (hotspots) throughout a zone. The generator assigns landmark types (boss arena, portal, safe zone) to hotspots per seed. Players know a zone *has* a boss — they have to explore to find it. Prevents memorizable layouts. Landmarks can also be hand-placed at fixed positions when explicit control is needed.

- **Two generation modes** per region:
  - **Structured mode**: Chunk-based coarse grid with solution path guarantee (inspired by Spelunky). Variable chunk sizes (8×8 to 64×64) mix within a single region. For corridors, labyrinths, and dense navigational areas.
  - **Open mode**: Scatter-based placement of walls, obstacle blocks, and enemies across open space. For expansive battlefields, arenas, and sparse cyberspace. No chunks or solution path — traversability is trivially guaranteed by low density.

- **Chunk templates**: Reusable room/corridor building blocks authored by the designer. Each chunk has exit configurations (which sides have openings), probabilistic cells, obstacle zones, and enemy spawn slots. One authored template produces thousands of variations through probabilistic resolution and mirroring.

- **Regions**: Rectangular areas of the zone designated for procedural generation. Multiple rectangles with the same ID union into composite shapes (L-shapes, T-shapes). Each region specifies generation mode, style (linear/labyrinthine/branching for structured; sparse/scattered/clustered for open), difficulty, density, and pacing.

- **Solution path first**: In structured mode, the algorithm builds a guaranteed traversable path from entry to exit before filling around it. Reachability by construction, not validation.

- **Content investment**: ~50-100 chunk templates + ~15-30 obstacle blocks across all biomes. Each template multiplies through procedural assembly, probabilistic variation, and mirroring. A fraction of hand-coding every room across 12+ zones at 1024×1024 scale.

**Prerequisites** (implement before procgen):
1. ~~God mode editing tools for procgen content authoring~~ (done)
2. ~~Portals and zone transitions~~ (done)
3. Diagonal walls (new cell types)
4. ~~At least one active enemy type~~ (done — hunters)

### Map & Navigation

**Minimap** (the HUD square at bottom-right):
- Shows nearby wall/map cell layout
- **Fog of war**: tracks where player has and hasn't explored
- Updates as player moves through the world

**Full Map** (planned, keypress to open):
- Larger detailed view, Super Metroid style
- Shows explored areas, fog of war boundaries
- More detail than minimap

### HUD

- **Stat Bars** (top-left): Integrity and Feedback bars with labels and numeric values
- **Skill Bar** (bottom): 10 slots, shared between gameplay and god mode (see Unified Skill Bar below)
- **Minimap** (bottom-right): 200×200 radar showing nearby geometry with fog of war

### Unified Skill Bar

The 10-slot skill bar at the bottom of the screen is the central interaction mechanism for both gameplay and god mode. It maintains two independent loadouts that swap when toggling modes:

- **Gameplay loadout**: Equipped subroutines. Activate a slot (click-release or number key) then use with the input appropriate to its type. Click activates on mouse-up to prevent accidental firing.
- **God mode loadout**: Equipped placeables. Activate a slot (click or number key) then LMB to place in the world.
- **Slot rearranging**: Drag skills between slots via the Catalog window (P). Drag-to-swap only works while the catalog is open — the skill bar is locked during gameplay.

**Skill activation by type** (gameplay mode):

| Skill Type | Activation Input |
|------------|-----------------|
| Projectile | LMB (fires toward cursor) |
| Movement | Shift (triggers dash/burst) |
| Shield | Slot key (toggles on/off) |
| Healing | Slot key (triggers effect) |
| Deployable | LMB (places at position) |

**Placeable activation** (god mode): Activate slot, LMB to place at cursor with grid snapping.

Only one subroutine of each type can be active at a time. Activating a sub deactivates any other of the same type. This type-exclusivity system is inspired by Guild Wars (ArenaNet), where limited skill slots and type restrictions make loadout choices meaningful.

### Catalog Window (P key)

A modal window for browsing and equipping skills or placeables, context-sensitive to the current mode.

**Layout**:
- **Left side**: Vertical tabs filtering by category
  - Gameplay mode tabs: Projectile, Movement, Shield, Healing, Deployable (more to come)
  - God mode tabs: Map Cells, Enemies, Portals, Decorations (more to come)
- **Right side**: Vertical scrollable list of items matching the active tab
  - Each entry: square icon (sized to match a skill bar slot) on the left, name + description + metadata on the right
  - Progression info shown for skills (e.g., "73/100 mines killed")
  - Obfuscated entries for undiscovered skills

**Equipping**: Drag an item from the catalog list and drop it onto a skill bar slot. The slot now holds that item.

**Future**: In gameplay mode, skill changes will be location-restricted — the catalog can only be opened at safe structures (spawn point, save stations). God mode has no such restriction.

### God Mode

God mode is the in-game level editor. Toggle with **G** key. God mode and gameplay mode are mutually exclusive — skills cannot be used in god mode, and placeables cannot be used in gameplay.

**When entering god mode**:
- Skill bar swaps to the god mode placeable loadout
- Camera becomes free (unrestricted pan/zoom)
- Ship collision and death disabled
- Catalog window (P) switches to showing placeables

**When exiting god mode** (press G again):
- Skill bar swaps back to the gameplay skill loadout
- Normal gameplay resumes immediately with existing skill configuration
- Camera returns to following the ship

**Editing workflow**:
1. Open catalog (P), browse placeable tabs (Map Cells, Enemies, Portals, etc.)
2. Drag a placeable onto a skill bar slot
3. Activate the slot (click or number key)
4. LMB to place at cursor position (grid-snapped for map cells)
5. Each edit persists immediately to the zone file

**Grid snapping**: Map cells snap to the 100-unit cell grid. Other placeables (enemy spawn points, portals) can be placed freely or snapped depending on type.

**Undo**: Stepwise undo (Ctrl+Z) reverts edits one at a time, updating the zone file with each undo.

**Placeable types** (initial):

| Type | Tab | Description |
|------|-----|-------------|
| Solid cell | Map Cells | Purple solid wall — blocks movement |
| Circuit cell | Map Cells | Blue circuit pattern wall — blocks movement |
| Mine spawn | Enemies | Mine spawn point marker |
| Portal | Portals | Zone transition point (future) |

More types will be added as new cell types, enemy types, and world features are implemented.

## Current Implementation State

### Working
- Ship movement (WASD + speed modifiers)
- Unified death system (all damage → Integrity → death check in one place)
- Enemy reset on player respawn (all enemies silently return to spawn at full health)
- Sub_pea projectile system (pooled, 8 max, 500ms cooldown, swept collision, 50 damage)
- Sub_mgun machine gun (pooled, 8 max, 200ms cooldown, swept collision, 20 damage, 2 feedback/shot)
- Sub_mine deployable mine (3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light)
- Sub_boost elite movement (hold shift for unlimited speed boost, no cooldown)
- Sub_egress dash burst (shift-tap, 150ms at 5x speed, 2s cooldown)
- Sub_mend instant heal (E key, 50 integrity, 10s cooldown, 20 feedback)
- Sub_aegis damage shield (Q key, 10s invulnerability, 30s cooldown, 30 feedback, cyan ring visual)
- Player stats system (Integrity + Feedback bars, spillover damage, regen with 2x rate at 0 feedback, damage flash + sound, shield state)
- Hunter enemy (patrol, chase, 3-shot burst, LOS requirement, near-miss aggro, deaggro on player death)
- Seeker enemy (stalk, orbit, telegraph, dash-charge, 60HP glass cannon)
- Defender enemy (support healer, heals hunters/seekers, aegis self-shield, flees player, random mend/aegis fragment drops)
- Mine state machine (idle → armed → exploding → destroyed → respawn)
- Fragment drops and collection (typed colored binary glyphs, attract to player, 10s lifetime)
- Subroutine progression/unlock system (fragment counting, per-enemy thresholds, discovery + unlock notifications)
- Zone data file format and loader (line-based .zone files with cell types, placements, spawns, portals, save points)
- Zone persistent editing and undo system (Ctrl+Z, auto-save on edit)
- Portals and zone transitions (warp pull → acceleration → flash → arrive cinematic)
- Save point system (checkpoint persistence, cross-zone respawn)
- God mode (G toggle — free camera, cell placement/removal, Tab to cycle types)
- Skill bar (10 slots, number key activation, type exclusivity, geometric icons, cooldown pie sweep)
- Catalog window (P key — tabbed sub browser, drag-and-drop equipping, slot swapping, right-click unequip, notification dots)
- Map cell rendering with zoom-scaled outlines
- Grid rendering
- View/camera with zoom and pixel-snapped translation
- Main menu (New / Load / Exit)
- HUD skill bar and minimap (live map cell display + player blip)
- Cursor (red dot + white crosshair)
- 7 gameplay music tracks (random selection) + menu music
- OpenGL 3.3 Core Profile rendering pipeline
- FBO post-process bloom (foreground entity glow + background diffuse clouds)
- Motion trails (ship boost ghost triangles, projectile thick line trails)
- Background parallax cloud system (3 layers, tiled, pulsing, drifting)
- Background zoom parallax (half-rate zoom for depth perception)
- Vertex reuse rendering (flush_keep/redraw for tiled geometry)
- Rebirth sequence (zoom-in cinematic on game start / zone load)

### Not Yet Implemented
- Unified skill bar Phase 3 (two-loadout system for gameplay/god mode)
- God mode placeable catalog (cell types, enemy spawns, portals via catalog drag-and-drop)
- Boss encounters
- Diagonal walls (new cell types with angled geometry + collision)
- Procedural level generation (hybrid approach — see spec at `plans/spec_procedural_generation.md`)
- Minimap fog of war
- Full map view
- Intro mode
- Virus mechanics (player tools against the system)
- Spatial partitioning for collision (grid buckets — see Technical Architecture)
- Zone/area design beyond current test layout
- Custom key mapping

## Audio

**Sound Effects**:

| Sound | Used For |
|-------|----------|
| statue_rise.wav | Ship respawn, aegis activation (player + defender) |
| bomb_explode.wav | Ship death, mine explosion, hunter/seeker/defender death |
| long_beam.wav | Sub_pea fire, sub_mgun fire, hunter shots |
| ricochet.wav | Projectile wall hit (pea, mgun), shielded defender hit |
| bomb_set.wav | Mine armed |
| door.wav | Mine/hunter/seeker/defender respawn, aegis deactivation |
| samus_die.wav | Ship death |
| samus_hurt.wav | Integrity damage (spillover, enemy hits, hit feedback) |
| refill_start.wav | Defender heal beam |
| samus_pickup.wav | Fragment collection |
| samus_pickup2.wav | Subroutine unlock |

**Music**: 7 deadmau5 tracks for gameplay (random), 1 for menu

## Technical Architecture

- **Language**: C99
- **Graphics**: SDL2 + OpenGL 3.3 Core Profile
- **Audio**: SDL2_mixer
- **Text**: stb_truetype bitmap font rendering
- **Pattern**: Custom ECS (Entity Component System)
- **UI**: Custom immediate-mode GUI (imgui)

### Abstraction Layers

The codebase is structured with platform abstraction in mind, enabling future ports to other rendering backends and platforms.

**Rendering abstraction** (`render.h` / `render.c`):
- All game entities call `Render_*` functions — never OpenGL directly
- Entity files (ship.c, mine.c, sub_pea.c, map.c) have zero GL includes
- The `Render_*` API provides: `Render_point`, `Render_triangle`, `Render_quad`, `Render_line_segment`, `Render_thick_line`, `Render_filled_circle`, `Render_quad_absolute`, `Render_flush`, `Render_flush_keep`, `Render_redraw`, `Render_clear`
- Swapping OpenGL for Vulkan/Metal/WebGPU requires only reimplementing render.c, batch.c, shader.c, and bloom.c — entity code is untouched

**Windowing and input abstraction**:
- `Input` struct (`input.h`) abstracts all player input into a platform-neutral struct (mouse position, buttons, keyboard state)
- SDL2 event polling translates platform events into the `Input` struct in the main loop
- All game systems receive `const Input*` — they never call SDL directly
- `Screen` struct (`graphics.h`) abstracts window dimensions
- Porting to another windowing system (GLFW, native platform) requires only changing the SDL calls in `graphics.c` and the main event loop

### Entity Component System (ECS)

Custom ECS with pointer-based components and static singleton pattern. See `spec_000_ecs_refactor.md` for detailed audit and refactoring roadmap.

**Entity** (`entity.h`): A container holding optional pointers to components and a void* state:
```c
typedef struct {
    bool empty, disabled;
    void *state;                        // Entity-specific data (MineState, ShipState, etc.)
    PlaceableComponent *placeable;      // Position + heading (per-instance)
    RenderableComponent *renderable;    // Render function pointer (shared singleton)
    CollidableComponent *collidable;    // Bounding box + collide/resolve functions (shared singleton)
    DynamicsComponent *dynamics;        // Physics (unused, reserved)
    UserUpdatableComponent *userUpdatable;  // Input-driven update (ship only)
    AIUpdatableComponent *aiUpdatable;      // Autonomous update (mines, etc.)
} Entity;
```

**Component types**:

| Component | Data | Function Pointers | Purpose |
|-----------|------|-------------------|---------|
| PlaceableComponent | position, heading | — | Spatial transform (per-instance) |
| RenderableComponent | — | render(state, placeable) | Draw the entity |
| CollidableComponent | boundingBox, collidesWithOthers | collide(state, placeable, bbox), resolve(state, collision) | Collision detection and response |
| UserUpdatableComponent | — | update(input, ticks, placeable) | Player input handling |
| AIUpdatableComponent | — | update(state, placeable, ticks) | Autonomous behavior |
| DynamicsComponent | mass | — | Physics (reserved, unused) |

**Static singleton pattern**: Components that contain only function pointers (Renderable, Updatable) are declared as `static` singletons shared by all instances of an entity type. Per-instance data lives in separate arrays (state[], placeables[]).

**System loops** (`entity.c`): Iterate all non-empty entities and call component functions:
- `Entity_render_system()` — calls renderable->render for each entity
- `Entity_user_update_system(input, ticks)` — calls userUpdatable->update
- `Entity_ai_update_system(ticks)` — calls aiUpdatable->update
- `Entity_collision_system()` — O(n^2) pairwise collision, queues resolve commands

**Creating a new entity type** (convention):
1. Define a state struct (e.g., `MineState`) with entity-specific data
2. Declare static singleton components for shared behavior (renderable, collidable, updatable)
3. Declare static arrays for per-instance data (states[], placeables[])
4. Write a factory function that calls `Entity_initialize_entity()`, wires up all component pointers, and calls `Entity_add_entity()`
5. Implement component callback functions (render, collide, resolve, update)

**Entity pool**: Fixed array of 1024 entities. Slots are reused when marked empty.

### Immediate-Mode GUI (imgui)

Custom lightweight immediate-mode UI system (`imgui.h` / `imgui.c`). Currently provides button widgets used in the main menu.

**Pattern**: Each frame, UI state is recomputed from input. No persistent widget tree — the caller owns the state.

**ButtonState** struct:
```c
typedef struct {
    Position position;
    int width, height;
    bool hover, active, disabled;
    char* text;
} ButtonState;
```

**Usage**: Call `imgui_update_button(input, &state, on_click)` each frame. Returns updated state with hover/active resolved. Fires `on_click` callback on mouse release within bounds.

**Future**: The catalog window, skill bar interaction, drag-and-drop, and god mode UI will all extend this imgui pattern. Widgets needed: scrollable lists, tabs, draggable icons, tooltips.

### Rendering Pipeline

**Batch renderer** (`batch.h` / `batch.c`):
- Accumulates vertices in CPU-side arrays (65,536 max per primitive type)
- Three primitive batches: triangles, lines, points
- `Batch_flush` uploads to VBO via `glBufferData` and draws with `glDrawArrays`
- Flush order: lines → triangles → points (opaque fills cover grid lines, points on top)

**Shader programs** (`shader.h` / `shader.c`):
- Two programs: color shader (points/lines/triangles) and text shader (textured quads)
- GLSL 330 core, embedded as string literals in source
- Uniforms: projection matrix, view matrix

**Render passes per frame**:
1. **Background bloom pass**: Render background blobs into bg_bloom FBO → gaussian blur → additive composite
2. **World pass**: Grid, map cells, entities (ship, mines, projectiles) with world projection + view transform
3. **Foreground bloom pass**: Re-render emissive elements into bloom FBO → gaussian blur → additive composite
4. **UI pass**: HUD, minimap, skill bar, text with UI projection + identity view

**Vertex reuse** (`Render_flush_keep` / `Render_redraw`):
- For tiled geometry (background clouds), vertices are pushed once per tile pattern
- `Render_flush_keep` uploads and draws but preserves the vertex data in the VBO
- `Render_redraw` sets new uniforms (offset view matrix) and redraws without re-uploading
- `Render_clear` resets batch counts after all tiles are drawn
- This makes vertex count O(blobs_per_layer) instead of O(blobs × tiles) — the 65k vertex cap is no longer a constraint regardless of zoom level

### FBO Post-Process Bloom

Two-instance bloom system replacing the old geometry-based glow (which hit vertex budget limits with 10 concentric shapes per object).

**Architecture** (`bloom.h` / `bloom.c`):
- 3 FBOs per instance (source, ping, pong) with `GL_RGB16F` textures
- Configurable `divisor` (FBO resolution = drawable / divisor), `intensity`, `blur_passes`
- Embedded GLSL 330 core shaders: fullscreen vertex, 9-tap separable gaussian blur, additive composite
- Fullscreen quad VAO/VBO for post-process passes

**Two bloom instances** (initialized in `graphics.c`):

| Instance | Purpose | Divisor | Intensity | Blur Passes |
|----------|---------|---------|-----------|-------------|
| bloom (foreground) | Neon halos on entities | 2 (half-res) | 2.0 | 5 |
| bg_bloom (background) | Diffuse ethereal clouds | 8 (eighth-res) | 1.5 | 10 |

**Bloom sources**: Map cells, ship, ship death spark, sub_pea/sub_mgun projectiles + sparks, sub_aegis shield ring, mine blink/explosion, hunter body + projectiles, seeker body + dash trail, defender body + aegis ring + heal beams, portals, save points, fragments. Each entity type provides a `*_render_bloom_source()` function that re-renders emissive elements into the FBO.

**Key design decision**: Background renders ONLY through the bg_bloom FBO (no raw polygon render). Additive bloom on top of sharp polygons doesn't hide edges — rendering exclusively through blur produces the desired diffuse cloud effect.

**Bloom API**:
- `Bloom_begin_source(bloom)` — bind source FBO, set reduced viewport, clear
- `Bloom_end_source(bloom, draw_w, draw_h)` — unbind, restore viewport
- `Bloom_blur(bloom)` — ping-pong gaussian blur passes
- `Bloom_composite(bloom, draw_w, draw_h)` — fullscreen quad with additive blend, restore viewport and blend mode

### Motion Trails

**Ship boost trail** (shift key): 20 ghost triangles stretched back along movement vector. Trail length = 4x frame delta. Ghost alpha fades from 0.4 to 0 along the trail. Rendered in both normal and bloom source passes.

**Sub_pea trail**: Thick line (3px) from previous position to current position at 0.6 alpha. Rendered in both normal and bloom source passes.

### Background System

**Tiled parallax clouds** (`background.c`):
- 3 layers with different tile sizes (14k, 17k, 21k) so repeats never align
- Each layer has parallax movement (0.05, 0.15, 0.30) relative to camera
- Blobs are irregular 12-segment polygons with per-vertex radius variation, smoothed
- Slow drift, sinusoidal pulse animation on radius
- Purple hue palette (4 colors)

**Zoom parallax**: Background zooms at half the rate of the foreground (in log space), anchored at default gameplay zoom (0.5). Formula: `bg_scale = 0.5 * pow(view_scale / 0.5, 0.5)`. Creates depth perception — background feels "further away" when zooming.

**Ambient drift**: Slow sinusoidal wander using incommensurate frequencies, independent of player movement. The background gently moves even when the player is still.

**Animation speed**: Background animation runs at 3x real time (pulse, drift, wander) for a dynamic "breathing" feel.

### Spatial Partitioning (Planned)

As enemy counts grow, brute-force collision checks (every projectile × every enemy per frame) will become a bottleneck. A **spatial hash grid** will be used to reduce this to near-constant-time lookups.

**Design**:
- A coarse 2D grid overlaying the world, with cells larger than any single entity (e.g., 500×500 units → ~26×26 grid for the current 12,800 unit world)
- Each frame: clear the grid, insert all active collidable entities (mines, future enemies) into the cell(s) they overlap
- Projectile collision queries walk only the cells the projectile's swept line passes through, testing against entities in those cells
- Ship-to-enemy collision queries check only the cell(s) the ship's bounding box overlaps

**Implementation Plan**:
- New files: `spatial.h` / `spatial.c`
- `Spatial_clear()` — reset all buckets each frame
- `Spatial_insert(entity_id, Position, Rectangle boundingBox)` — add entity to overlapping cell(s)
- `Spatial_query_line(x0, y0, x1, y1)` — return list of entity IDs in cells the line crosses
- `Spatial_query_rect(Rectangle)` — return list of entity IDs in cells the rect overlaps
- Each cell holds a small static array of entity pointers (e.g., 32 max per cell); overflow is ignored or asserted
- Called from `mode_gameplay.c` update loop: clear → insert all enemies → collision systems use spatial queries instead of brute-force iteration
- Grid cell size is tunable; 500 units is a starting point (5× the map cell size, large enough that most entities fit in one cell)

**Why grid buckets over quadtree**:
- The world is already grid-based; a spatial grid maps naturally
- ~50 lines of C with static arrays — no dynamic allocation, no tree rebalancing
- Cache-friendly flat memory layout
- Uniform distribution expected (enemies spread across zones, not heavily clustered)
- If density becomes highly non-uniform (e.g., boss arenas with swarms), quadtree can be swapped in behind the same query interface later

**When to implement**: Before adding the second enemy type or whenever mine count exceeds ~100

### Zone Data Files

Zones are fully data-driven — each zone is defined by a single `.zone` file. Zone files are the primary output of God Mode editing. Two zones currently exist: zone_001 (The Origin) and zone_002, connected by portals.

**Design**:
- One file per zone, 1:1 mapping (e.g., `resources/zones/zone_001.zone`)
- A zone file contains all data needed to fully load that zone
- A zone loader reads the file and populates the map grid, spawns entities, and registers portals
- The current hardcoded zone becomes the first zone file, serving as the reference format
- Map size can vary per zone (stored in the file, not hardcoded)

**File format** (line-based text, one entry per line, `#` comments):

```
# Zone metadata
name Starting Zone
size 128

# Cell type definitions (global defaults + zone overrides)
# celltype <id> <primary_r> <primary_g> <primary_b> <primary_a> <outline_r> <outline_g> <outline_b> <outline_a> <pattern>
celltype solid 20 0 20 255 128 0 128 255 none
celltype circuit 10 20 20 255 64 128 128 255 circuit

# Cell placements (grid coordinates, 0-indexed)
# cell <grid_x> <grid_y> <type_id>
cell 65 65 solid
cell 65 66 solid
cell 72 73 circuit

# Enemy spawn points (world coordinates)
# spawn <enemy_type> <world_x> <world_y>
spawn mine 1600.0 1600.0
spawn mine -1600.0 1600.0
spawn hunter 800.0 800.0

# Portals (grid coordinates + destination)
# portal <grid_x> <grid_y> <dest_zone> <dest_x> <dest_y>
portal 10 64 zone_002 64 64
```

**Cell type registry**: The engine provides built-in global cell types (solid, circuit) with default colors and patterns. Zone files can override these defaults or define entirely new zone-specific types. This enables themed zones (fire, ice, poison, blood) with custom visuals without engine changes.

**Grid coordinates**: Cell positions use 0-indexed grid coordinates (0 to size-1) mapped directly to the map array. The zone loader handles conversion to/from world coordinates. No centered coordinate quirks.

**Persistent editing**:
- Every God Mode edit (place, remove, modify) rewrites the zone file from in-memory state
- The zone file is the single source of truth — the in-memory world state is always a reflection of the file
- Stepwise undo modifies in-memory state and rewrites the zone file
- Undo history is maintained per editing session (not persisted across sessions)
- For expected data volumes (hundreds of cells, dozens of spawns), full file rewrites are sub-millisecond

**Benefits**:
- Zones can be authored, tested, and iterated independently
- Adding a new zone requires no code changes — just a new data file
- Zone transitions (portals) become simple: unload current zone, load target zone file
- God Mode is the zone editor — no external tooling needed
- Toggle G to instantly playtest what you just built
- Text format is human-readable, diffable, and version-control friendly

**Implementation Plan**:
- New files: `zone.h` / `zone.c` — zone loading, unloading, editing, and persistence
- `Zone_load(const char *path)` — parse file, populate map, spawn enemies, register portals
- `Zone_unload()` — clear map, destroy zone entities, reset state
- `Zone_save(const char *path)` — write current in-memory state to zone file
- `Zone_place_cell(grid_x, grid_y, type_id)` — place a cell, save
- `Zone_remove_cell(grid_x, grid_y)` — remove a cell, save
- `Zone_place_spawn(enemy_type, world_x, world_y)` — add spawn point, save
- `Zone_remove_spawn(index)` — remove spawn point, save
- `Zone_undo()` — revert last edit, save
- `mode_gameplay.c` initialization calls `Zone_load()` instead of inline spawning code
- Zone directory: `resources/zones/`
- `map.c` needs a public `Map_set_cell()` function and `Map_clear()` for the zone loader to use
