# spec_000: ECS Framework Refactor

## Summary

The current ECS framework supports ~30 mines, 1 ship, and 8 projectiles. It cannot scale to multiple enemy types, large entity counts, or the progression/combat systems described in the PRD. This spec captures the problems found during audit and defines a phased refactoring roadmap.

---

## Critical Bugs (Fix Immediately)

### 1. Collision Buffer Overflow — `entity.c:160-163`

`Entity_create_collision_command` increments `highestCollisionIndex` with **no bounds check** against `COLLISION_COUNT` (64). If more than 64 collisions occur in a single frame, memory is silently corrupted.

**Trigger scenario**: Dense cluster of mines + ship + explosions could exceed 64 collision pairs in one frame.

**Fix**: Add bounds check, drop excess collisions or assert.

### 2. Mine Pool Exhaustion — `mine.c:49-51`

`Mine_initialize` calls `exit(-1)` when `highestUsedIndex == MINE_COUNT` (64). The game hard-crashes with no recovery.

**Fix**: Return an error, or implement pool recycling (reuse destroyed mines).

### 3. `highestIndex` Never Decreases — `entity.c:26-33`

`highestIndex` tracks the highest entity slot ever used but never shrinks when entities are destroyed. After creating and destroying 500 entities, every system iterates 500 slots per frame even if only 10 are active.

**Fix**: Rebuild `highestIndex` on entity destruction, or switch to a freelist/compact array.

### 4. `correctTruncation` Uses Pre-Decrement — `map.c:184-189`

```c
if (i < 0) return --i;
```

Should be `return i - 1`. The pre-decrement is confusing and relies on implicit behavior. Functionally equivalent here but reads as a mutation side effect.

---

## Architectural Problems

### 5. Entity Types Manage Their Own Pools

Each entity type allocates and manages its own state arrays independently:

- `mine.c`: `static MineState mines[64]`, `static PlaceableComponent placeables[64]`
- `sub_pea.c`: `static Projectile projectiles[8]`
- `ship.c`: `static ShipState shipState`, `static PlaceableComponent placeable`

**Problems**:
- No central registry of all active entities
- Each type has different pool sizes, exhaustion behavior, and lifecycle patterns
- Adding a new entity type requires building a new pool from scratch
- Cannot query "all entities in area" without knowing every type

### 6. Components Are Static Singletons, Not Per-Instance

Ship has one static `RenderableComponent`, `CollidableComponent`, etc. shared by the single instance. Mine shares `renderable` and `collidable` across all 64 instances (only `placeable` and `state` are per-instance arrays).

**Problem**: Cannot have two ship-like entities, or enemies with per-instance collision boxes. The component model is fake — it's really just function pointer dispatch with shared config.

### 7. Inter-Entity Interactions Are Hardcoded

- `mine.c` directly calls `Sub_Pea_check_hit()`
- `ship.c` directly calls `Sub_Pea_update()`
- `sub_pea.c` stores a raw `Entity *parent` pointer to the ship

Adding a new entity that interacts with projectiles requires modifying both the new entity AND `sub_pea.c`. There is no event system, message bus, or collision callback dispatch.

### 8. Sub_Pea Bypasses the Entity System Entirely

Projectiles are not entities. They live in a static array inside `sub_pea.c`, are updated via direct call from `Ship_update`, and are collision-checked via direct calls from `Mine_update`. The ECS collision system never sees them.

### 9. Collision System Is Asymmetric

Entity `i` (with `collidesWithOthers=true`) checks against entity `j`'s `collide` function, but resolution is called on entity `i`. The collision data comes from `j`'s perspective but is resolved from `i`'s perspective. This works by convention but is fragile and undocumented.

### 10. No Entity Lifecycle Hooks

`Entity_destroy` marks the slot as empty. No cleanup callback, no `on_destroy` hook, no resource release. State pointers become dangling unless the entity type module handles cleanup externally.

---

## Missing Features (Needed Before Game Grows)

### 11. No Entity Tags or Groups

Cannot query entities by type, role, or faction. Must iterate all slots and check component presence.

### 12. No Spatial Partitioning

Collision is O(n^2) — every collidable entity checked against every other. No broad-phase. See PRD spatial partitioning section for the planned grid bucket solution.

### 13. No Entity Disable/Enable

The `disabled` flag exists on Entity but has no public API. Cannot temporarily deactivate entities (e.g., off-screen culling, pausing, phase transitions).

### 14. No Data-Driven Entity Configuration

All entity parameters (pool sizes, velocities, cooldowns, bounding boxes) are `#define` constants requiring recompilation. No config files, no prefabs.

### 15. No Serialization

Cannot save/load entity state. No way to enumerate all active entities and dump their state. Blocks save/load system.

### 16. Unused `DynamicsComponent`

`component.h` defines `DynamicsComponent` with a `mass` field. It's never used anywhere. Either build it out or remove it.

---

## Refactoring Phases

### Phase 1: Critical Fixes (Do Now)

**No architectural changes. Just prevent crashes.**

1. Add bounds check to `Entity_create_collision_command` — clamp or assert at `COLLISION_COUNT`
2. Fix `correctTruncation` to use `i - 1` instead of `--i`
3. Add `highestIndex` recalculation to `Entity_destroy`
4. Replace mine pool `exit(-1)` with graceful handling (skip creation, log warning)

**Files**: `entity.c`, `map.c`, `mine.c`

### Phase 2: Unified Entity Lifecycle (Before Second Enemy Type)

**Goal**: All game objects are entities in the central pool. No separate static arrays.

1. Per-instance component allocation: entities own their `PlaceableComponent`, `CollidableComponent`, and state. Allocate from typed pools rather than sharing static singletons
2. Entity factory functions: `Entity_create_mine(Position)`, `Entity_create_ship()`, etc. return `Entity*` and handle all component setup
3. Lifecycle hooks: add `on_destroy` callback to Entity. Called by `Entity_destroy` before marking empty
4. Move Sub_Pea into the entity system as proper entities with `CollidableComponent`
5. Remove the parallel arrays from `mine.c` — mine state lives in the entity pool

**Files**: `entity.h`, `entity.c`, `component.h`, `mine.c`, `ship.c`, `sub_pea.c`

### Phase 3: Decoupled Interactions (Before Complex Combat)

**Goal**: Entities interact through the system, not through direct cross-module calls.

1. Collision layers/masks: entities declare what layers they're on and what layers they collide with (e.g., `LAYER_PLAYER`, `LAYER_ENEMY`, `LAYER_PROJECTILE`)
2. Collision callbacks become symmetric: both entities in a collision pair get notified with a reference to the other
3. Remove direct `Sub_Pea_check_hit` calls from `mine.c` — mine-projectile collision happens through the ECS collision system
4. Event system (optional): lightweight publish/subscribe for game events (entity_destroyed, damage_dealt, item_dropped)

**Files**: `entity.h`, `entity.c`, `collision.h`, `component.h`, `mine.c`, `sub_pea.c`

### Phase 4: Spatial Partitioning (Before 100+ Entities)

As described in PRD. Grid bucket spatial hash for O(1) collision lookups.

**Files**: new `spatial.h`/`spatial.c`, `entity.c`

### Phase 5: Data-Driven Entities (Before Content Expansion)

1. Entity prefab definitions in zone data files
2. Component parameters loaded from data, not `#define` constants
3. Entity tags/groups for querying by type

**Files**: `zone.c`, `entity.h`, `entity.c`

---

## Scope Notes

- This spec does not prescribe a "pure ECS" (struct-of-arrays, archetype-based). The current pointer-based entity with component structs is fine for this game's scale. The goal is to fix bugs, unify lifecycle, and decouple interactions — not rewrite into a generic ECS engine.
- Each phase should be independently shippable. The game should compile and run correctly after each phase.
- Phase 1 is prerequisite for everything. Phases 2-5 can be reordered based on what feature is being built next.
