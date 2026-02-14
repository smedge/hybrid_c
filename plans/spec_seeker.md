# Seeker — Dash-Kill Enemy Type

## Context

The second active enemy. Where the hunter is a ranged combatant that patrols and shoots from distance, the seeker is a predatory stalker that tracks you down and uses an egress-style dash to try to one-shot you. It's terrifying — you hear it homing in, you see it circling, and then it launches.

Killing seekers drops seeker fragments that unlock sub_egress (currently free, needs to be gated). This creates a satisfying progression loop: survive seekers long enough to learn their dash ability and use it yourself.

## Design

### Seeker Behavior

| Property | Value |
|---|---|
| Speed (stalking) | 300 units/sec (slower than hunter, deliberate) |
| Speed (orbiting) | 500 units/sec (circling at strike range) |
| Aggro range | 1000 units |
| De-aggro range | 1500 units |
| LOS required | Yes (same as hunter) |
| HP | 60 |
| Death | 2 sub_pea shots, 3 sub_mgun shots, 1 sub_mine |

**State machine**: `IDLE → STALKING → ORBITING → WINDING_UP → DASHING → RECOVERING → (repeat STALKING) → DYING → DEAD`

- **IDLE**: Slow random drift around spawn point, same pattern as hunter idle. Slightly larger wander radius (500 units) — seekers patrol wider.
- **STALKING**: Homing toward player at 300 u/s. Doesn't fire, doesn't attack. Closing the gap. Transitions to ORBITING when within strike range (~250 units).
- **ORBITING**: Circles the player at ~250 unit radius, speed 500 u/s. Erratic, hard to track. Lasts 1-2 seconds (randomized). Builds tension — the player knows the dash is coming. Then transitions to WINDING_UP.
- **WINDING_UP**: Stops moving. Brief telegraph (300ms) — the seeker flashes white/bright, giving the player a split-second to react. Locks aim direction at start of windup (aims where player IS, not where they'll be — this is the dodge window).
- **DASHING**: Egress-style burst — 5000 u/s in a straight line for 150ms. Covers ~750 units. If it hits the player during the dash, deals 80 damage (near-lethal). The dash direction is locked from windup — dodging sideways is the counter.
- **RECOVERING**: Post-dash cooldown, 2 seconds. The seeker drifts/decelerates from dash momentum. Vulnerable — this is the punish window. Returns to STALKING after recovery.
- **DYING**: Brief death animation (200ms flash), same pattern as hunter.
- **DEAD**: Respawn timer (30s, same as hunter).

### Damage Model

| Source | Damage |
|---|---|
| Seeker dash (contact) | 80 |
| sub_pea vs seeker | 50 (2 shots to kill) |
| sub_mgun vs seeker | 20 (3 shots to kill) |
| sub_mine vs seeker | 60+ (1 shot kill) |

- Seeker HP: 60 — glass cannon. Deadly but fragile.
- Player integrity: 100 → one dash hit leaves you at 20 HP, a second is death. Regen between encounters is critical.
- Seeker does NO ranged damage. Pure melee/dash threat.

### Collision

- Dash contact: check player bounding box against seeker position during DASHING state. On hit, call `PlayerStats_damage(80.0)`.
- Non-dash contact: solid collision (pushes player) but no damage. The seeker bumping you while orbiting is annoying, not lethal.
- Player projectiles: same `Sub_Pea_check_hit()` / `Sub_Mgun_check_hit()` / `Sub_Mine_check_hit()` pattern as hunter.
- Near-miss aggro: same 200-unit proximity check as hunter (`Sub_Pea_check_nearby`, `Sub_Mgun_check_nearby`).

### Visual Design

- **Body**: Green elongated diamond / needle shape, pointed along facing direction. Narrow and sharp — reads as fast and dangerous.
- **Idle**: Dim green, slow pulse.
- **Stalking**: Brighter green, steady glow.
- **Orbiting**: Bright green with motion trail showing the circular path.
- **Winding up**: Flashes white rapidly (telegraph).
- **Dashing**: Bright white streak with thick motion trail — unmistakable visual of the dash.
- **Recovering**: Dim, fading trail. Visually reads as "vulnerable."
- **Death**: White spark flash (same style as hunter/mine).
- **Bloom**: Body in all active states + dash trail rendered into bloom FBO.

### Fragment Type

New `FRAG_TYPE_SEEKER` — green colored binary glyphs.

### Audio

- **Stalking**: Could use a subtle ambient hum/drone when aggro'd (stretch goal, not required).
- **Windup**: Short rising tone or click — audio telegraph matching the visual flash.
- **Dash**: Whoosh/burst sound (could reuse or pitch-shift an existing sample).
- **Hit player**: samus_hurt via PlayerStats_damage.
- **Take damage**: samus_hurt spark (same as hunter hit feedback).
- **Death**: bomb_explode (same as hunter).
- **Respawn**: door.wav (same as hunter).

## Changes

### 1. Add fragment type
**File**: `src/fragment.h`
- Add `FRAG_TYPE_SEEKER` to `FragmentType` enum (before `FRAG_TYPE_COUNT`)

**File**: `src/fragment.c`
- Add color entry in `typeInfo[]`: green `{0.0f, 1.0f, 0.2f, 1.0f}`

### 2. Fix sub_egress progression (no longer free)
**File**: `src/progression.c`
- Change sub_egress entry: threshold 0 → 3, frag type → `FRAG_TYPE_SEEKER`

### 3. Create `src/seeker.h`
Public API (same pattern as hunter):
```c
void Seeker_initialize(Position position);
void Seeker_cleanup(void);
void Seeker_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Seeker_render(const void *state, const PlaceableComponent *placeable);
void Seeker_render_bloom_source(void);
Collision Seeker_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Seeker_resolve(const void *state, const Collision collision);
void Seeker_deaggro_all(void);
```

### 4. Create `src/seeker.c`
Follow hunter.c pattern:
- Static arrays: `SeekerState seekers[]`, `PlaceableComponent placeables[]`
- Shared singleton components (renderable, collidable, aiUpdatable)
- `COLLISION_LAYER_ENEMY`, `mask = COLLISION_LAYER_PLAYER`
- No projectile pool (seekers are melee) — dash hit check is against player bounding box
- AI logic: distance + LOS → state transitions → orbiting math → windup telegraph → dash burst → recovery
- Fragment drop on death: `Fragment_spawn(pos, FRAG_TYPE_SEEKER)` when killed by player and sub_egress not yet unlocked
- Wall avoidance in stalking/orbiting (same `Map_line_test` pattern)
- Dash should NOT clip through walls — check `Map_line_test_hit` along dash path, stop at wall if hit

### 5. Wire zone spawn
**File**: `src/zone.c`
- Add `#include "seeker.h"`
- Add `Seeker_cleanup()` in `Zone_unload()` and `apply_zone_to_world()`
- Add `else if (strcmp(sp->enemy_type, "seeker") == 0)` spawn case

### 6. Wire bloom rendering
**File**: `src/mode_gameplay.c`
- Add `#include "seeker.h"`
- Call `Seeker_render_bloom_source()` in bloom pass

### 7. Wire deaggro on death
**File**: `src/ship.c`
- Add `#include "seeker.h"`
- Call `Seeker_deaggro_all()` alongside `Hunter_deaggro_all()` in unified death check

### 8. Add seeker spawns to test zone
**File**: `resources/zones/zone_001.zone`
- Add a few `spawn seeker <x> <y>` lines

## Key Gameplay Interactions

- **Hunter + Seeker together**: The hunter forces you to dodge sideways (avoiding bursts), while the seeker punishes predictable lateral movement with its dash. Together they create a deadly crossfire where you can't just strafe.
- **Sub_egress as counter**: Once unlocked, the player's own dash becomes the natural counter to the seeker's dash — dash sideways at the last moment to dodge, then punish during recovery.
- **Sub_mine as trap**: Drop a mine in the seeker's orbit path. When it dashes through, the mine detonates and one-shots it.
- **Risk/reward**: Seekers are glass cannons (60 HP). If you can land shots during the orbit/recovery window, they die fast. But miss the dodge window and you're nearly dead.

## Dodge Window Analysis

- Windup telegraph: 300ms (visual + audio cue)
- Dash covers 750 units in 150ms
- Player normal speed: 800 u/s → in 300ms (windup) the player moves ~240 units laterally
- Dash width: seeker bounding box is ~20 units wide
- **Result**: If the player starts moving perpendicular during windup, they clear the dash path comfortably. The skill expression is *reacting to the telegraph* and *choosing the right dodge direction*.

## Verification

- `make compile` — clean build
- Add seeker spawns to zone_001, launch game
- Approach seeker — should start stalking at ~1000 units
- Seeker circles you, winds up (white flash), then dashes
- Dodge sideways during windup — dash misses
- Get hit by dash — 80 damage, red flash + hurt sound
- 2 sub_pea shots kill seeker → death spark + fragment drop
- Collect 3 seeker fragments → sub_egress unlocks
- Seeker doesn't dash through walls
- Seeker loses aggro behind walls
- Seeker de-aggros on player death
