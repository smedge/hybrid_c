# Hybrid — Product Requirements Document

## Setting & Fiction

**Hybrid** takes place in a digital world where a human brain has been interfaced with a computer system for the first time. The player character is the "Hybrid" — a fusion of human mind and AI consciousness navigating cyberspace.

- **The Grid**: Represents the fabric of cyberspace. Movement across it shows traversal through digital space.
- **Map Cells / Walls**: Give the system topography — structural geometry the player must navigate around. The architecture of the digital system.
- **Mines**: Security intrusion detection devices. Part of the system's defenses against the Hybrid.
- **Future Enemies**: Security programs that actively hunt and try to kill the player.
- **Viruses**: Planned as tools the player eventually uses *against* the system.

## Genre

Bullet hell shooter × Metroidvania

- **Bullet hell**: Dense enemy patterns, projectile-heavy combat, reflexive gameplay
- **Metroidvania**: Interconnected world, ability-gated progression, exploration with fog of war, boss encounters

## Core Mechanics

### The Ship (Player Avatar)

The player's representation in cyberspace. A red triangle that moves with WASD, aims with mouse.

- **Movement**: WASD directional, with speed modifiers (Shift = fast, Ctrl = warp)
- **Death**: Destroyed on contact with solid threats (mines, walls). Respawns at origin after 3 seconds.
- **Death FX**: White diamond spark flash + explosion sound

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
| sub_pea | projectile | Basic projectile weapon. Fires white dots toward cursor. 500ms cooldown, 1000ms TTL, up to 8 simultaneous. | Implemented |
| sub_egress | movement | Quick burst movement for limited duration. Escape/dash ability. | Stub only (empty DEV file) |
| sub_mine | deployable | Drop a mine behind the player to kill pursuing enemies or destroy objects/walls. Unlocked by killing 100 mines. | Not started |

**Many more subroutines planned** — each enemy type will have a corresponding subroutine unlocked by defeating enough of that enemy.

### Progression System

Enemies drop **pills** (small collectibles) when destroyed. Collecting pills from a specific enemy type progresses the player toward unlocking the subroutine associated with that enemy.

- Kill 100 mines → unlock sub_mine (0% to 100% progression)
- Each enemy type has its own subroutine and kill-count threshold
- A **subroutine catalog** window (planned) will show:
  - All known/discovered subroutines with progress meters
  - Obfuscated entries for undiscovered subroutines
  - Serves as both inventory and progression tracker

### World Design

**Structure**:
- 128×128 grid of 100-unit cells (12,800×12,800 unit world)
- Zones are always accessible (no hard locks) except the **final zone** which requires all normal bosses defeated
- Zones are **difficulty-gated**: without the right subroutines, areas are effectively impossible to survive
- This creates natural metroidvania progression — unlock abilities, access harder zones, unlock more abilities

**Bosses**:
- Normal bosses spread across zones
- Final boss zone locked until all normal bosses defeated
- Boss encounters drive major progression milestones

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

- **Subroutine Slots** (bottom): 10 slots for equipped subroutines, activated by number keys
- **Minimap** (bottom-right): 200×200 radar showing nearby geometry with fog of war

## Current Implementation State

### Working
- Ship movement (WASD + speed modifiers)
- Ship-wall collision (death + spark + sound)
- Ship-mine collision (triggers mine arming → explosion)
- Sub_pea projectile system (pooled, 8 max, 500ms cooldown, swept collision)
- Sub_pea wall collision (spark effect at intersection point)
- Sub_pea mine collision (direct hit on mine body causes immediate explosion)
- Mine state machine (idle → armed → exploding → destroyed → respawn)
- Mine idle blink (100ms red flash at 1Hz, randomized per mine)
- Map cell rendering with zoom-scaled outlines
- Grid rendering
- View/camera with zoom
- Main menu (New / Load / Exit)
- HUD skill bar and minimap placeholders (visual only)
- Cursor (red dot + white crosshair)
- 7 gameplay music tracks (random selection) + menu music
- OpenGL 3.3 Core Profile rendering pipeline

### Not Yet Implemented
- Subroutine equip/activation system (slot selection, type exclusivity)
- Sub_egress (movement dash)
- Sub_mine (deployable mine)
- Enemy pill drops and collection
- Subroutine progression/unlock system
- Subroutine catalog UI
- Active security programs (hunting enemies)
- Boss encounters
- Minimap fog of war and rendering
- Full map view
- Save/Load system
- Intro mode
- Virus mechanics (player tools against the system)
- Spatial partitioning for collision (grid buckets — see Technical Architecture)
- Zone/area design beyond current test layout

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

**Music**: 7 deadmau5 tracks for gameplay (random), 1 for menu

## Technical Architecture

- **Language**: C99
- **Graphics**: SDL2 + OpenGL 3.3 Core Profile
- **Audio**: SDL2_mixer
- **Text**: stb_truetype bitmap font rendering
- **Pattern**: Component-based Entity System (ECS-like) — see `spec_000_ecs_refactor.md` for audit findings and refactoring roadmap
- **Rendering**: Batched vertices, two-pass (world + UI), three shader-supported primitive types (triangles, lines, points)

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

### Zone Data Files (Planned)

The current zone is hand-built in code (`mode_gameplay.c` spawns 32 mines at hardcoded positions, `map.c` places wall cells via `set_map_cell` calls). This will be replaced with a data-driven zone system where each zone is defined by a single file.

**Design**:
- One file per zone, 1:1 mapping (e.g., `resources/zones/zone_01.dat`)
- A zone file contains all data needed to fully load that zone:
  - **Map cells / walls**: Position, cell type, colors
  - **Enemy spawn points**: Position, enemy type, count
  - **Portals**: Position, destination zone ID, destination coordinates
  - **Future data**: Boss triggers, environmental hazards, scripted events, loot tables
- A zone loader reads the file and populates the map grid, spawns entities, and registers portals
- The current hardcoded zone becomes the first zone file, serving as the reference format

**Benefits**:
- Zones can be authored, tested, and iterated independently
- Adding a new zone requires no code changes — just a new data file
- Zone transitions (portals) become simple: unload current zone, load target zone file
- Enables future tooling (zone editor) without touching game code

**Implementation Plan**:
- New files: `zone.h` / `zone.c` — zone loading, unloading, and management
- Define a simple binary or text format for zone data (text is easier to hand-edit early on)
- `Zone_load(const char *path)` — parse file, populate map, spawn enemies, register portals
- `Zone_unload()` — clear map, destroy zone entities, reset state
- `mode_gameplay.c` initialization calls `Zone_load()` instead of inline spawning code
- Zone directory: `resources/zones/`
