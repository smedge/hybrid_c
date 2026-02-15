# Defender — Support Enemy Type

## Context

The third active enemy. Where hunters attack from range and seekers dash-kill at close range, the **Defender** is a support program that keeps them alive. It heals wounded allies and shields itself with an aegis when threatened. This creates the classic "kill the healer first" dynamic — leave a defender alive and the hunters/seekers you're fighting keep getting topped off.

Killing defenders randomly drops one of two fragment types: `FRAG_TYPE_MEND` or `FRAG_TYPE_AEGIS`. These unlock `sub_mend` (instant 50 HP heal for the player) and `sub_aegis` (10s invulnerability shield, 30s cooldown) — the first healing and shield subroutines.

## Design

### Defender Behavior

| Property | Value |
|---|---|
| Speed (normal) | 250 units/sec (slow, not a fighter) |
| Speed (fleeing) | 400 units/sec (trying to survive) |
| Aggro range | 1200 units (same ballpark as hunter) |
| De-aggro range | 3200 units (standard) |
| Heal range | 800 units (8 cells — heals from a distance) |
| Heal amount | 50 HP per heal |
| Heal cooldown | 4 seconds |
| LOS required | Yes (same as hunter/seeker) |
| HP | 80 |
| Death | 2 sub_pea shots, 4 sub_mgun shots, 1 sub_mine |
| Color | Light blue |

**State machine**: `IDLE → SUPPORTING → FLEEING → DYING → DEAD`

- **IDLE**: Slow random drift around spawn point (400-unit wander radius, same as hunter). No allies in range or all allies at full HP. Checks for allies periodically.
- **SUPPORTING**: Allies detected within heal range. Stays within heal range of the nearest ally cluster. Moves toward wounded allies to maintain range. Heals the most wounded ally every 4 seconds (heal cooldown). Activates aegis shield when taking damage or player gets close (~400 units).
- **FLEEING**: Player is within 400 units or defender just took damage. Moves away from player at 400 u/s. Still heals allies while fleeing if any are in range. Transitions back to SUPPORTING once player is >600 units away.
- **DYING**: Brief death animation (200ms flash), same pattern as hunter/seeker.
- **DEAD**: Respawn timer (30s, standard).

### Aegis Shield (Enemy)

| Property | Value |
|---|---|
| Duration | 10 seconds |
| Cooldown | 30 seconds |
| Trigger | Taking damage, or player within 400 units |
| Effect | All incoming damage absorbed (Sub_Pea, Sub_Mgun, Sub_Mine hits register but deal 0 damage) |
| Visual | Bright light-blue circle/ring pulsing around the defender |
| Audio | Distinct activation sound |

The shield creates a timing puzzle: you can't just focus-fire the defender while it's shielded. You either wait 10 seconds and punish during the 30s cooldown window, or you ignore the defender and kill its allies first (but they keep getting healed). This forces target prioritization decisions.

### Healing (Enemy)

- Every 4 seconds, the defender selects the most wounded alive ally (hunter or seeker) within 800 units.
- Heals that ally for 50 HP (capped at their max HP).
- Visual: light-blue line/beam briefly connecting defender to healed ally, plus a flash on the healed target.
- Audio: subtle heal sound effect.
- The defender does NOT heal mines (they're passive traps, not active combatants).
- The defender does NOT heal other defenders (no chain-healing).

### Damage Model

| Weapon | Damage |
|---|---|
| sub_pea vs defender | 50 (2 shots to kill without shield) |
| sub_mgun vs defender | 20 (4 shots to kill without shield) |
| sub_mine vs defender | 100 (instant kill without shield) |
| All weapons vs shielded defender | 0 (absorbed) |

### Collision

- **Body**: Solid collision with player (pushes, like hunter). No contact damage.
- **Player projectiles**: Same `Sub_Pea_check_hit()` / `Sub_Mgun_check_hit()` / `Sub_Mine_check_hit()` pattern. When shielded, hits register (spark + sound) but deal 0 damage.
- **Near-miss aggro**: Same 200-unit proximity check as hunter/seeker.
- **Getting hit while idle**: Immediate transition to FLEEING + aegis activation if available.

### Visual Design

- **Body**: Light blue hexagon shape (6-sided, defensive/protective geometry). Flat side forward.
- **Idle**: Dim light blue, slow pulse.
- **Supporting**: Brighter blue, steady glow. Occasional heal beam to allies.
- **Fleeing**: Same brightness, faster pulse.
- **Shielded**: Bright pulsing light-blue ring/circle around the body. Unmistakable "I'm invulnerable" visual.
- **Heal beam**: Brief thick line from defender to target, light blue, fades over ~200ms.
- **Death**: White spark flash (standard).
- **Bloom**: Body + shield ring + heal beams rendered into bloom FBO.

### Fragment Types

Two new fragment types, randomly dropped:
- `FRAG_TYPE_MEND` — light blue binary glyphs
- `FRAG_TYPE_AEGIS` — light cyan/white binary glyphs

On death, the defender drops ONE fragment, randomly chosen between the two types (50/50). This means unlocking both subs requires killing ~20 defenders on average (5 threshold each, random drops).

### Player Subroutines

#### sub_mend (Healing type)
| Property | Value |
|---|---|
| Type | SUB_TYPE_HEALING |
| Activation | Press assigned key |
| Effect | Instantly heals 50 integrity |
| Feedback cost | 20 |
| Cooldown | 10 seconds |
| Cap | Won't heal above max integrity (100) |
| Fragment source | FRAG_TYPE_MEND |
| Threshold | 5 fragments |

#### sub_aegis (Shield type)
| Property | Value |
|---|---|
| Type | SUB_TYPE_SHIELD |
| Activation | Press assigned key |
| Effect | Invulnerable to all damage for 10 seconds |
| Feedback cost | 30 |
| Cooldown | 30 seconds |
| Visual | Bright cyan ring around player ship |
| Fragment source | FRAG_TYPE_AEGIS |
| Threshold | 5 fragments |

### Audio

- **Heal beam**: Could use a pitched-up version of refill_start.wav or a new soft chime.
- **Aegis activate**: Distinct shield-up sound (statue_rise.wav pitched up?).
- **Aegis break** (timeout): Subtle shield-down sound.
- **Take damage (shielded)**: Different sound from normal hit — metallic/absorbed feel. Could use ricochet.wav.
- **Take damage (unshielded)**: samus_hurt spark (same as hunter/seeker).
- **Death**: bomb_explode (standard).
- **Respawn**: door.wav (standard).

## Changes

### 1. Add fragment types
**File**: `src/fragment.h`
- Add `FRAG_TYPE_MEND` and `FRAG_TYPE_AEGIS` to enum (before `FRAG_TYPE_COUNT`)

**File**: `src/fragment.c`
- Add color entries: `FRAG_TYPE_MEND` = light blue `{0.3f, 0.7f, 1.0f, 1.0f}`, `FRAG_TYPE_AEGIS` = light cyan `{0.6f, 0.9f, 1.0f, 1.0f}`

### 2. Add SUB_ID_MEND and SUB_ID_AEGIS
**File**: `src/progression.h`
- Add `SUB_ID_MEND` and `SUB_ID_AEGIS` to `SubroutineId` enum (before `SUB_ID_COUNT`)

**File**: `src/progression.c`
- Add entries: `[SUB_ID_MEND] = { "MEND", "sub_mend", FRAG_TYPE_MEND, 5, false }`
- `[SUB_ID_AEGIS] = { "AEGIS", "sub_aegis", FRAG_TYPE_AEGIS, 5, false }`

**File**: `src/skillbar.c`
- Add sub_registry entries with type, name, description
- Add icon rendering cases
- Add cooldown fraction cases

### 3. Add heal/query APIs to hunter and seeker
**File**: `src/hunter.h` — add:
```c
bool Hunter_find_wounded(Position from, double range, Position *out_pos, int *out_index);
void Hunter_heal(int index, double amount);
```

**File**: `src/seeker.h` — add:
```c
bool Seeker_find_wounded(Position from, double range, Position *out_pos, int *out_index);
void Seeker_heal(int index, double amount);
```

These let the defender query for wounded allies and heal them by index.

### 4. Create `src/sub_mend.h` and `src/sub_mend.c`
- Simple activated ability: press key → heal 50 integrity, 20 feedback, 5s cooldown
- Follow sub_egress pattern (state machine: READY → COOLDOWN)

### 5. Create `src/sub_aegis.h` and `src/sub_aegis.c`
- Activated ability: press key → invulnerable 10s, 30 feedback, 30s cooldown
- PlayerStats needs a shield check: `PlayerStats_is_shielded()` — when true, `PlayerStats_damage()` and `PlayerStats_force_kill()` do nothing
- Visual: render cyan ring around ship when active
- Follow sub_egress pattern for state machine

### 6. Create `src/defender.h`
```c
void Defender_initialize(Position position);
void Defender_cleanup(void);
void Defender_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Defender_render(const void *state, const PlaceableComponent *placeable);
void Defender_render_bloom_source(void);
Collision Defender_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Defender_resolve(const void *state, const Collision collision);
void Defender_deaggro_all(void);
void Defender_reset_all(void);
```

### 7. Create `src/defender.c`
Follow hunter.c/seeker.c pattern:
- Static arrays for state + placeables
- AI: find allies → stay near them → heal most wounded → flee from player → aegis when threatened
- Heal beam rendering (brief thick line to target)
- Shield ring rendering (pulsing circle around self)
- Random fragment drop: `rand() % 2 ? FRAG_TYPE_MEND : FRAG_TYPE_AEGIS`
- Standard hit detection, spark, deaggro, reset

### 8. Wire into zone, bloom, ship death/respawn
**File**: `src/zone.c` — add defender include, cleanup, spawn case
**File**: `src/mode_gameplay.c` — add defender bloom rendering
**File**: `src/ship.c` — add `Defender_deaggro_all()` and `Defender_reset_all()` calls
**File**: `src/savepoint.c` — add "mend" and "aegis" to frag_names arrays

### 9. Wire sub_mend and sub_aegis into ship update
**File**: `src/ship.c`
- Call `Sub_Mend_update()` and `Sub_Aegis_update()` in ship update
- Call `Sub_Mend_initialize()` / `Sub_Aegis_initialize()` in ship init
- Call cleanup functions

### 10. Add defender spawns to test zone
**File**: `resources/zones/zone_001.zone`
- Add `spawn defender <x> <y>` near existing hunter/seeker spawns

## Key Gameplay Interactions

- **Kill the healer**: If a defender is supporting hunters/seekers, you MUST prioritize it or the fight becomes attrition you can't win. Hunters get healed through your DPS.
- **Aegis timing**: The 10s shield forces you to either wait (dangerous with hunters/seekers shooting at you) or switch targets. The 30s cooldown is the punish window — you have 20 seconds of vulnerability to work with.
- **Sub_mine counter**: Drop a mine near the defender, wait for shield to expire, mine one-shots it through anything. But mines have limited count and cooldown.
- **Defender + Seeker combo**: The seeker dashes at you while the defender heals it between attacks. You need to dodge the seeker AND find windows to chip the defender.
- **Player sub_mend**: Gives the player sustain between fights. Critical for deeper exploration runs. 20 feedback cost means you can't spam it — you're trading offense capacity (feedback) for survival.
- **Player sub_aegis**: The ultimate "oh shit" button. 10s of invulnerability lets you escape or reposition, but 30s cooldown means you use it once per major fight. 30 feedback is expensive.

## Verification

- `make compile` — clean build
- Add defender spawns near hunters in zone_001
- Approach a defender near hunters — should stay near its allies
- Damage a hunter, watch defender heal it (blue beam visual)
- Shoot the defender — it activates aegis (blue ring), shots do 0 damage
- Wait for aegis to expire, kill the defender → death spark + random fragment (mend or aegis)
- Collect 3 mend fragments → sub_mend unlocks, heals 50 integrity on use
- Collect 3 aegis fragments → sub_aegis unlocks, 10s invulnerability on use
- Defender flees when player gets close
- Defender deaggros on player death, resets on respawn
- Defender doesn't heal mines or other defenders
