# Pipeline Refactor Spec (Render + Update)

## Problem

`Mode_Gameplay_render()` has **89 explicit render calls** across 9 hardcoded stages. Every entity module (hunter, seeker, mine, etc.) and every subroutine (sub_pea, sub_mine, sub_aegis, etc.) has its bloom, light, and overlay render functions called individually by name. Adding a new entity or sub means manually wiring 3-5 new calls into mode_gameplay.c in the correct order.

This is the opposite of the ECS philosophy. `Entity_render_system()` already iterates entities and calls their `render` callback — but bloom source, light source, and world overlay renders bypass this entirely.

### Current Pain Points
- **89 render calls** in a single function, growing with every new entity/sub
- **Tight coupling**: mode_gameplay.c must `#include` every module header
- **Ordering bugs**: easy to add a bloom call in the wrong stage
- **No discoverability**: new contributors must trace mode_gameplay.c to understand what renders where
- **Duplication**: corridors, auras, fire pools, footprints all follow the same pattern (main + bloom + light) but each is hand-wired

### How We Got Here

The original ECS design got this right: `RenderableComponent` has a `render` callback, `Entity_render_system()` iterates entities and calls it. One pattern, works everywhere. That was the intent for all rendering — entity owns how it draws itself, pipeline just calls it.

Then bloom FBOs were added. Instead of extending `RenderableComponent` with a bloom callback, we took the shortcut: write a standalone `Hunter_render_bloom_source()` function and call it explicitly from mode_gameplay.c. It was one line, it worked, ship it. Then light FBOs came, same shortcut. Then overlays. Every new entity and sub copy-pasted the pattern. 89 explicit calls later, mode_gameplay.c became a 220-line manifest of every renderable thing in the game, and the original design — entity owns its rendering — was buried under expedience.

The fix is to finish what the ECS started: extend `RenderableComponent` to support multiple render passes, so bloom/light/overlay rendering follows the same per-entity callback pattern that main rendering already uses. No new architecture — just completing the one we already have.

## Design Principles

1. **Entity owns its render passes.** Each entity/module registers what it needs rendered and in which passes.
2. **Pipeline owns ordering.** The render pipeline defines pass order; entities don't need to know about FBOs or bloom.
3. **Opt-in passes.** An entity with no bloom contribution simply doesn't register a bloom callback.
4. **Zero-overhead for simple entities.** Entities that only need a basic `render` callback keep working unchanged.

## Proposed Architecture

### Render Pass Enum

```c
typedef enum {
    RENDER_PASS_LIGHT_SOURCE,     // captured into light_bloom FBO
    RENDER_PASS_WEAPON_BLOOM,     // captured into weapon bloom FBO (beam weapons: disintegrate, inferno, future beams)
    RENDER_PASS_BLOOM_SOURCE,     // captured into main bloom FBO
    RENDER_PASS_WORLD_OVERLAY,    // rendered after bloom composite, before UI (corridors, auras, pools)
    RENDER_PASS_MAIN,             // the existing entity render (body/sprite)
    RENDER_PASS_COUNT
} RenderPass;
```

### Extended RenderableComponent

```c
typedef void (*RenderFunc)(const void *state, const PlaceableComponent *placeable);

typedef struct {
    RenderFunc passes[RENDER_PASS_COUNT];  // NULL = skip this pass
} RenderableComponent;
```

The existing `render` callback becomes `passes[RENDER_PASS_MAIN]`. All other slots default to NULL.

### Render Pipeline (mode_gameplay.c)

The render function collapses from 89 explicit calls to pass-driven loops:

```c
// Stage: Light FBO
Bloom_begin_source(light_bloom);
Entity_render_pass(RENDER_PASS_LIGHT_SOURCE);   // replaces 31 explicit calls
GlobalRender_pass(RENDER_PASS_LIGHT_SOURCE);    // non-entity light sources (burn, map)
Bloom_end_source(light_bloom);

// Stage: Weapon Bloom FBO
Bloom_begin_source(weapon_bloom);
Entity_render_pass(RENDER_PASS_WEAPON_BLOOM);    // beam weapons (disintegrate, inferno, future beams)
GlobalRender_pass(RENDER_PASS_WEAPON_BLOOM);
Bloom_end_source(weapon_bloom);

// Stage: Main Bloom FBO
Bloom_begin_source(bloom);
Entity_render_pass(RENDER_PASS_BLOOM_SOURCE);    // replaces 25 explicit calls
GlobalRender_pass(RENDER_PASS_BLOOM_SOURCE);    // non-entity bloom sources (map, burn)
Bloom_end_source(bloom);

// Stage: Entities + Overlays
Entity_render_pass(RENDER_PASS_WORLD_OVERLAY);   // replaces 8 explicit overlay calls
Entity_render_pass(RENDER_PASS_MAIN);            // existing Entity_render_system()
```

Where `Entity_render_pass(pass)` iterates the entity array and calls `entities[i].renderable->passes[pass]` for non-NULL entries.

### Global Render Registry

Some render sources are not entities (Map bloom, Burn system, background). These register callbacks into a parallel global registry:

```c
typedef void (*GlobalRenderFunc)(void);

void GlobalRender_register(RenderPass pass, GlobalRenderFunc func);
```

The pipeline iterates these after entity passes. This handles:
- `Map_render_bloom_source` (map is not an entity)
- `Burn_render_*` (burn is a cross-cutting system)
- `Sub_Scorch_render_footprints*` (player footprints are not entities)
- Other sub effects that exist independently of entities (corridors, auras, pools)

### Entity Registration Example

```c
// hunter.c — at entity creation time
RenderableComponent hunterRenderable = {
    .passes = {
        [RENDER_PASS_MAIN]         = Hunter_render,
        [RENDER_PASS_BLOOM_SOURCE] = Hunter_render_bloom_source,
        [RENDER_PASS_LIGHT_SOURCE] = Hunter_render_light_source,
    }
};
```

```c
// mine.c — mine entity
RenderableComponent mineRenderable = {
    .passes = {
        [RENDER_PASS_MAIN]          = Mine_render,
        [RENDER_PASS_BLOOM_SOURCE]  = Mine_render_bloom_source,
        [RENDER_PASS_LIGHT_SOURCE]  = Mine_render_light_source,
    }
};

// Fire pool spawned as its own lightweight entity when mine detonates
RenderableComponent firePoolRenderable = {
    .passes = {
        [RENDER_PASS_MAIN]          = FirePool_render,
        [RENDER_PASS_BLOOM_SOURCE]  = FirePool_render_bloom_source,
        [RENDER_PASS_LIGHT_SOURCE]  = FirePool_render_light_source,
    }
};
// + CollidableComponent for burn damage via Entity_collision_system()
```

## What Does NOT Change

- **Background bloom** — stays as-is. It's a scene-level pass, not entity-driven.
- **MapReflect / stencil pipeline** — stays as-is. Stencil is a scene-level resource.
- **UI rendering** — stays as-is. UI is not part of the entity system.
- **God mode debug overlays** — stays as-is. Debug renders are intentionally explicit.
- **Render_flush() placement** — the pipeline still controls when flushes happen between passes. **Render callbacks (both per-entity and global) must only emit geometry into the batch — never call `Render_flush()`.** The pipeline flushes at stage boundaries. This is already the implicit rule today; the refactor makes it an explicit contract.
- **Bloom FBO lifecycle** — begin/blur/composite stays in the pipeline, not in entities.

## Edge Cases

### Per-Entity vs Per-Module Rendering

Today, bloom/light functions like `Hunter_render_bloom_source()` are module-level — one call loops all hunters internally. The refactor moves to per-entity callbacks, where the pipeline calls each hunter's bloom callback individually as it iterates the entity array. This is not a concern — it's exactly how `RENDER_PASS_MAIN` already works. Each entity already has its own `render` callback that draws just that one instance. Bloom/light callbacks follow the same pattern, receiving `(state, placeable)` for that specific entity.

The split is clean: anything that IS an entity (hunter body, seeker body, mine body) renders through per-entity passes. Anything that's a module-level singleton (projectile pools, fire pools, corridors, footprints) renders through the global registry. These singletons aren't entities — they're shared pools managed at the module level, so the global registry is the correct home for them.

### Pool-Based Modules (hunter projectiles, stalker projectiles)
Hunter and Stalker projectile pools are module-level singletons, not per-entity state. They register their render callbacks via the global registry.

### Ground Effects (corridors, fire pools, footprints, auras)
These become lightweight entities with their own RenderableComponent passes and CollidableComponent. They render through `Entity_render_pass()` and collide through `Entity_collision_system()` like any other entity. This replaces both the hardcoded render calls AND the hardcoded burn check functions. See "Ground Effects as Entities" in the Update Pipeline section.

### Ship (Player Entity)
Ship is a single entity. Its renderable gets bloom and light passes like any other entity. Player subs (sub_pea, sub_aegis, etc.) that produce light/bloom register as globals since they're module-level singletons.

### Dormant Entities
`Entity_render_pass()` **must** check `SpatialGrid_is_active()` and skip dormant entities. This is a core requirement of the refactor, not a follow-up optimization. Today `Entity_render_system()` does not filter by dormancy — it renders every entity, wasting vertex submission on things well off screen. The refactor fixes this: one `is_active` check in `Entity_render_pass()` gates all passes (main, bloom, light, overlay) for dormant entities. No bloom halos computed for enemies three screens away. The active region (3x3 buckets × 64×64 cells) is significantly larger than the viewport, so there is no risk of visible pop-in at bucket boundaries.

### Weapon Bloom Pass
Currently named `disint_bloom` and only used by sub_disintegrate, but the FBO itself is completely generic — it's just a bloom pipeline (capture → blur → composite). Nothing about it is disintegrate-specific. The disintegrate beam looks purple because it emits purple geometry, not because the FBO is configured for purple. A second beam weapon rendering green geometry into the same FBO would bloom green, composited alongside the purple, sharing the same blur treatment.

The refactor renames this to `weapon_bloom` and exposes it as `RENDER_PASS_WEAPON_BLOOM`. Any beam weapon (disintegrate, inferno, future beams) registers for this pass. Visual identity comes from what each weapon emits (color, brightness), not from FBO settings. Blur radius and intensity are tuned once for beam weapons as a class — they're properties of how light spreads, not of any individual weapon.

---

# Update Pipeline Refactor

## Problem

`Mode_Gameplay_update()` has **~17 hardcoded module-specific update calls** alongside the entity system. The entity system works correctly — `Entity_ai_update_system()` and `Entity_user_update_system()` iterate entities and call their callbacks. But module-level singleton pools (projectile pools, fire pools, corridors, footprints) are updated via explicit calls in mode_gameplay.c, same pattern that bloated the render pipeline.

### Current Pain Points
- **17 hardcoded calls** for pool/effect updates, growing with every new effect type
- **Implicit ordering contract**: projectile pools update before collision, effect pools after — enforced only by line position in a 200-line function
- **Burn checks masquerading as updates**: `Seeker_check_corridor_burn_player()` is collision detection, but corridors aren't entities so it's jammed into the update loop
- **Tight coupling**: mode_gameplay.c must `#include` every module that has a pool or effect

### How We Got Here

Same story as render. Projectile pools aren't entities — they're arrays inside hunter/stalker modules. When they needed per-frame updates, the expedient path was an explicit call in mode_gameplay.c. Then fire pools, corridors, footprints — all the same shortcut. The entity update system was right there, but these singletons didn't fit the per-entity model, so they got wired in by hand.

### What's Already Clean

- **Entity AI updates** — fully component-driven via `Entity_ai_update_system()`
- **Entity user updates** — fully component-driven via `Entity_user_update_system()`
- **Collision** — fully component-driven via `Entity_collision_system()` with spatial grid filtering, layer masks, and deferred resolution. No changes needed.
- **System-level updates** — `PlayerStats_update()`, `View_update()`, `Progression_update()`, `Zone_update_notification()`, etc. are game systems, not entities. They stay explicit, same as background bloom and UI rendering stay explicit on the render side.

## Proposed Architecture

### Two Update Phases

The current call sequence has a strict ordering contract around collision:

```
[things move]          →  collision checks stale positions if these run after
Entity_ai_update_system()
Entity_collision_system()
[effects tick]         →  timers and ground damage run after overlaps resolved
```

Two explicit phases capture this:

```c
typedef void (*GlobalUpdateFunc)(const unsigned int ticks);

void GlobalUpdate_register_pre_collision(GlobalUpdateFunc func);
void GlobalUpdate_register_post_collision(GlobalUpdateFunc func);
```

### Update Pipeline (mode_gameplay.c)

```c
// Systems (explicit, not registered)
Skillbar_update(input, ticks);
PlayerStats_update(ticks);
Burn_clear_registrations();

// Entity updates (ECS, already correct)
Entity_user_update_system(&input, ticks);

// Pre-collision: things that need to move before overlap checks
GlobalUpdate_pre_collision(ticks);      // projectile pools move
Entity_ai_update_system(ticks);         // entity AI moves

// Collision (ECS, already correct)
Entity_collision_system();

// Post-collision: effects tick, ground damage checks
GlobalUpdate_post_collision(ticks);     // corridors, footprints, fire pools, burn checks

// Systems (explicit, not registered)
Portal_update_all(ticks);
Savepoint_update_all(ticks);
View_update(input, ticks);
// ... etc
```

### Registration Examples

```c
// hunter.c — projectile pool moves before collision
GlobalUpdate_register_pre_collision(Hunter_update_projectiles);

// seeker.c — corridors tick after collision
GlobalUpdate_register_post_collision(Seeker_update_corridors);
GlobalUpdate_register_post_collision(Seeker_check_corridor_burn_player);

// sub_cinder.c — fire pools tick after collision
GlobalUpdate_register_post_collision(Sub_Cinder_update_pools);
```

### Ground Effects as Entities

Corridors, footprints, fire pools, and auras are currently module-level arrays with hand-rolled lifetime management and hardcoded burn checks (`Seeker_check_corridor_burn_player()`, etc.). These burn checks are collision detection that bypasses the entity collision system because the effects aren't entities.

The refactor promotes ground effects to lightweight entities with `CollidableComponent`. This eliminates the burn check functions entirely — ground effect damage goes through `Entity_collision_system()` like everything else, getting layer masks, spatial filtering, and dormancy for free.

The "overhead" concern is minimal. The entity pool is a static array — `Entity_add` finds the next `empty == true` slot, `Entity_destroy` sets `empty = true`. This is the same work the module-level pools already do manually, just through the entity system instead of hand-rolled per module.

This means corridors, footprints, fire pools, and auras move OUT of the global registries and INTO per-entity render/collision passes. The global registries shrink to truly non-entity systems (map, burn particles, player sub effects).

### Collision Optimization Path

`Entity_collision_system()` is O(a²) where `a` is active entities in the spatial grid's 3×3 neighborhood. With ground effects as entities, `a` grows (corridors, footprints, fire pool tiles). The spatial grid already limits this to the player's vicinity, and layer masks prevent meaningless pair checks from triggering resolution — but the inner loop still iterates and checks masks for every pair.

If active entity counts grow large enough to impact frame time, the known optimization is **broad-phase spatial hashing within the active region** — subdivide the 3×3 bucket neighborhood into smaller collision cells and only check pairs that share a cell. This is a straightforward extension of the existing spatial grid, not a new architecture. Not needed today, but noted as the path forward if entity counts demand it.

### Callback Signature

```c
typedef void (*GlobalUpdateFunc)(const unsigned int ticks);
```

All registered update callbacks take `ticks`. This keeps registration uniform — no separate `void(*)(void)` variant.

## What Does NOT Change

- **Entity_ai_update_system()** — already correct, no changes
- **Entity_user_update_system()** — already correct, no changes
- **Entity_collision_system()** — already correct, no changes
- **System-level updates** — `PlayerStats_update`, `View_update`, `Progression_update`, etc. stay explicit. They're game systems, not module pools.
- **Burn lifecycle** — `Burn_clear_registrations()` / `Burn_update_embers()` frame bookends stay explicit. They're frame-level lifecycle, not per-module pools.

---

# Combined Migration Strategy

### Phase 1: Infrastructure
- Add `passes[]` array to RenderableComponent
- Keep existing `render` field as `passes[RENDER_PASS_MAIN]` (source-compatible)
- Add `Entity_render_pass(RenderPass)` function to entity.c (with dormancy filtering)
- Create `global_render.c/h` — GlobalRender_register, GlobalRender_pass
- Create `global_update.c/h` — GlobalUpdate_register_pre_collision, GlobalUpdate_register_post_collision, GlobalUpdate_pre_collision, GlobalUpdate_post_collision

### Phase 2: Migrate Entity Render Passes
- For each entity type (mine, hunter, seeker, defender, stalker, corruptor):
  - Add bloom/light callbacks to its RenderableComponent
  - Remove the explicit call from mode_gameplay.c
- Ship gets the same treatment

### Phase 3: Migrate Global Render Sources
- Register non-entity render sources (burn particles, map bloom, player sub effects) via GlobalRender_register
- Remove their explicit calls from mode_gameplay.c

### Phase 4: Promote Ground Effects to Entities
- Corridors, fire pools, footprints, auras become lightweight entities
- Each gets RenderableComponent (main + bloom + light passes) and CollidableComponent (burn damage)
- Remove hardcoded burn check functions (`Seeker_check_corridor_burn_player`, etc.)
- Remove hardcoded update/render calls from mode_gameplay.c

### Phase 5: Migrate Update Pipeline
- Register projectile pool updates as pre-collision (Hunter, Stalker)
- Register remaining non-entity pool updates via GlobalUpdate
- Remove their explicit calls from mode_gameplay.c

### Phase 6: Cleanup
- Remove unused `#include` directives from mode_gameplay.c
- mode_gameplay.c render function: ~40 lines (scene setup, pass loops, UI, flip)
- mode_gameplay.c update function: explicit systems + two GlobalUpdate calls bracketing collision

## Expected Result

### Render Pipeline

mode_gameplay.c render function goes from ~220 lines / 89 calls to ~40 lines:

```
Background bloom (scene-level, stays explicit)
Grid + Map + CircuitAtlas + MapReflect (scene-level, stays explicit)
Light FBO:     Entity_render_pass(LIGHT) + GlobalRender_pass(LIGHT)
Weapon bloom:  Entity_render_pass(WEAPON_BLOOM) + GlobalRender_pass(WEAPON_BLOOM)
Main bloom:    Entity_render_pass(BLOOM) + GlobalRender_pass(BLOOM)
Overlays:      Entity_render_pass(OVERLAY) + GlobalRender_pass(OVERLAY)
Entities:      Entity_render_pass(MAIN)
God mode debug (stays explicit)
UI pass (stays explicit)
```

### Update Pipeline

mode_gameplay.c update function drops ~17 hardcoded calls:

```
Systems (explicit): Skillbar, PlayerStats, Burn_clear
Entity_user_update_system (ECS, unchanged)
GlobalUpdate_pre_collision(ticks)           // projectile pools
Entity_ai_update_system (ECS, unchanged)
Entity_collision_system (ECS, unchanged)
GlobalUpdate_post_collision(ticks)          // corridors, footprints, fire pools, burn checks
Systems (explicit): Portal, Savepoint, View, Progression, etc.
```

### Adding New Content

- New entity type = set its RenderableComponent passes at creation. Zero changes to mode_gameplay.c.
- New sub effect with pools = `GlobalRender_register()` + `GlobalUpdate_register_post_collision()` in its init. Zero changes to mode_gameplay.c.
- New projectile pool = `GlobalUpdate_register_pre_collision()` in its init. Zero changes to mode_gameplay.c.
