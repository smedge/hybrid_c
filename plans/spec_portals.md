# Spec: Portals & Zone Transitions

## Summary

Portals are data connection points in cyberspace that link zones together. The player stands on a portal for ~1 second, triggering a cinematic warp transition that unloads the current zone and loads the destination. Each portal is bidirectional — linked to a named partner portal in the destination zone, enabling travel both ways.

**Design decisions:**

| Decision | Choice |
|----------|--------|
| Direction | Bidirectional — each portal links to a named partner in the destination zone |
| Transition FX | Cinematic warp — pull-in → data stream → flash → arrival (~2s total) |
| Zone state on return | Fresh reload — enemies/destructibles respawn every visit |
| Activation | Dwell time (~1s) — stand on portal to charge, prevents accidental transitions |

---

## Definitions

| Term | Definition |
|------|-----------|
| **Portal** | An entity placed at a grid cell position that links to a partner portal in another zone (or the same zone). Occupies one map cell (100×100 world units). |
| **Portal ID** | A unique string identifier within a zone (e.g. "east_gate", "boss_entrance"). Used to link portal pairs across zones. |
| **Dwell time** | Duration the player must stand on a portal before the transition triggers. ~1000ms. |
| **Warp sequence** | The multi-phase cinematic transition: pull-in → acceleration → flash/zone swap → arrival → resume. |
| **Arrival suppression** | A brief cooldown (~1500ms) after arriving at a destination portal, preventing immediate re-trigger. |

---

## 1. Portal Entity Design

Portals follow the Mine pattern: static pool, singleton components, per-instance state and placeables.

### Data Structures

```c
#define PORTAL_COUNT 16       /* max portals per zone */
#define PORTAL_DWELL_MS 1000  /* 1 second dwell to activate */

typedef struct {
    char id[32];               /* unique ID within this zone (e.g. "north_gate") */
    char dest_zone[256];       /* path to destination zone file */
    char dest_portal_id[32];   /* portal ID in the destination zone */
    double dwell_timer;        /* ms the ship has been inside this portal */
    bool ship_inside;          /* is the ship currently overlapping? */
    bool active;               /* has this portal been initialized? */
    double anim_timer;         /* ms, drives idle animation cycles */
} PortalState;
```

Pool uses the same static array pattern as mines:

```c
static PortalState portals[PORTAL_COUNT];
static PlaceableComponent placeables[PORTAL_COUNT];
static int portalCount = 0;
```

### Component Setup

```c
static RenderableComponent renderable = { Portal_render };
static AIUpdatableComponent updatable = { Portal_update };
/* No CollidableComponent — portal detection uses direct point test (see Section 4) */
```

Portals intentionally skip the entity collision system. Detection is handled by a direct point-in-AABB test in `Portal_update_all()`, matching the pattern sub_pea and sub_mine already use for hit detection in their update loops. This is simpler than making portals both non-solid and detectable through the collision system.

### Public API

```c
/* Lifecycle */
void Portal_initialize(const char *id, const char *dest_zone,
                       const char *dest_portal_id, Position position);
void Portal_cleanup(void);

/* Per-frame */
void Portal_update_all(unsigned int ticks);
void Portal_render_all(void);
void Portal_render_bloom_source(void);

/* Transition query */
typedef struct {
    char dest_zone[256];
    char dest_portal_id[32];
} PortalTransition;

const PortalTransition *Portal_get_pending_transition(void);
void Portal_clear_pending_transition(void);

/* Arrival positioning */
Position Portal_get_position_by_id(const char *id);

/* God mode overlay */
void Portal_render_god_mode_overlay(void);

/* Zone save support */
int Portal_get_count(void);
const PortalState *Portal_get_state(int index);
const PlaceableComponent *Portal_get_placeable(int index);
```

---

## 2. Portal Visual Design

Portals are concentrated data connection nodes in cyberspace — geometric, glowing, animated.

### Idle Animation

**Core shape**: Diamond (45-degree rotated square), ~60 world units across. Rendered via `Render_quad(&position, 45.0, rect, &color)`.

**Color palette**:
- Outer ring: Cyan `{0.0, 0.8, 1.0, 0.6}`
- Inner diamond: Deep blue `{0.0, 0.2, 0.5, 0.8}`
- Pulse accent: White-cyan `{0.5, 1.0, 1.0, alpha}` where alpha oscillates

**Animation layers** (driven by `anim_timer`):
1. **Rotating outer ring**: Four thick line segments forming a square at radius ~40, rotating at 45 deg/sec. Uses `Render_thick_line` (renders as triangles, correct draw order).
2. **Pulsing inner diamond**: Radius oscillates between 25-30 via `sin(anim_timer * 0.003)`.
3. **Data particles**: 4-8 small points orbiting the center at radius ~35, phase-offset by `anim_timer`. Rendered as `Render_point` with varying alpha.
4. **Corner markers**: Small filled circles at the four cardinal points of the diamond, steady glow.

### Charging Glow (Dwell Feedback)

As `dwell_timer` goes from 0 to `PORTAL_DWELL_MS`:

- **Progress fraction**: `f = dwell_timer / PORTAL_DWELL_MS` (0.0 → 1.0)
- Inner diamond alpha: 0.8 → 1.0
- Outer ring color: cyan → white (lerp by `f`)
- Pulse frequency: multiply animation speed by `1.0 + f * 3.0`
- Ring thickness: 2.0 → 4.0
- At `f > 0.8`: rapid point particles radiate outward (4-6 per frame)

### Bloom Source

`Portal_render_bloom_source()` re-renders:
- Inner diamond at full brightness
- Rotating outer ring
- Charging glow (if dwell > 0)
- NOT data particles (too small, just noise in bloom)

This makes portals visually glow on the map from far away — important for navigation.

### Minimap Representation

Portals appear on the minimap as bright cyan dots, distinct from wall cells. Portal render pass added to `Hud_render` — iterate portal pool, render a point at each portal position.

---

## 3. Zone File Format

### Portal Line Format

```
portal <grid_x> <grid_y> <portal_id> <dest_zone_path> <dest_portal_id>
```

### Bidirectional Example

**zone_001.zone:**
```
portal 540 512 east_gate ./resources/zones/zone_002.zone west_gate
portal 480 512 west_gate ./resources/zones/zone_003.zone east_gate
```

**zone_002.zone:**
```
portal 480 512 west_gate ./resources/zones/zone_001.zone east_gate
portal 540 512 east_gate ./resources/zones/zone_004.zone west_gate
```

Each portal explicitly names its partner. Both sides must be authored — the engine does not auto-create return portals.

### Zone Struct Addition

```c
#define ZONE_MAX_PORTALS 16

typedef struct {
    int grid_x, grid_y;
    char id[32];
    char dest_zone[256];
    char dest_portal_id[32];
} ZonePortal;

/* Added to Zone struct: */
ZonePortal portals[ZONE_MAX_PORTALS];
int portal_count;
```

### Parser

New `else if` branch in `Zone_load()`:

```c
else if (strncmp(line, "portal ", 7) == 0) {
    if (zone.portal_count >= ZONE_MAX_PORTALS) continue;
    ZonePortal *p = &zone.portals[zone.portal_count];
    if (sscanf(line + 7, "%d %d %31s %255s %31s",
               &p->grid_x, &p->grid_y, p->id,
               p->dest_zone, p->dest_portal_id) == 5)
        zone.portal_count++;
}
```

### Saver

New block in `Zone_save()` after spawns:

```c
for (int i = 0; i < zone.portal_count; i++) {
    ZonePortal *p = &zone.portals[i];
    fprintf(f, "portal %d %d %s %s %s\n",
            p->grid_x, p->grid_y, p->id,
            p->dest_zone, p->dest_portal_id);
}
```

### apply_zone_to_world

After spawning mines, spawn portals:

```c
Portal_cleanup();
for (int i = 0; i < zone.portal_count; i++) {
    ZonePortal *p = &zone.portals[i];
    double wx = (p->grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5;
    double wy = (p->grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5;
    Position pos = {wx, wy};
    Portal_initialize(p->id, p->dest_zone, p->dest_portal_id, pos);
}
```

---

## 4. Dwell Time Mechanics

### Detection

Each frame during `Portal_update_all(ticks)`:

1. Get ship position via `Ship_get_position()`
2. For each active portal, test if the ship's center point is inside the portal's 100×100 bounding box
3. If inside and ship not destroyed: increment `dwell_timer += ticks`
4. If not inside: reset `dwell_timer = 0`, clear `ship_inside`

Point test (not AABB overlap) because the ship's 40×40 collision box is small — a point test against the portal's 100×100 cell ensures the player must actually be standing on the portal, not clipping the edge.

```c
void Portal_update_all(unsigned int ticks) {
    if (portal_suppress_timer > 0) {
        portal_suppress_timer -= ticks;
        /* Still update animations */
        for (int i = 0; i < portalCount; i++)
            portals[i].anim_timer += ticks;
        return;
    }

    Position ship_pos = Ship_get_position();

    for (int i = 0; i < portalCount; i++) {
        PortalState *ps = &portals[i];
        if (!ps->active) continue;

        Position portal_pos = placeables[i].position;
        Rectangle bbox = Collision_transform_bounding_box(portal_pos,
            (Rectangle){-50.0, 50.0, 50.0, -50.0});

        bool inside = Collision_point_test(ship_pos.x, ship_pos.y, bbox);

        if (inside && !Ship_is_destroyed()) {
            ps->ship_inside = true;
            ps->dwell_timer += ticks;
            if (ps->dwell_timer >= PORTAL_DWELL_MS) {
                set_pending_transition(ps->dest_zone, ps->dest_portal_id);
            }
        } else {
            ps->ship_inside = false;
            ps->dwell_timer = 0.0;
        }

        ps->anim_timer += ticks;
    }
}
```

### Activation

When `dwell_timer >= PORTAL_DWELL_MS`:
- Set a static `pending_transition` with dest_zone and dest_portal_id
- `Portal_get_pending_transition()` returns a pointer to this
- The gameplay state machine checks this each frame and initiates the warp

### Arrival Suppression

After arriving at a destination portal, the ship is standing on it. Without protection, the dwell timer would immediately start counting toward a return trip.

Solution: `portal_suppress_timer` set to 1500ms at the end of WARP_ARRIVE. While active, `Portal_update_all` skips all dwell accumulation.

### Audio Feedback

- `dwell_timer > 0`: play charging loop sound, fading in over the dwell period
- Activation: cut charging sound, play warp initiation sound
- Ship leaves before activation: fade out charging sound quickly (~100ms)

---

## 5. Warp Transition Sequence

The transition is a cinematic multi-phase sequence driven by new gameplay states. Total duration ~2 seconds.

### New Gameplay States

```c
typedef enum {
    GAMEPLAY_REBIRTH,          /* existing: zoom-in birth sequence */
    GAMEPLAY_ACTIVE,           /* existing: normal gameplay */
    GAMEPLAY_WARP_PULL,        /* ship pulled toward portal center */
    GAMEPLAY_WARP_ACCEL,       /* camera zooms in, data stream effect */
    GAMEPLAY_WARP_FLASH,       /* white flash, zone swap happens */
    GAMEPLAY_WARP_ARRIVE,      /* camera zooms out at destination */
    GAMEPLAY_WARP_RESUME       /* brief input lockout before full control */
} GameplayState;
```

A `warpTimer` tracks progress within each phase. Phase transitions advance the state and reset the timer.

### Phase Breakdown

**Phase 1: WARP_PULL (600ms)**
- Ship input disabled (no WASD, no subroutines)
- Ship velocity overridden: linearly interpolate ship position toward portal center
- Camera stays locked on ship (normal follow behavior)
- Portal glow intensifies to full white
- Ship leaves dense motion trail toward portal center
- Audio: warp charge sound intensifies
- **Ship can still die** (collision active) — if destroyed, cancel warp, return to GAMEPLAY_ACTIVE

**Phase 2: WARP_ACCEL (400ms)**
- Ship at portal center, invisible (or very small)
- Camera zoom accelerates inward (zoom from default 0.5 → 2.0 in log space)
- Radial lines streak outward from screen center (data stream effect)
- Screen edges fade to white
- Portal bloom at max intensity
- Audio: rising pitch whoosh
- Collision disabled

**Phase 3: WARP_FLASH (200ms)**
- Screen goes full white (fullscreen white quad over everything)
- **Zone swap happens here** (see Section 6)
- Audio: brief silence or bass impact thud

**Phase 4: WARP_ARRIVE (600ms)**
- New zone loaded; ship positioned at destination portal
- Ship visible but input still disabled
- Camera zoom set to tight (2.0), smoothly zooms back out to default (0.5)
- Inverse radial lines (data stream converging inward)
- Destination portal pulses brightly
- Audio: emergence/arrival sound, crossfade into new zone BGM

**Phase 5: WARP_RESUME (200ms)**
- Ship input re-enabled
- Camera fully at default zoom
- Set `portal_suppress_timer = 1500`
- Gameplay state returns to GAMEPLAY_ACTIVE

### Data Stream Visual Effect

Rendered in the UI pass (screen-space, not world-space):

- 20-30 lines radiating from screen center
- Each line has random length and angular offset (seeded per-line, not per-frame)
- Length increases over time (Phase 2: outward) or decreases (Phase 4: inward)
- Color: white with alpha fade at the tips
- Simple function: `render_warp_streaks(float intensity, bool outward)`

### State Behavior Summary

| State | Ship Input | Ship Movement | Camera | AI Systems | Collision |
|-------|-----------|---------------|--------|------------|-----------|
| WARP_PULL | Disabled | Interpolated to portal | Following ship | Running | Active |
| WARP_ACCEL | Disabled | None (at portal center) | Zooming in | Running | Disabled |
| WARP_FLASH | Disabled | None | Frozen | Stopped | Disabled |
| WARP_ARRIVE | Disabled | None (at dest portal) | Zooming out | Running (new zone) | Disabled |
| WARP_RESUME | Enabled | Normal | Following ship | Running | Active |

---

## 6. Zone Transition Flow

The zone swap happens during WARP_FLASH, while the white flash covers the screen.

### State to Preserve

| System | What Persists | Mechanism |
|--------|---------------|-----------|
| Skillbar | Equipped subroutines in all 10 slots | `Skillbar_snapshot()` / `Skillbar_restore()` |
| Progression | Fragment counts, unlock states, discovery states | Static globals in `progression.c` — NOT re-initialized during transition |
| Player Stats | Current integrity and feedback levels | `PlayerStats_snapshot()` / `PlayerStats_restore()` |
| Catalog | Tab state, seen states | Static globals in `catalog.c` — preserved automatically |

### State to Reset

| System | What Resets | Why |
|--------|-------------|-----|
| Map | Entire grid | Fresh zone load |
| Mines (enemy) | All mine entities | Enemies respawn per fresh load |
| Portals | Portal pool | New zone has different portals |
| Destructibles | Destruction states + timers | Fresh zone — walls respawn |
| Fragments (world) | All floating fragment entities | They belong to the old zone |
| Sub_Pea | All active projectiles | Projectiles don't travel between zones |
| Sub_Mine (player) | All deployed player mines | Player mines don't persist |
| Movement subs | Dash/boost state | Prevents stale movement state (same as respawn) |

### Exact Call Sequence

```
// --- UNLOAD ---
1. Store dest_zone path and dest_portal_id in local variables
2. snapshot = PlayerStats_snapshot()
3. sb_snapshot = Skillbar_snapshot()
4. Fragment_deactivate_all()
5. Sub_Pea_deactivate_all()
6. Sub_Mine_deactivate_all()
7. Zone_unload()                  // calls Map_clear() + Mine_cleanup()
8. Portal_cleanup()
9. Entity_destroy_all()

// --- LOAD ---
10. Map_initialize()              // re-create map entity
11. Ship_initialize()             // re-create ship entity
12. Zone_load(dest_zone)          // parse file, place cells, spawn mines + portals
13. Destructible_initialize()     // set up new zone's destructibles

// --- POSITION ---
14. Position arrival = Portal_get_position_by_id(dest_portal_id)
15. Ship_force_spawn(arrival)     // place ship at destination portal
16. View_set_position(arrival)    // snap camera to arrival point

// --- RESTORE ---
17. PlayerStats_restore(snapshot)
18. Skillbar_restore(sb_snapshot)

// --- MUSIC ---
19. Pick new random BGM track
20. Start playing
```

### Why Entity_destroy_all + re-initialize?

Mine and Portal systems use static arrays where entity IDs index into a pool. When `Zone_unload` calls `Mine_cleanup()`, it resets `highestUsedIndex = 0`, but the entity slots in the global entity array still reference old pointers. `Entity_destroy_all()` marks all 1024 slots as empty. Then `Ship_initialize()` and `Zone_load()` re-add entities cleanly starting from slot 0. This is the same pattern as `Mode_Gameplay_initialize`/`Mode_Gameplay_cleanup`, just done mid-game.

### New APIs Needed

```c
/* player_stats.h */
typedef struct {
    double integrity;
    double feedback;
} PlayerStatsSnapshot;

PlayerStatsSnapshot PlayerStats_snapshot(void);
void PlayerStats_restore(PlayerStatsSnapshot snapshot);

/* skillbar.h */
typedef struct {
    int slot_subs[SKILLBAR_SLOTS];  /* SubroutineId per slot, or SUB_NONE */
} SkillbarSnapshot;

SkillbarSnapshot Skillbar_snapshot(void);
void Skillbar_restore(SkillbarSnapshot snapshot);

/* ship.h */
bool Ship_is_destroyed(void);
```

**Important**: `Progression_initialize()` and `Catalog_initialize()` are NOT called during zone transitions. They are only called once per gameplay session (entering from main menu). The transition flow does not touch them — their state persists in static globals.

---

## 7. God Mode Integration

### Current State

Portals require string metadata (id, dest_zone, dest_portal_id) that god mode has no text input for. For now, portals are authored directly in zone files. God mode displays portal positions with labels but doesn't create them.

### Portal Visibility in God Mode

When `godModeActive`, `Portal_render_god_mode_overlay()` draws for each portal:
- Portal ID as text above the portal diamond
- Destination zone name as text below
- Distinct border color (bright cyan outline around the cell)

### Future Catalog Integration

When god mode Phase 3 (placeable catalog) is implemented, portals get their own tab. The catalog entry for a portal needs a sub-dialog for string fields. Out of scope for this spec.

### Undo Support

Zone editing undo needs new undo types:

```c
UNDO_PLACE_PORTAL,
UNDO_REMOVE_PORTAL
```

The `UndoEntry` union needs a `ZonePortal` field for storing removed portal data.

---

## 8. Edge Cases

### Destination Zone File Missing

If `Zone_load()` fails to open the destination file:
- Detect by checking `Zone_get()->name[0] == '\0'` after load attempt
- Abort transition: reload the source zone (store source zone path before unloading)
- Spawn ship at origin (0,0) as fallback
- Print error to stdout: "Portal transition failed: zone '%s' not found"

### Destination Portal ID Not Found

If `Portal_get_position_by_id(dest_portal_id)` finds no match in the new zone:
- Return sentinel position `{0.0, 0.0}` (zone origin)
- Print warning: "Portal '%s' not found in zone, spawning at origin"
- Player arrives at origin instead of a portal

### Player Dies During Warp

- **During WARP_PULL**: Cancel warp, return to GAMEPLAY_ACTIVE, normal death/respawn
- **During WARP_ACCEL through WARP_RESUME**: Ship collision is disabled, death cannot occur

### God Mode Toggle During Warp

Ignored. God mode toggle only processed in GAMEPLAY_ACTIVE state.

### Rapid Re-Entry (Arrival Suppression)

After arriving at destination, `portal_suppress_timer = 1500ms`. While active, `Portal_update_all` skips all dwell accumulation. Timer decrements each frame. This is slightly longer than `PORTAL_DWELL_MS` so the player can't re-trigger by standing still.

### Portal on Solid Wall Cell

Designer error — the player can't reach the portal because the ship would die on wall contact. The engine does not prevent this placement. The designer should avoid it.

### Multiple Portals, Overlapping

Each portal has its own dwell timer. Only one can trigger at a time (the ship's center can only be inside one at a time). If somehow overlapping, the first in the array wins.

### Self-Referencing Portal

A portal pointing to its own zone is valid. The zone reloads fresh, ship arrives at the destination portal. Effectively a zone reset mechanic.

---

## 9. Implementation Order

| Step | What | Visible Result |
|------|------|---------------|
| 1 | Zone file portal parsing + saving | printf parsed portal data |
| 2 | Portal entity skeleton (pool, cyan diamond render) | See diamond in world |
| 3 | Spawn portals in apply_zone_to_world | Portals appear on zone load |
| 4 | Dwell detection + pending transition + Ship_is_destroyed() | printf dwell timer, triggers at 1s |
| 5 | Full portal visuals (animation, charging glow) | Portal looks good idle + charging |
| 6 | Portal bloom source | Portal glows |
| 7 | Warp state machine skeleton (state transitions, timer) | State changes on printf |
| 8 | Zone swap + snapshot/restore APIs | Round-trip zone_001 ↔ zone_002 works |
| 9 | Warp visual effects (pull-in, streams, flash, zoom) | Cinematic transition |
| 10 | Audio (charging, warp, arrival, BGM swap) | Full audio experience |
| 11 | God mode portal overlay (ID + dest labels) | Labels visible in god mode |
| 12 | Minimap portal blips | Cyan dots on minimap |
| 13 | Create zone_002.zone test zone | Full round-trip test |

---

## 10. Files Summary

| File | Action | Purpose |
|------|--------|---------|
| `src/portal.h` | **NEW** | Portal entity header |
| `src/portal.c` | **NEW** | Portal pool, render, update, bloom, dwell detection |
| `src/zone.h` | Modify | ZonePortal struct, portal array in Zone |
| `src/zone.c` | Modify | Parse/save portal lines, spawn in apply_zone_to_world |
| `src/mode_gameplay.c` | Modify | Warp state machine, visual FX, zone swap orchestration |
| `src/ship.h` / `.c` | Modify | Ship_is_destroyed() getter |
| `src/player_stats.h` / `.c` | Modify | Snapshot/restore API |
| `src/skillbar.h` / `.c` | Modify | Snapshot/restore API |
| `src/component.h` | Modify | (only if using collision layers — current design uses direct point test instead) |
| `src/hud.c` | Modify | Portal minimap blips |
| `resources/zones/zone_001.zone` | Modify | Add portal lines |
| `resources/zones/zone_002.zone` | **NEW** | Test destination zone |

---

## 11. Test Zone Content

### zone_001.zone portal placement

The existing minefield extends to ±3200 world units (grid ~480-544 from center 512). Place the portal well east of the outermost mines with ~20 cells of clearance:

```
portal 565 512 east_gate ./resources/zones/zone_002.zone west_gate
```

Grid 565, 512 → world position (5350, 50) — ~2150 world units east of the nearest mine at 3200.

### zone_002.zone — minimal test zone

Empty zone with just the return portal. The designer can dress it up with god mode.

```
# Zone: Test Zone
name Test Zone
size 256

celltype solid 20 0 20 255 128 0 128 255 none
celltype circuit 10 20 20 255 64 128 128 255 circuit

portal 128 128 west_gate ./resources/zones/zone_001.zone east_gate
```

Smaller zone (256×256) since it's just a test. Portal at center (128, 128) → world origin of that zone.
