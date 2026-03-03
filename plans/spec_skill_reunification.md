# Skill Re-Unification Spec

## Principle

**The skill is the skill.** One config per core, used by everyone. No duplicate configs in player wrappers or enemy files. The only exception is stalker stealth (fundamentally different mechanic).

## Already Unified (this session)

- `sub_resist_core.c` — `SubResist_get_config()` — player + corruptor
- `sub_emp_core.c` — `SubEmp_get_config()` — player + corruptor
- `sub_sprint_core.c` — `SubSprint_get_config()` — player + corruptor

## Already Shared (prior work)

- Stalker uses `Sub_Pea_get_config()` for projectiles (same as player pea)
- Stalker uses `Sub_Egress_get_config()` for dash (same as player egress)

---

## 1. Projectile — Hunter shifts to sub_mgun

**Decision:** Hunter switches from sub_pea to sub_mgun. Mgun's rapid-fire / lower-damage profile is a natural fit for the hunter's burst-fire combat role.

**Current state:** Mgun config in `sub_mgun.c` (player), hunter config in `hunter.c` (separate). Mgun has no `get_config()` yet — needs one added, then hunter uses it.

| Field | Mgun (player) | Hunter (current) | Notes |
|-------|--------------|-----------------|-------|
| fire_cooldown_ms | 200 | 0 (burst logic) | Hunter uses burst timing (100ms between, 1.5s between bursts) |
| velocity | 3500 | 2000 | Mgun is 75% faster |
| ttl_ms | 1000 | 800 | |
| pool_size | 16 | 256 | Hunter pool shared across all hunters |
| damage | 35 | 15 | Mgun does 2.3x more per shot |
| color | white | orange | |
| light color | white | orange | |

**Balance implications of unifying to mgun values:**
- Hunter burst = 3 shots at 35 dmg = **105 per burst** (currently 45). That's a huge damage spike.
- Hunter projectiles become faster (3500 vs 2000) — harder to dodge.
- Hunter projectiles become white (same as player) — loses the orange visual identity.
- Hunter projectiles travel further (1000ms TTL at 3500 speed = 3500 range vs 1600 range currently).

**Questions to resolve:**
1. **Damage:** 35 per shot makes hunter bursts devastating (105 total). Lower mgun damage to something that works for both? Or accept that hunters become deadlier?
2. **Velocity:** 3500 is fast. Keep it, or lower to something that works for both player feel and dodge-ability?
3. **Color:** Should mgun projectiles be white for everyone, or should color be pulled out of the config and passed per-user? (Color as an entity concern, not a skill concern.)
4. **Pool size:** Hunter needs a larger shared pool (many hunters, simultaneous shots). Pool size could stay as an entity-level allocation rather than a config value.
5. **Fire rate:** Hunter's burst pattern (3 shots at 100ms intervals, 1.5s between bursts) is AI behavior, not skill config. The `fire_cooldown_ms` in the config is what the player uses for hold-to-fire. Hunter AI can ignore this and use its own burst timing. Is that acceptable, or should burst timing also come from the config?

---

## 2. Shield — sub_aegis vs Defender

**Current state:** Config in `sub_aegis.c` (player) and `defender.c` (separate).

| Field | Player (Aegis) | Defender | Notes |
|-------|---------------|----------|-------|
| duration_ms | 10000 | 10000 | Match |
| cooldown_ms | 30000 | 30000 | Match |
| break_grace_ms | 0 | 200 | Defender has 200ms grace to survive same-frame mine hits |
| ring_radius | 35.0 | 18.0 | Player ship is larger |
| ring_thickness | 2.0 | 1.5 | |
| pulse_speed | 6.0 | 5.0 | |
| pulse_alpha_min | 0.7 | 0.6 | |
| radius_pulse_amount | 0.05 | 0.0 | |
| bloom_thickness | 3.0 | 2.0 | |
| bloom_alpha_range | 0.3 | 0.0 | |
| light_radius | 90.0 | 0.0 | Defender shield has no light |

**Key question:** Gameplay values already match (10s/30s). The differences are all visual (defender is smaller entity, smaller ring makes sense) plus the 200ms break grace. Should the visual ring scale to the entity automatically rather than being hardcoded? Should break_grace be unified (give player 200ms too, or remove it from defender)?

**Decision:** Defender keeps 200ms break grace (protects against mine same-frame damage). Player stays at 0. This means `break_grace_ms` is a per-user override, not part of the shared config. Visual fields (ring_radius, thickness, bloom) also stay per-user since they depend on entity size.

**Implementation approach:** Move shared gameplay values (duration_ms, cooldown_ms, colors) to core via `SubShield_get_config()`. The config struct keeps break_grace_ms and visual fields, but callers can override break_grace_ms when constructing their local config. Or: split into gameplay config (shared in core) and visual config (per-user). TBD on cleanest pattern.

---

## 3. Heal — sub_mend vs Defender

**Current state:** Config in `sub_mend.c` (player) and `defender.c` (separate).

| Field | Player (Mend) | Defender | Notes |
|-------|-------------|----------|-------|
| heal_amount | 50.0 | 50.0 | Match |
| cooldown_ms | 10000 | 4000 | Defender heals 2.5x more often |
| beam_duration_ms | 0 | 200 | Player has no beam (instant self-heal) |
| beam_thickness | 0.0 | 2.5 | |
| beam_color | 0,0,0 | 0.3, 0.7, 1.0 | |
| bloom_beam_thickness | 0.0 | 3.0 | |

**Key question:** Player mend is a self-heal (instant, no beam). Defender mend is a targeted ally-heal (has a visible beam connecting to target). The beam fields are zero on the player because there's no target to beam to. But the cooldown difference is a balance choice — should the defender heal at the same rate as the player?

**Options:**
- A) Full unify — 10s cooldown for both. Defender heals less often but same amount. Beam fields are non-zero for everyone but only render when there's a target (player self-heal = no beam drawn).
- B) Unify cooldown to a middle ground (e.g. 6s or 8s) that works for both.
- C) Keep cooldown separate — the defender's role as a healer means it needs to heal more frequently. A 10s cooldown defender barely heals at all.

---

## 4. Mine — sub_mine vs Enemy Mine

**Current state:** Config in `sub_mine.c` (player) and `mine.c` (separate).

| Field | Player | Enemy | Notes |
|-------|--------|-------|-------|
| armed_duration_ms | 2000 | 500 | Enemy arms 4x faster |
| exploding_duration_ms | 100 | 100 | Match |
| dead_duration_ms | 0 | 10000 | Enemy respawns after 10s, player mines don't |
| explosion_half_size | 250.0 | 250.0 | Match |
| body_half_size | 10.0 | 10.0 | Match |
| colors | identical | identical | All match |
| light_armed_radius | 180.0 | 150.0 | |
| light_armed_a | 0.4 | 0.3 | |

**Key question:** Enemy mines arm faster because they're pre-placed hazards (500ms means the player sees them blink briefly before danger). Player mines have a 2s fuse as a tactical deployment timer. The dead_duration is a respawn mechanic — enemy mines come back, player mines don't. These feel like entity-level concerns (spawn behavior), not skill tuning.

**Options:**
- A) Full unify — both use 2s arm time. Enemy mines become less threatening but more predictable.
- B) Unify to enemy values — 500ms arm. Player mines become faster/more aggressive.
- C) Move arm time to a deploy parameter rather than skill config — the MINE is the same, but the deployment context differs.
- D) Keep separate — the arm time difference is about deployment context (pre-placed trap vs thrown deployable), not the mine itself.

---

## 5. Dash — sub_egress vs Seeker

**Current state:** Config in `sub_egress.c` (player) and `seeker.c` (separate).
Stalker already uses `Sub_Egress_get_config()` (shares player dash).

| Field | Player (Egress) | Seeker | Notes |
|-------|----------------|--------|-------|
| duration_ms | 150 | 150 | Match |
| speed | 4000 | 5000 | Seeker 25% faster |
| cooldown_ms | 2000 | 0 | Seeker has no cooldown (uses AI orbit/windup timing instead) |
| damage | 50 | 80 | Seeker hits 60% harder |

**Key question:** Stalker already uses the player's dash config. Seeker has boosted values — faster, harder hitting, no cooldown. The seeker is a "glass cannon" (60 HP) that relies on its dash being deadly. If unified to player values (50 dmg, 4000 speed, 2s cooldown), the seeker becomes much less threatening. If unified to seeker values, the player dash becomes overpowered.

**Options:**
- A) Unify to player values — seeker uses 4000/50/2s cooldown. Seeker AI would need rebalancing (it's designed around spammable dashes). Stalker already works with these values.
- B) Unify to middle ground — e.g. 4500 speed, 65 damage, 1s cooldown.
- C) Keep separate — seeker dash is tuned for a specific enemy role (glass cannon predator). The AI timing IS the cooldown.

---

## Recommendation Pattern

For skills where the mechanic is truly the same (corruptor EMP/resist/sprint — support abilities), full unification is correct and already done.

For skills where enemies use the ability in a fundamentally different combat role (seeker dash as primary weapon vs player dash as escape, defender heal as rapid support vs player heal as emergency self-heal), the question is whether game balance should force identical tuning or allow role-based differentiation.

The middle ground: **move all configs to cores** regardless, but allow named variants where the combat role demands it. This keeps configs centralized (one file to edit) while acknowledging that a seeker's dash and a player's dash serve different tactical purposes.
