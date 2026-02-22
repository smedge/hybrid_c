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

```
bucket_x = (int)(world_x / (BUCKET_SIZE * MAP_CELL_SIZE))
bucket_y = (int)(world_y / (BUCKET_SIZE * MAP_CELL_SIZE))
```

Or equivalently, from grid coords:
```
bucket_x = grid_x / BUCKET_SIZE
bucket_y = grid_y / BUCKET_SIZE
```

Integer division — no floating point, no sqrt, no distance checks. An entity's bucket only changes when it crosses a 64-cell boundary, which is infrequent for most enemies.

## Update Tiers

| Tier | Condition | Behavior |
|------|-----------|----------|
| **Active** | In player's 3x3 bucket neighborhood | Full AI, full collision, full rendering |
| **Dormant** | Outside 3x3 | No AI update, no collision, no rendering |

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

8 bytes per reference. The type+index pair maps directly to the existing static arrays (hunters[], seekers[], etc.).

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

### Stale Reference Watchdog

A periodic validation sweep detects registration bugs during development. Every **15 seconds**, the watchdog iterates all bucket entries and checks each EntityRef against the actual entity's `active` flag:

```c
void SpatialGrid_validate(void) {
    for each bucket:
        for each entity_ref in bucket:
            if !entity_is_active(ref.type, ref.index):
                fprintf(stderr, "SPATIAL: stale ref type=%d index=%d in bucket (%d,%d)\n",
                        ref.type, ref.index, bx, by);
                remove from bucket
}
```

This is a safety net, not a cleanup strategy. Proper unregister discipline is the primary mechanism. If the watchdog fires, it means we missed an unregister call somewhere — the console warning tells us exactly which entity type so we can fix the source. The watchdog silently cleans up the stale entry so it doesn't cause ghost collisions in the meantime.

Cost: walking 256 buckets and checking `active` flags is negligible at 15-second intervals.

## Collision Integration

Current collision is brute-force: every collidable checks against every other. With the central registry, collision narrows to the active 3x3 neighborhood:

- **Projectile vs enemy:** Query the projectile's bucket + adjacent buckets. Only test entities in those buckets.
- **Enemy vs enemy:** (Defender healing, seeker contact damage) Same bucket + neighbor query.
- **Entity vs map:** Unchanged — `Map_line_test` is already grid-based and cheap.

This transforms collision from O(n^2) over all entities to O(m^2) where m is the active set count (~30-60 entities typically).

## What Goes Dormant

When an entity is outside the active set:
- No AI state machine updates
- No movement
- No collision checks
- No rendering (already naturally culled by being off-screen, but explicitly skipping avoids wasted transform math)
- No sound
- **Health and status preserved** — a wounded enemy stays wounded when the player returns

## What Stays Active

- Entity existence (array slot remains occupied)
- Bucket membership (still tracked in the grid so it can reactivate)
- Persistent state (HP, patrol waypoints, status effects)

## Edge Cases

- **Entity moving between buckets**: Recompute bucket on position change via `SpatialGrid_update()`. If the entity's new bucket is outside the active set, it goes dormant immediately. An enemy chasing the player across a bucket boundary stays active as long as it's within the 3x3 — the active set is large enough (19,200 world units) that this is a non-issue for normal chase distances.
- **Player moving fast**: Bucket transitions are instant — new 3x3 activates, old one deactivates. No fade-in. Enemies on the new edge immediately start their AI.
- **Projectiles in flight**: Hunter projectiles that enter a dormant zone keep flying. They're simple ballistics (position += velocity * dt), trivially cheap to update. They don't need spatial registration — they're already pooled per-hunter and checked against entities in the active set.
- **Defenders healing allies**: Defender's `find_wounded` queries the spatial grid for entities in its bucket + neighbors instead of scanning the full array by distance.
- **Cross-bucket interactions**: Entities in adjacent active buckets can interact freely — LOS checks, shooting, healing all work across bucket boundaries since both entities are in the active set. The 3x3 neighborhood is specifically sized so that an entity at the edge of one bucket can see/reach entities in the next bucket.

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
- Each enemy update loop wraps with `if (!SpatialGrid_is_active(pos.x, pos.y)) continue;`
- Watchdog: accumulate dt, call `SpatialGrid_validate()` every 15 seconds. Console warnings on stale refs.
- `SpatialGrid_clear()` on zone transitions to wipe all registrations

### Integration With Enemy Modules
- Each enemy type (hunter, seeker, mine, defender, stalker) adds 3 calls: add on spawn, remove on kill/deactivate, update on move
- The `SpatialGrid_is_active()` check at the top of each update function replaces the need for the grid to "push" updates — entities pull their own activity status
- Minimal touch per enemy module — the spatial grid is infrastructure they call into, not something that restructures their code

## Done When

- Spatial partition grid divides the zone into 64x64-cell buckets
- Central registry stores entity references per bucket with capacity 128
- Only entities in the player's 3x3 bucket neighborhood run AI and collision
- Dormant entities preserve state and reactivate seamlessly
- Stale reference watchdog runs every 15 seconds, logs warnings to console
- Enemy counts can scale to 500+ without frame rate impact
- Both systems are transparent to entity code (minimal changes in hunter.c, seeker.c, etc. — just add/remove/update calls)
