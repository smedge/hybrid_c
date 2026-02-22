# Spec: Spatial Partitioning

## Overview

A grid-based bucket system that limits AI updates and collision checks to entities near the player. Only enemies in the player's bucket and the 8 surrounding buckets are ticked. Everything else is dormant. This makes enemy count a map authoring decision, not a performance constraint.

**Prerequisite:** View normalization (see `spec_view_normalization.md`) should be implemented first — it defines the visible area that the active region needs to cover.

## The Problem

Every enemy runs full AI every frame: distance checks, LOS raycasts (`Map_line_test`), pathfinding, shooting logic. On a 1024x1024 zone, the player sees ~3-7% of the map at any time, but 100% of enemies are burning CPU. This caps practical enemy counts at a few hundred before frame rate suffers.

## Design

Divide the zone into a grid of **64x64-cell buckets**. A 1024x1024 zone produces a **16x16 grid of buckets**. Each entity belongs to one bucket based on its grid position.

Every frame, determine the player's bucket. The **active set** is that bucket plus the 8 neighbors — a 3x3 region of 192x192 cells (19,200 world units across). Only entities in the active set get full updates.

## Bucket Assignment

Convert world position to 0-indexed grid coords first, then divide by bucket size. This avoids C's truncation-toward-zero problem with negative world coordinates (the map is centered at origin, so world coords range from -51,200 to +51,200).

```c
int grid_x = (int)(world_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;  // [0, 1024)
int grid_y = (int)(world_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;

int bucket_x = grid_x / BUCKET_SIZE;  // [0, 16)
int bucket_y = grid_y / BUCKET_SIZE;
```

Integer division — no floating point, no sqrt, no distance checks. An entity's bucket only changes when it crosses a 64-cell boundary, which is infrequent for most enemies.

**Why not divide world coords directly?** `(int)(world_x / 6400)` gives wrong results for negative values — C truncates toward zero, so world_x = -100 and world_x = +100 both map to bucket 0 despite being on opposite sides of the map. Converting to grid coords first (always non-negative) avoids this.

## Update Tiers

| Tier | Condition | Behavior |
|------|-----------|----------|
| **Active** | In player's 3x3 bucket neighborhood | Full AI, full collision, full rendering |
| **Dormant** | Outside 3x3 | No AI update, no collision, no rendering (but respawn timers still tick — see below) |

Two tiers keeps it simple. A middle "reduced tick" tier is possible later but adds complexity for marginal gain — the jump from 100% to 3.5% is the big win.

## Why 64x64

- 1024 / 64 = 16x16 = 256 buckets (clean division)
- Active area: 192x192 cells = 19,200 world units across
- At max zoom-out (0.25 scale), normalized visible screen is ~5,760 x 3,600 world units
- Active area is ~3.3x the screen width — enemies start AI well before they're visible
- 64 cells = 6,400 world units per bucket — large enough that enemies don't constantly cross boundaries

## Entity Registration: Central Registry (Option A)

A spatial grid structure stores lists of entity references per bucket. Entities register on spawn, unregister on death/deactivation, and re-register on bucket crossing.

**Why Option A over the alternatives:**
- **Fastest queries** — "what's in this bucket?" is O(1) lookup + iterate the list. No wasted iteration over dormant entities.
- **Collision broadphase solved** — only test pairs within the same or adjacent buckets. Current brute-force O(n^2) broadphase is the real bottleneck for scaling entity counts.
- **Scales with map size, not entity count** — 2,000 enemies on the map but 30 in your neighborhood? You only touch 30.
- The alternative (per-entity bucket ID) still iterates every entity array every frame just to skip most of them.

### Entity References

Each reference in the grid identifies an entity by type and array index:

```c
typedef enum {
    ENTITY_HUNTER,
    ENTITY_SEEKER,
    ENTITY_MINE,
    ENTITY_DEFENDER,
    ENTITY_STALKER
} EntityType;

typedef struct {
    EntityType type;
    int index;
} EntityRef;
```

8 bytes per reference. The type+index pair maps directly to the existing static arrays (hunters[], seekers[], etc.). Only enemy types are registered — the ship is always the camera focus, portals/savepoints are static and don't need spatial updates.

### Per-Bucket Storage

Each bucket holds a fixed-size array of entity references:

```c
#define BUCKET_SIZE 64
#define GRID_MAX 16          // 1024 / 64
#define BUCKET_CAPACITY 128

typedef struct {
    EntityRef entities[BUCKET_CAPACITY];
    int count;
} Bucket;

static Bucket grid[GRID_MAX][GRID_MAX];  // 256 buckets
```

**Memory cost:** 256 buckets x 128 entries x 8 bytes = **256 KB**. Trivial.

**Why 128 capacity:** Total entities across all types is ~2,560 (5 types x 512). Average per bucket is 10. Capacity 128 is 12x the average. Even with worst-case clustering (landmark + obstacle spawns stacking in one area + player kiting a chase group into the same bucket), 128 is generous. Insert is bounds-checked with a stderr warning if it ever triggers — if we see that warning, we bump the capacity.

### Registration Discipline

Every code path that activates or deactivates an entity must call into the spatial grid:

- **Spawn/activate:** `SpatialGrid_add(EntityRef)` — computes bucket from entity position, adds to that bucket
- **Death/deactivate:** `SpatialGrid_remove(EntityRef)` — removes from current bucket
- **Bucket crossing:** `SpatialGrid_update(EntityRef)` — recomputes bucket, moves between buckets if changed

The `_update` call happens in each enemy's movement code. Since bucket crossings only occur at 64-cell boundaries (6,400 world units), the actual move-between-buckets case is infrequent — most frames the bucket hasn't changed and the call is a cheap no-op (compare old bucket to new bucket, return if same).

**Dead enemies stay registered.** An enemy in the DYING/DEAD/respawning state keeps its bucket registration — it still occupies that space and will reactivate in place. Only `*_cleanup()` (zone transition) unregisters.

### Stale Reference Watchdog

A periodic validation sweep detects registration bugs during development. Every **15 seconds**, the watchdog iterates all bucket entries and checks each EntityRef against the actual entity's `alive` flag:

```c
void SpatialGrid_validate(void) {
    for each bucket:
        for each entity_ref in bucket:
            if !entity_is_alive(ref.type, ref.index):
                fprintf(stderr, "SPATIAL: stale ref type=%d index=%d in bucket (%d,%d)\n",
                        ref.type, ref.index, bx, by);
                remove from bucket
}
```

This is a safety net, not a cleanup strategy. Proper unregister discipline is the primary mechanism. If the watchdog fires, it means we missed an unregister call somewhere — the console warning tells us exactly which entity type so we can fix the source. The watchdog silently cleans up the stale entry so it doesn't cause ghost collisions in the meantime.

Cost: walking 256 buckets and checking `alive` flags is negligible at 15-second intervals.

## Collision Integration

There are two collision paths in the codebase, and spatial partitioning benefits both differently:

### Entity Collision System (`Entity_collision_system()`)
The O(n²) brute-force loop in entity.c that tests every collidable against every other (ship vs enemy body contact, etc.). With spatial partitioning, this narrows to entities in the active 3x3 neighborhood — O(m²) where m is the active set count (~30-60 entities).

### Per-Update Damage Checks
Each enemy's update function calls `Enemy_check_player_damage()` which tests all player weapon pools (sub_pea, sub_mine, sub_mgun, etc.) against that enemy's hitbox. These benefit from dormancy **for free** — if the enemy doesn't update, it doesn't run its damage check. No spatial query needed.

### Entity vs Map
Unchanged — `Map_line_test` is already grid-based and cheap.

### Defender Healing
Defender's `find_wounded` queries the spatial grid for entities in its bucket + neighbors instead of scanning the full array by distance.

## Respawn While Dormant

**Respawn timers tick regardless of dormancy.** When a dormant enemy is in the DEAD state, its respawn timer still counts down. When the timer expires, the enemy respawns at its spawn point — even if no player is nearby.

This makes the world feel alive. The player clears an area, leaves, comes back 60 seconds later, and enemies have respawned. The alternative (freezing respawn timers while dormant) creates a dead-zone effect where corpses litter areas the player already cleared, only starting their respawn countdown when revisited. That feels like a bug.

**Implementation:** Each enemy update loop has a lightweight dormancy path:

```c
if (!SpatialGrid_is_active(pos.x, pos.y)) {
    // Dormant: only tick respawn timer
    if (h->aiState == HUNTER_DEAD) {
        h->respawnTimer += ticks;
        if (h->respawnTimer >= RESPAWN_MS) {
            h->alive = true;
            h->hp = HUNTER_HP;
            h->aiState = HUNTER_IDLE;
            placeables[i].position = h->spawnPoint;
            SpatialGrid_update(ref, old_pos, h->spawnPoint);
        }
    }
    continue;
}
// ... full AI update below ...
```

Cost is negligible — one bucket check and at most one int comparison per dormant enemy per frame. No pathfinding, no raycasts, no collision.

**Note on mine phase terminology:** Mines have a `MINE_PHASE_DORMANT` state meaning "alive but not yet triggered." This is unrelated to spatial dormancy (outside the active 3x3). Context makes the distinction clear, but be aware of the naming overlap when reading mine code.

## What Goes Dormant

When an entity is outside the active set:
- No AI state machine updates (except respawn timer — see above)
- No movement
- No collision checks
- No rendering (already naturally culled by being off-screen, but explicitly skipping avoids wasted transform math)
- No sound
- **Health and status preserved** — a wounded enemy stays wounded when the player returns

## What Stays Active

- Entity existence (array slot remains occupied)
- Bucket membership (still tracked in the grid so it can reactivate)
- Persistent state (HP, patrol waypoints, status effects)
- Respawn timers (dead enemies respawn on schedule regardless of player proximity)

## Global Projectile Pools

Hunter projectiles (and any other enemy projectile types) use a **global pool** shared across all instances of that enemy type. Currently, the pool update and player-hit check are gated inside the per-enemy loop on `idx == 0`:

```c
// Current pattern (hunter.c) — BREAKS with dormancy
if (idx == 0) {
    SubProjectile_update(&hunterProjPool, ...);
    // player hit check...
}
```

If hunter[0] is dormant, the entire projectile pool freezes mid-flight. **This must be pulled out of the per-enemy loop** and run unconditionally once per frame, before or after the enemy update loop:

```c
// Fixed pattern — runs regardless of any individual enemy's dormancy
SubProjectile_update(&hunterProjPool, &hunterProjCfg, ticks);
// player hit check...

for (int i = 0; i < highestUsedIndex; i++) {
    // per-enemy AI update with dormancy check...
}
```

Check all enemy types for this pattern (hunter, seeker, defender, stalker) and extract any global pool updates.

**Pool exhaustion warning:** `SUB_PROJ_MAX_POOL` is 256. When `SubProjectile_try_fire` can't find an inactive slot, it recycles the oldest projectile — this works but means projectiles are visually disappearing mid-flight. Print a `stderr` warning when this happens so we can detect saturation during testing:

```c
if (slot < 0) {
    fprintf(stderr, "PROJECTILE: pool exhausted, recycling oldest\n");
    // find and recycle oldest...
}
```

## Edge Cases

- **Entity moving between buckets**: Recompute bucket on position change via `SpatialGrid_update()`. If the entity's new bucket is outside the active set, it goes dormant immediately. An enemy chasing the player across a bucket boundary stays active as long as it's within the 3x3 — the active set is large enough (19,200 world units) that this is a non-issue for normal chase distances.
- **Player moving fast**: Bucket transitions are instant — new 3x3 activates, old one deactivates. No fade-in. Enemies on the new edge immediately start their AI.
- **Projectiles in flight**: Enemy projectiles are globally pooled and updated unconditionally — they keep flying regardless of their source enemy's dormancy state. They don't need spatial registration — they're already pooled per-type and checked against entities in the active set.
- **Defenders healing allies**: Defender's `find_wounded` queries the spatial grid for entities in its bucket + neighbors instead of scanning the full array by distance.
- **Cross-bucket interactions**: Entities in adjacent active buckets can interact freely — LOS checks, shooting, healing all work across bucket boundaries since both entities are in the active set. The 3x3 neighborhood is specifically sized so that an entity at the edge of one bucket can see/reach entities in the next bucket.
- **Respawn position change**: When an enemy respawns at its spawn point (which may differ from where it died), `SpatialGrid_update()` moves it to the correct bucket. If the spawn point is in a different bucket than where it died, it moves cleanly.

## API Surface

```c
// spatial_grid.h
void SpatialGrid_init(void);                    // zero out grid
void SpatialGrid_clear(void);                   // remove all entries (zone transition)
void SpatialGrid_add(EntityRef ref, double world_x, double world_y);
void SpatialGrid_remove(EntityRef ref, double world_x, double world_y);
void SpatialGrid_update(EntityRef ref, double old_x, double old_y, double new_x, double new_y);
bool SpatialGrid_is_active(double world_x, double world_y);  // is this position in the active 3x3?
void SpatialGrid_set_player_bucket(double world_x, double world_y);  // call once per frame
void SpatialGrid_validate(void);                // watchdog — call every 15 seconds

// Query: get all entities in a bucket + neighbors
int SpatialGrid_query_neighborhood(int bucket_x, int bucket_y,
                                    EntityRef *out, int out_capacity);
```

The `_remove` and `_update` calls take position so they can compute the bucket without storing it redundantly on the entity. Position is already on the entity's PlaceableComponent — we just pass it through.

## Implementation Notes

- New module: `spatial_grid.c/h`
- Static 16x16 grid of Bucket structs — no malloc
- Each enemy module calls `SpatialGrid_add` on spawn, `SpatialGrid_remove` on death, `SpatialGrid_update` on movement
- Main game loop calls `SpatialGrid_set_player_bucket()` once per frame before enemy updates
- Each enemy update loop has a dormancy gate: check `SpatialGrid_is_active()`, if dormant only tick respawn timer, then `continue`
- Watchdog: accumulate dt, call `SpatialGrid_validate()` every 15 seconds. Console warnings on stale refs.
- `SpatialGrid_clear()` on zone transitions to wipe all registrations
- Extract global projectile pool updates from per-enemy loops so they run unconditionally

### Integration With Enemy Modules
- Each enemy type (hunter, seeker, mine, defender, stalker) adds 3 calls: add on spawn, remove on kill/deactivate, update on move
- The `SpatialGrid_is_active()` check at the top of each update function replaces the need for the grid to "push" updates — entities pull their own activity status
- Dead enemies in the dormancy path only tick their respawn timer — zero AI cost
- Minimal touch per enemy module — the spatial grid is infrastructure they call into, not something that restructures their code

## Done When

- Spatial partition grid divides the zone into 64x64-cell buckets
- Central registry stores entity references per bucket with capacity 128
- Only entities in the player's 3x3 bucket neighborhood run full AI and collision
- Dormant enemies preserve state, tick respawn timers, and reactivate seamlessly
- Global projectile pool updates run unconditionally (not gated on individual enemy dormancy)
- Stale reference watchdog runs every 15 seconds, logs warnings to console
- Enemy counts can scale to 500+ without frame rate impact
- Both systems are transparent to entity code (minimal changes in hunter.c, seeker.c, etc. — just add/remove/update calls)
