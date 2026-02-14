# Hybrid — Product Requirements Document

## Setting & Fiction

**Hybrid** takes place in a digital world where a human brain has been interfaced with a computer system for the first time. The player character is the "Hybrid" — a fusion of human mind and AI consciousness navigating cyberspace.

- **The Grid**: Represents the fabric of cyberspace. Movement across it shows traversal through digital space.
- **Map Cells / Walls**: Give the system topography — structural geometry the player must navigate around. The architecture of the digital system.
- **Mines**: Security intrusion detection devices. Part of the system's defenses against the Hybrid.
- **Future Enemies**: Security programs that actively hunt and try to kill the player.
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
- **Death**: Destroyed on contact with solid threats (mines, walls), or when Integrity reaches 0. Respawns at origin after 3 seconds.
- **Death FX**: White diamond spark flash + explosion sound

#### Integrity (Health)

The ship's structural health. Starts at 100, regens at 5/sec after 2 seconds without taking damage. At 0, the ship is destroyed (same death sequence as wall collision). Damaged by feedback spillover and future enemy attacks.

#### Feedback (Overload Meter)

Feedback accumulates from subroutine usage and represents connection strain. Decays at 15/sec after a 500ms grace period. When feedback is full (100) and a subroutine is used, the excess feedback spills over as direct Integrity damage — the damage equals the feedback the action would have added. This creates a resource management layer: sustained combat has consequences, and players must pace their ability usage.

**Feedback costs per subroutine**:

| Subroutine | Feedback Cost |
|------------|--------------|
| sub_pea | 1 per shot |
| sub_mine | 15 per mine placed |
| sub_egress | 25 per dash |
| sub_boost | None |

**Spillover example**: Feedback is at 95, sub_mine adds 15. Feedback caps at 100, the remaining 10 spills over as 10 Integrity damage.

**Damage feedback**: When spillover damage occurs, a bright red border flashes around the screen (fades over 200ms) and the samus_hurt sound plays. This gives clear visual/audio feedback that you're burning health.

**HUD**: Two horizontal bars in the top-left corner, with labels and numeric values to the left of each bar:
- **Integrity bar**: Green at full → yellow when low → red when critical
- **Feedback bar**: Cyan when low → yellow at mid → magenta when high

### Subroutines (sub_ system)

Subroutines are abilities the Hybrid AI can execute to interact with digital space. They are the core progression mechanic.

**Architecture**:
- Each subroutine has a **type**: `projectile`, `movement`, `shield`, etc.
- Only one subroutine of each type can be **active** at a time
- 10 equippable HUD slots (keys 1-9, 0)
- Pressing a slot's key activates that subroutine and deactivates any other of the same type
- Example: sub_pea (projectile) in slot 1, sub_plasma (projectile) in slot 2 — pressing 2 deactivates pea, activates plasma. LMB now fires plasma.

**Known Subroutines**:

| Subroutine | Type | Description | Status |
|------------|------|-------------|--------|
| sub_pea | projectile | Basic projectile weapon. Fires white dots toward cursor. 500ms cooldown, 1000ms TTL, up to 8 simultaneous. 1 feedback per shot. | Implemented |
| sub_egress | movement | Shift-tap dash burst in WASD/facing direction. 150ms dash at 5x speed, 2s cooldown. 25 feedback per dash. | Implemented |
| sub_boost | movement (elite) | Hold shift for unlimited speed boost. No cooldown, no feedback cost. Elite subroutine (gold border). | Implemented |
| sub_mine | deployable | Deployable mine. 3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light. 15 feedback per mine placed. Unlocked by collecting 5 mine fragments. | Implemented |

**Many more subroutines planned** — each enemy type will have a corresponding subroutine unlocked by defeating enough of that enemy.

### Progression System

Enemies drop **fragments** (small colored binary glyph collectibles) when destroyed. Fragments last 10 seconds (fade begins at 8s), and are collectible for their full lifetime. Collecting fragments from a specific enemy type progresses the player toward unlocking the subroutine associated with that enemy. Fragments are different colors based on their type and **elite** fragments are golden.

- Kill 5 mines → unlock sub_mine (0% to 100% progression)
- Each enemy type has its own subroutine and kill-count threshold
- The **Catalog Window** (P key) shows progression and allows equipping — see Catalog Window section below

### World Design

**Structure**:
- Each zone is a complete map — grid size varies by zone (default 128×128, 100-unit cells)
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
- Ship-wall collision (death + spark + sound)
- Ship-mine collision (triggers mine arming → explosion)
- Sub_pea projectile system (pooled, 8 max, 500ms cooldown, swept collision, gated by skill bar activation)
- Sub_pea wall collision (spark effect at intersection point)
- Sub_pea mine collision (direct hit on mine body causes immediate explosion)
- Sub_mine deployable mine (3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light)
- Sub_boost elite movement (hold shift for unlimited speed boost, no cooldown)
- Sub_egress dash burst (shift-tap, 150ms at 5x speed, 2s cooldown)
- Player stats system (Integrity + Feedback bars, spillover damage, regen, damage flash + sound)
- Mine state machine (idle → armed → exploding → destroyed → respawn)
- Mine idle blink (100ms red flash at 1Hz, randomized per mine)
- Fragment drops and collection (magenta binary glyphs, attract to player when nearby, 10s lifetime with 2s fade starting at 8s)
- Subroutine progression/unlock system (fragment counting, thresholds, unlock notifications)
- Zone data file format and loader (line-based .zone files with cell types, placements, spawns)
- Zone persistent editing and undo system (Ctrl+Z, auto-save on edit)
- God mode (G toggle — free camera, cell placement/removal, Tab to cycle types)
- Skill bar Phase 1 (10 slots, number key activation, type exclusivity, geometric icons, cooldown pie sweep)
- Catalog window Phase 2 (P key — tabbed sub browser, drag-and-drop equipping, slot swapping, right-click unequip)
- Map cell rendering with zoom-scaled outlines
- Grid rendering
- View/camera with zoom and pixel-snapped translation
- Main menu (New / Load / Exit)
- HUD skill bar and minimap (live map cell display + player blip)
- Cursor (red dot + white crosshair)
- 7 gameplay music tracks (random selection) + menu music
- OpenGL 3.3 Core Profile rendering pipeline
- FBO post-process bloom (foreground entity glow + background diffuse clouds)
- Motion trails (ship boost ghost triangles, sub_pea thick line trail)
- Background parallax cloud system (3 layers, tiled, pulsing, drifting)
- Background zoom parallax (half-rate zoom for depth perception)
- Vertex reuse rendering (flush_keep/redraw for tiled geometry)
- Rebirth sequence (death → zoom out → slow crawl → zoom in → respawn)

### Not Yet Implemented
- Unified skill bar Phase 3 (two-loadout system for gameplay/god mode)
- God mode placeable catalog (cell types, enemy spawns, portals via catalog drag-and-drop)
- Active security programs (hunting enemies)
- Boss encounters
- Minimap fog of war
- Full map view
- Save/Load system
- Intro mode
- Virus mechanics (player tools against the system)
- Spatial partitioning for collision (grid buckets — see Technical Architecture)
- Zone/area design beyond current test layout
- Custom key mapping

## Audio

**Sound Effects**:

| Sound | Used For |
|-------|----------|
| statue_rise.wav | Ship respawn |
| bomb_explode.wav | Ship death, mine explosion |
| long_beam.wav | Sub_pea fire |
| ricochet.wav | Sub_pea wall hit |
| bomb_set.wav | Mine armed |
| door.wav | Mine respawn/recycle |
| samus_die.wav | Ship death |
| samus_hurt.wav | Feedback spillover damage |
| samus_pickup.wav | (reserved) |

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

**Bloom sources**: Map cells, ship, ship death spark, sub_pea projectiles + sparks, mine blink/explosion. Each entity type provides a `*_render_bloom_source()` function that re-renders emissive elements into the FBO.

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

The current zone is hand-built in code (`mode_gameplay.c` spawns 32 mines at hardcoded positions, `map.c` places wall cells via `set_map_cell` calls). This will be replaced with a data-driven zone system where each zone is defined by a single file. Zone files are the primary output of God Mode editing.

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
