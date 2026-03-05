# Burn Visual Upgrade Spec

## Problem

Burn visuals are embarrassingly weak: 2-8 CPU-batched `Render_quad()` calls per entity with a fire color gradient. No bloom, no map lighting, no embers. Compare to inferno (256 GPU-instanced blobs + bloom + weapon lighting) or disintegrate (3-layer particles + dedicated bloom FBO). The gap is massive.

This spec upgrades burn to use the same GPU-instanced particle infrastructure as our best effects, with bloom glow, map lighting, drifting embers, and visual intensity that scales with stack count.

---

## Current State (burn.c/burn.h)

**BurnState** (embedded in every enemy + player singleton):
```c
typedef struct {
    int stacks;                        // 0-3 active stacks
    int duration_ms[BURN_MAX_STACKS];  // per-stack timers
} BurnState;
```

**Current visuals** (`Burn_render(BurnState*, Position)`):
- `1 + stacks` flame kernels per frame (2-4 total)
- Each kernel = 2 overlapping rotated quads (primary + 55° offset secondary)
- Hash-based pseudo-random jitter seeded by position + index
- `fire_color()` gradient: white->yellow->orange->dark red->fade over 400-600ms cycle
- Rising motion (up to 12 world units), size shrink over lifetime
- Rendered via CPU-batched `Render_quad()` into the main triangle buffer
- **No bloom, no map light, no embers**

**Integration**: Each enemy render function calls `Burn_render(&e->burn, pos)` inline. Player has `Burn_render_player(pos)` but it's never called.

---

## Architecture: Centralized Registration + Batch Rendering

**Key insight**: The current inline-render approach (one `Burn_render()` per enemy) works for CPU batching but is wrong for GPU instancing — you'd get one `glDrawArraysInstanced()` call per enemy with 5-25 instances each, which is inefficient. Instead, burn should collect all active burn positions into a single batch and issue one draw call per pass.

**Pattern**: Registration during update -> batch render during render.

### Registration System (internal to burn.c)

```c
#define BURN_MAX_REGISTERED  64    // generous for active set
#define BURN_EMBERS_PER_ENTITY 6   // max tracked embers per burning entity

typedef struct {
    bool active;
    float x, y;          // world position (drifts away from entity)
    float vx, vy;        // velocity
    int age_ms;          // time since spawn
    int ttl_ms;          // total lifetime (300-600ms randomized)
    float size;           // base size (2-4 px)
    float rotation;       // visual spin
} BurnEmber;

typedef struct {
    bool active;
    int stacks;
    Position pos;
    BurnEmber embers[BURN_EMBERS_PER_ENTITY];
    int emberSpawnTimer;  // ms countdown to next ember spawn
} BurnRegistration;
```

**Why 64 max**: Spatial grid active set = 3x3 buckets. Realistically only a handful of enemies burn simultaneously. 64 is generous headroom.

**Why 6 embers per entity**: At 3 stacks, spawn interval is ~80ms, lifetime 300-600ms -> 4-6 live embers at steady state. 6 slots covers it.

---

## Particle Design

### Flames (Procedural — No Stored State)

Computed from time + position seed each frame. Same approach as current `Burn_render()` but outputting `ParticleInstanceData` for GPU instancing instead of `Render_quad()`.

Each flame kernel produces 2 overlapping quads (primary + secondary at offset rotation), matching inferno's dual-quad pattern.

**Per-entity flame count by stacks**:

| Stacks | Flame Kernels | Instances (x2 quads) |
|--------|---------------|----------------------|
| 1      | 3             | 6                    |
| 2      | 5             | 10                   |
| 3      | 8             | 16                   |

**Flame behavior**:
- Pseudo-random jitter: +/-(6 + stacks*2) px horizontal, +/-75% of that vertical
- Rising motion: phase-driven upward drift (10 + stacks*3 px over lifetime)
- Rotation: continuous spin via hash(time + seed)
- Size: base (8 + stacks*3) px, shrinks 40% over lifetime
- Lifetime: 400-600ms per kernel (hash-randomized)
- Primary quad: full size + color. Secondary: 60% size, +/-55 degree rotation offset, slightly darker

### Embers (Tracked — Simulated Physics)

Small bright sparks that break away and drift. These need velocity + lifetime tracking because they move independently of the entity.

**Ember behavior**:
- Spawn at entity position + small random offset
- Velocity: upward 30-60 px/sec + lateral kick +/-20 px/sec
- Lateral drag: 0.95x/sec (embers curve, don't fly straight)
- Spin: 180 degrees/sec rotation
- Lifetime: 300-600ms (randomized)
- Size: 2-4px at spawn, shrinks 60% over lifetime
- Rendered as soft circles (ParticleInstance with `soft_circle=true`)

**Spawn rate by stacks**:

| Stacks | Spawn Interval | Steady-State Live Embers |
|--------|----------------|--------------------------|
| 1      | 200ms          | 1-2                      |
| 2      | 120ms          | 2-4                      |
| 3      | 80ms           | 4-6                      |

---

## Color Palette

### Flames (reuse existing `fire_color()`)

| Phase    | R    | G         | B         | A        | Look             |
|----------|------|-----------|-----------|----------|------------------|
| 0.0-0.2  | 1.0  | 1.0       | 1.0->0.3  | 0.9      | White-hot center |
| 0.2-0.5  | 1.0  | 1.0->0.7  | 0.3->0.1  | 0.9      | Bright yellow    |
| 0.5-0.8  | 1.0  | 0.7->0.35 | 0.1       | 0.8->0.6 | Orange           |
| 0.8-1.0  | 1.0->0.6 | 0.35->0.1 | 0.1->0.05 | 0.6->0.1 | Dark red, fading |

### Embers

| Phase    | R    | G         | B         | Look              |
|----------|------|-----------|-----------|-------------------|
| 0.0-0.3  | 1.0  | 1.0       | 0.7       | White-yellow spark |
| 0.3-0.6  | 1.0  | 1.0->0.7  | 0.7->0.2  | Cooling yellow     |
| 0.6-1.0  | 1.0  | 0.7->0.4  | 0.2->0.05 | Orange dying spark |

Alpha: quadratic falloff `1.0 - t^2` (bright on spawn, aggressive fade at end).

### Map Light Color
- `(1.0, 0.5, 0.1)` — deep warm orange
- Distinct from inferno's `(1.0, 0.6, 0.15)` yellow-orange — burn is a smoldering DOT, not a flamethrower

---

## Stack Scaling Summary

| Property              | 1 Stack (Smolder) | 2 Stacks (Burning) | 3 Stacks (Engulfed) |
|-----------------------|-------------------|--------------------|--------------------|
| Flame kernels         | 3                 | 5                  | 8                  |
| Flame instances       | 6                 | 10                 | 16                 |
| Ember spawn rate      | 200ms             | 120ms              | 80ms               |
| Live embers (steady)  | 1-2               | 2-4                | 4-6                |
| **Total instances**   | **~8**            | **~14**            | **~22**            |
| Flame base size (px)  | 11                | 14                 | 17                 |
| Flame jitter range    | +/-8              | +/-10              | +/-12              |
| Flame rise distance   | 13px              | 16px               | 19px               |
| Light radius          | 160               | 200                | 240                |
| Light alpha           | 0.45              | 0.60               | 0.75               |

Visual difference between 1 and 3 stacks should be immediately obvious: 1 = a few flickering wisps, 3 = roaring engulfed entity with orange glow on nearby walls.

---

## Rendering Pipeline

### Instance Budget

| Component     | Per entity (3 stacks) | 8 entities burning | Notes |
|---------------|-----------------------|-------------------|-------|
| Flames        | 16                    | 128               | 2 quads x 8 kernels |
| Embers        | 6                     | 48                | Soft circles |
| **Total**     | **22**                | **176**           | Well under inferno's 512 |

Static buffer: `ParticleInstanceData burnInstances[512]` (~18KB).

### Three Render Passes

**1. Main pass** (`Burn_render_all`) — after entity system flush:
- Two `ParticleInstance_draw()` calls, both with `soft_circle=true`:
  - Flames: round organic fire look (smooth falloff on overlapping quads)
  - Embers: round sparks
- Color/size at 1.0x (no boost)
- Note: Using soft_circle for flames is a deliberate departure from inferno's sharp quads. If this looks good, inferno will be retrofitted with the same approach.

**2. Bloom source pass** (`Burn_render_bloom_source`) — in foreground bloom FBO:
- Same two draws (both `soft_circle=true`) with boosted values:
  - Flames: color x1.4, size x1.3 (slightly subtler than inferno's 1.5x — burn is a DOT, not a weapon)
  - Embers: color x1.5, size x1.2 (embers are the bright accent, glow hard)

**3. Light source pass** (`Burn_render_light_source`) — in light_bloom FBO:
- One `Render_filled_circle()` per burning entity (batched by renderer)
- Radius: 120 + stacks*40 (160/200/240)
- Alpha: 0.3 + stacks*0.15 (0.45/0.60/0.75)
- Color: warm orange (1.0, 0.5, 0.1)

Reference — inferno light: 210px radius, alpha 0.7. Burn at max is similar but slightly smaller.

---

## New API (burn.h)

```c
/* Registration — call from enemy update when burning */
void Burn_register(const BurnState *state, Position pos);

/* Batch rendering — call once per pass from mode_gameplay.c */
void Burn_render_all(void);
void Burn_render_bloom_source(void);
void Burn_render_light_source(void);

/* Frame lifecycle — call from mode_gameplay update */
void Burn_update_embers(unsigned int ticks);
void Burn_clear_registrations(void);
```

Old `Burn_render()` becomes a no-op (removed from all entity render functions).

---

## Integration Points

### mode_gameplay.c — Update Phase

```
Burn_clear_registrations()       <- start of update
  ... enemy updates (each calls Burn_register() if burning) ...
  ... Burn_update_player() internally registers player burn ...
Burn_update_embers(ticks)        <- end of update, after all registrations
```

### mode_gameplay.c — Render Phase

```
[light bloom pass]
  Burn_render_light_source()     <- add alongside other light sources

[main render, after Entity_render_system + Render_flush]
  Burn_render_all()              <- after entity flush so fire renders on top

[foreground bloom pass]
  Burn_render_bloom_source()     <- add alongside other bloom sources
```

### Enemy Files (hunter.c, seeker.c, stalker.c, defender.c, corruptor.c)

**Add** in update function (near existing `Burn_tick_enemy()` call):
```c
if (Burn_is_active(&e->burn))
    Burn_register(&e->burn, placeable->position);
```

**Remove** from render function:
```c
Burn_render(&e->burn, placeable->position);  // DELETE
```

### Player (burn.c internal)

`Burn_update_player()` internally calls `Burn_register(&playerBurn, pos)` so player burn gets the same visual treatment.

---

## Files Modified (Implementation)

| File | Change |
|------|--------|
| `src/burn.h` | New function signatures, keep BurnState unchanged |
| `src/burn.c` | Registration system, ember simulation, GPU-instanced rendering, bloom/light source functions. New includes: `particle_instance.h`, `graphics.h` |
| `src/mode_gameplay.c` | Wire lifecycle (clear/update_embers in update, render_all/bloom/light in render) |
| `src/hunter.c` | Add Burn_register in update, remove Burn_render from render |
| `src/seeker.c` | Same |
| `src/stalker.c` | Same |
| `src/defender.c` | Same |
| `src/corruptor.c` | Same |

---

## Gotchas

- **Render_flush timing**: `Burn_render_all()` uses its own shader (ParticleInstance). The batch renderer must be flushed before. The existing `Render_flush()` after `Entity_render_system()` handles this.
- **Bloom FBOs have no stencil**: Fine — burn bloom is just additive color, no stencil needed.
- **Dormant enemies**: Skip AI updates -> won't call `Burn_register()` -> won't render burn. Correct — they're off-screen.
- **Registration timing**: Registrations happen during update, rendering during render. Position is post-movement, same frame. Correct.
- **Player `Burn_render_player()`**: Currently declared but never called. Becomes obsolete — registration pattern handles player burn automatically.

---

## Verification

1. `make compile` — zero warnings
2. Apply burn to enemies with flak/inferno, verify:
   - Flames visible and scale with stack count (1->3 stacks = smolder->engulfed)
   - Embers drift upward and outward, fade correctly
   - Bloom glow visible around burning entities
   - Orange light casts on nearby map cells
   - Player burn (from fire enemies) has same visual treatment
   - Soft circle rendering gives organic round fire look
3. Stress test: 5+ enemies burning at 3 stacks simultaneously — no FPS drop
4. Verify burn still deals correct damage (10 DPS/stack, 3s duration) — visual-only change
