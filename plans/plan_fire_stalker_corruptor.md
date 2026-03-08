# Implementation Plan: Fire Stalker & Fire Corruptor

## Scope

Implement the last two fire-themed enemy variants and their 4 new subroutines:

| Enemy | New Subs | Base Derives From |
|-------|----------|-------------------|
| Fire Stalker | sub_smolder, sub_blaze (exists) | stealth (unique), dash_core |
| Fire Corruptor | sub_scorch, sub_heatwave, sub_temper | sprint_core, emp_core, resist_core |

## What Already Exists

- **stalker.c/corruptor.c**: Full AI, variant table with THEME_FIRE entries (tint, fragment drops), but no ability divergence yet
- **sub_blaze_core**: Fire dash with flame corridors — used by Fire Seeker, reusable for Fire Stalker
- **Burn system**: Full DOT + GPU-instanced particles, registration API
- **Enemy feedback/variant framework**: Ready for multipliers and theme branching
- **Fragment types & SUB_IDs**: SMOLDER, BLAZE, SCORCH, HEATWAVE, TEMPER all defined in sub_types.h
- **Skill balance entries**: sub_blaze already scored

## Build Order

Work in dependency order. Each phase compiles clean before the next begins.

---

### Phase 1: sub_smolder_core — Heat Shimmer Stealth

**New files**: `sub_smolder_core.h`, `sub_smolder_core.c`

**Why a core (not extending sub_stealth)**: Per CLAUDE.md, stalker stealth is the sole exception — fundamentally different mechanic. But sub_smolder IS a player-acquirable skill. The core pattern gives both player and enemy access. Unlike base stealth (singleton module), smolder needs per-instance state for enemies.

**Config struct** (`SubSmolderConfig`):
- `cloak_duration_ms` — shorter than base stealth (~8s vs unlimited toggle)
- `cooldown_ms` — (~12s, shorter than stealth's 15s since it's time-limited)
- `ambush_duration_ms` — 1000ms (same as stealth)
- `ambush_multiplier` — 5.0 (same)
- `ambush_burn_radius` — 80.0 (ignition burst on ambush break)
- `ambush_burn_stacks` — 2 (applied to targets in radius on ambush)
- `alpha_min` — 0.15 (semi-transparent shimmer, not fully invisible)
- `alpha_pulse_speed` — heat shimmer rate
- Colors: orange-amber shimmer (not purple like stealth)

**State struct** (`SubSmolderCore`):
- `SmolderState state` — READY / ACTIVE / COOLDOWN
- `int duration_ms` — remaining cloak time
- `int cooldown_ms` — remaining cooldown
- `int ambush_ms` — ambush window remaining
- `float pulse_timer` — for shimmer alpha

**Key API**:
- `SubSmolder_init(core)`
- `SubSmolder_try_activate(core, cfg)` → bool (requires feedback == 0 for player, AI decision for enemy)
- `SubSmolder_break(core, cfg)` → enters cooldown
- `SubSmolder_break_attack(core, cfg)` → starts ambush window + returns true (caller applies burn burst)
- `SubSmolder_update(core, cfg, ticks)` → ticks timers, auto-breaks when duration expires
- `SubSmolder_is_active(core)` → bool
- `SubSmolder_is_ambush_active(core)` → bool
- `SubSmolder_get_damage_multiplier(core)` → 5.0 if ambush, else 1.0
- `SubSmolder_notify_kill(core)` → cooldown reset on ambush kill
- `SubSmolder_get_alpha(core, cfg)` → shimmer alpha (pulsing heat distortion)
- `SubSmolder_initialize_audio()` / `SubSmolder_cleanup_audio()` — fire whoosh activate, crackle ambient

**Burn burst on ambush**: When `break_attack()` is called, the CALLER (player wrapper or stalker AI) checks nearby targets in `ambush_burn_radius` and applies `ambush_burn_stacks` via `Burn_apply()`. The core just manages state/timers.

**Render**: `SubSmolder_render_shimmer(core, cfg, pos)` — heat distortion particles (ember-colored points drifting upward around cloaked entity). Bloom and light variants.

---

### Phase 2: Fire Stalker AI Integration

**Modified file**: `stalker.c`

**Approach**: Branch on `theme == THEME_FIRE` at key decision points. Fire Stalker uses sub_smolder instead of base stealth, and sub_blaze_core instead of sub_egress (dash).

**State additions to StalkerState**:
- `SubSmolderCore smolderCore` — embedded per-instance (fire only)
- `SubBlazeCore blazeCore` — embedded per-instance (fire only, for trail segments)
- `BlazeCorridorSegment blazeSegments[STALKER_CORRIDOR_MAX]` — trail buffer (fire only)

**AI divergence points**:

| State | Base Stalker | Fire Stalker |
|-------|-------------|--------------|
| IDLE stealth | Alpha pulsing (base stealth visual) | `SubSmolder_get_alpha()` shimmer |
| IDLE aggro check | `Sub_Stealth_is_stealthed()` blocks | Same — player stealth still blocks |
| STALKING → WINDING_UP | Sub_Egress config dash | Sub_Blaze config dash (longer trail) |
| WINDING_UP visual | White crescent flash | Orange crescent flash |
| DASHING | `SubDash_try_activate` with egress config | `SubDash_try_activate` with blaze config + spawn corridor segments |
| DASHING contact | 50 damage | 50 damage + 2 burn stacks |
| RETREATING | Parting shot (pea projectile) | Parting shot (ember projectile, if available, else pea) |
| RETREATING re-stealth | Alpha fade over 3s | `SubSmolder_try_activate()` after 3s |
| Death | Normal | Ambush burn burst if killed during smolder ambush window |

**Corridor management**:
- Module-level shared corridor buffer (like seeker pattern): `static SubBlazeCore stalkerBlazeCore; static BlazeCorridorSegment stalkerCorridorBuf[STALKER_CORRIDOR_MAX];`
- `Stalker_update_corridors(ticks)` — called unconditionally from mode_gameplay.c
- `Stalker_check_corridor_burn_player()` — called unconditionally, burns player on overlap
- `Stalker_render_corridors()` / `_bloom` / `_light` — three render functions

**New public API additions to stalker.h**:
- `Stalker_update_corridors(unsigned int ticks)`
- `Stalker_check_corridor_burn_player(void)`
- `Stalker_render_corridors(void)` / `_bloom_source` / `_light_source` (or fold into existing render functions)

**Render changes**:
- Fire tint already applied via `Variant_get_color()`
- Add shimmer particle render when `theme == THEME_FIRE && smolderCore.state == ACTIVE`
- Dash trail uses orange instead of cyan
- Bloom/light use warm orange instead of cyan

---

### Phase 3: sub_scorch_core — Burning Sprint Trail

**New files**: `sub_scorch_core.h`, `sub_scorch_core.c`

**Concept**: Sprint that deposits burning footprints. Wraps sub_sprint_core for the movement boost, adds trail management.

**Config struct** (`SubScorchConfig`):
- `SubSprintConfig sprint` — nested sprint config (duration, cooldown, speed_mult)
  - Shorter duration than base sprint (~3s vs 5s per spec "shorter duration")
  - Same cooldown (15s)
- `footprint_interval_ms` — 100 (deposit frequency during sprint)
- `footprint_radius` — 25.0 (burn check radius per footprint)
- `footprint_life_ms` — 4000 (how long footprints persist and burn)
- `burn_interval_ms` — 500 (throttle per footprint)

**State struct** (`SubScorchCore`):
- `SubSprintCore sprint` — embedded sprint state
- `int footprint_timer` — accumulator for deposit spacing

**Footprint pool** (module-level, shared like blaze corridors):
- `ScorchFootprint footprints[SCORCH_FOOTPRINT_MAX]`
  - `bool active`, `Position position`, `int life_ms`, `int burn_tick_ms`, `BurnState burn`

**Key API**:
- `SubScorch_init(core)`
- `SubScorch_try_activate(core, cfg)` → delegates to `SubSprint_try_activate`
- `SubScorch_update(core, cfg, ticks, current_pos)` → ticks sprint + deposits footprints while active
- `SubScorch_is_active(core)` → `SubSprint_is_active`
- `SubScorch_update_footprints(cfg, ticks)` — tick all footprints (life, burn registration)
- `SubScorch_check_footprint_burn(cfg, target)` — circle-AABB overlap, returns hit count
- `SubScorch_deactivate_all()` — clear footprints
- `SubScorch_get_config()` — static singleton
- Render: `SubScorch_render_footprints()` / `_bloom` / `_light`
- Audio: `SubScorch_initialize_audio()` / `cleanup()` — sizzle on deposit, fire crackle ambient

---

### Phase 4: sub_heatwave_core — Feedback Multiplier Ring

**New files**: `sub_heatwave_core.h`, `sub_heatwave_core.c`

**Concept**: Expanding heat ring (like EMP visual) that applies 3x feedback accumulation for 10s instead of EMP's disable effect.

**Prerequisites — player_stats.c/h changes**:
- Add `feedbackMultiplier` (double, default 1.0) and `feedbackMultiplierMs` (int) statics
- `PlayerStats_apply_feedback_multiplier(double mult, unsigned int duration_ms)` — sets multiplier + timer
- `PlayerStats_has_feedback_multiplier()` → bool
- `PlayerStats_get_feedback_multiplier()` → double (for UI display)
- In `PlayerStats_add_feedback()`: multiply amount by `feedbackMultiplier` before adding
- In `PlayerStats_update()`: tick `feedbackMultiplierMs`, reset multiplier to 1.0 when expired

**Prerequisites — enemy_feedback.h/c changes**:
- Add `feedbackMultiplier` and `feedbackMultiplierMs` to `EnemyFeedback` struct
- Apply multiplier in `EnemyFeedback_try_spend()` — cost × multiplier
- Tick timer in `EnemyFeedback_update()`
- `EnemyFeedback_apply_heatwave(fb, multiplier, duration_ms)` — sets multiplier

**Config struct** (`SubHeatwaveConfig`):
- `cooldown_ms` — 30000 (same as EMP)
- `visual_ms` — 300 (ring expansion duration, slightly longer than EMP's 167ms for heat feel)
- `half_size` — 400.0 (800×800 AABB, same as EMP)
- `ring_max_radius` — 400.0
- `feedback_multiplier` — 3.0
- `debuff_duration_ms` — 10000
- Colors: orange-red ring (not EMP's cyan)

**State struct** (`SubHeatwaveCore`):
- `int cooldown_ms`
- `int visual_ms` — ring expansion timer
- `bool active` — visual playing
- `Position center` — where ring originated

**Key API** (mirrors SubEmp pattern):
- `SubHeatwave_init(core)`
- `SubHeatwave_update(core, cfg, ticks)` — tick cooldown and visual
- `SubHeatwave_try_activate(core, cfg, center)` → bool
- `SubHeatwave_is_active(core)` → bool (visual still playing)
- `SubHeatwave_get_cooldown_fraction(core, cfg)` → float
- `SubHeatwave_get_config()` — static singleton
- Render: `SubHeatwave_render_ring(core, cfg)` — expanding orange ring
- Bloom/light variants
- Audio: `SubHeatwave_initialize_audio()` / `cleanup()` — fire whoosh + heat sizzle

**Player wrapper** (`sub_heatwave.c`): On activation, calls `PlayerStats_apply_feedback_multiplier(3.0, 10000)` on nearby enemies via `EnemyRegistry_apply_heatwave()`.

**Enemy (corruptor) usage**: On activation, calls `PlayerStats_apply_feedback_multiplier(3.0, 10000)` on the player if in range.

---

### Phase 5: sub_temper_core — Fire Damage Reflection Aura

**New files**: `sub_temper_core.h`, `sub_temper_core.c`

**Concept**: Aura that reflects 50% of fire and burn damage back to the attacker. Wraps sub_resist_core for the aura framework, replaces DR with reflection.

**Design decision — reflection scope**:
- Reflects **fire and burn damage only** (not all damage types)
- Requires damage source tracking — who dealt the fire/burn damage
- Reflection damages the SOURCE entity, not area damage

**Prerequisites — damage source tracking**:

Current `Enemy_check_player_damage()` returns a `PlayerDamageResult` but doesn't identify which specific source. For temper reflection, we need to know if damage was fire-typed.

**Approach**: Tag damage as fire-typed at the source. Add a `bool is_fire` field to `PlayerDamageResult` (or track per-weapon). When an enemy with temper active takes fire damage, 50% is reflected back to the player via `PlayerStats_damage()`.

For the **player side** (player uses sub_temper): When player takes fire/burn damage from an enemy, reflect 50% back. This requires:
- `PlayerStats_damage_fire(amount, source_pos)` — new function or flag on existing
- On reflect: `EnemyRegistry_damage_at_position(source_pos, reflected_amount)`

**Simpler approach for v1**: Since burn damage ticks from the `BurnState` system which doesn't track source, and projectile damage comes from known pools:
- Tag projectile pools as fire-typed in config
- `Burn_tick_enemy()` returns damage dealt → if temper active, reflect 50% to... nobody (burn has no source). Burn reflection could just be "reduced burn damage" (effectively 50% fire resist) rather than true reflection.
- True reflection works for direct hits (projectile/contact damage with known source position)

**Config struct** (`SubTemperConfig`):
- `SubResistConfig resist` — nested resist config (duration, cooldown, ring visuals)
  - Same duration/cooldown as base resist (5s / 10s)
- `float reflect_percent` — 0.50 (50%)
- `bool fire_only` — true (only reflects fire-typed damage)
- Colors: deep orange ring with red pulse

**State struct** (`SubTemperCore`):
- `SubResistCore resist` — embedded resist state
- `double reflected_this_frame` — accumulator for visual feedback

**Key API**:
- `SubTemper_init(core)`
- `SubTemper_try_activate(core, cfg)` → delegates to `SubResist_try_activate`
- `SubTemper_update(core, cfg, ticks)` → delegates to `SubResist_update`
- `SubTemper_is_active(core)` → `SubResist_is_active`
- `SubTemper_check_reflect(core, cfg, damage, is_fire)` → returns reflected amount (0 if not active or not fire)
- `SubTemper_get_config()` — static singleton
- Render: `SubTemper_render_ring()` — orange-red pulsing ring (reuses resist ring geometry with fire colors)
- Render: `SubTemper_render_beam()` — beam to protected target (orange-red)
- Bloom/light variants
- Audio: `SubTemper_initialize_audio()` / `cleanup()` — resonant impact on reflect, fire crackle ambient

**Enemy integration** (corruptor applies temper to allies):
- When `theme == THEME_FIRE`, corruptor uses `SubTemperCore` instead of `SubResistCore`
- Protected allies get 50% fire damage reflection instead of 50% DR
- When player hits a tempered enemy with fire damage (ember, blaze corridor, cinder, inferno, flak), 50% reflects back

**Player integration** (player acquires sub_temper):
- Self-cast aura: player takes 50% reduced fire/burn damage, reflected portion damages nearby fire enemies
- Burn DOT while tempered: each tick reduced by 50%

---

### Phase 6: Fire Corruptor AI Integration

**Modified file**: `corruptor.c`

**Approach**: Branch on `theme == THEME_FIRE` at ability activation points. Fire Corruptor uses sub_scorch instead of sprint, sub_heatwave instead of EMP, sub_temper instead of resist.

**State additions to CorruptorState**:
- `SubScorchCore scorchCore` — embedded (fire only)
- `SubHeatwaveCore heatwaveCore` — embedded (fire only)
- `SubTemperCore temperCore` — embedded (fire only)

**AI divergence points**:

| State | Base Corruptor | Fire Corruptor |
|-------|---------------|----------------|
| Init | SubSprint/SubEmp/SubResist init | SubScorch/SubHeatwave/SubTemper init |
| SUPPORTING resist | `SubResist_try_activate` on wounded ally | `SubTemper_try_activate` on nearby ally |
| SUPPORTING beam | Resist beam to target | Temper beam to target (orange-red) |
| SPRINTING movement | `SubSprint` speed boost | `SubScorch` speed boost + footprint trail |
| EMP_FIRING | `SubEmp_try_activate` → disables feedback | `SubHeatwave_try_activate` → 3x feedback for 10s |
| RETREATING | Normal flee | Scorch footprints persist behind |
| Render aura | Resist ring (orange) | Temper ring (deep orange-red) |
| Render ring | EMP ring (cyan) | Heatwave ring (orange-red) |

**Footprint management** (like stalker corridors):
- Module-level shared pool: `static ScorchFootprint corruptorFootprints[CORRUPTOR_FOOTPRINT_MAX]`
- `Corruptor_update_footprints(ticks)` — called unconditionally from mode_gameplay.c
- `Corruptor_check_footprint_burn_player()` — burns player on overlap
- Render functions for footprints (main, bloom, light)

**New public API additions to corruptor.h**:
- `Corruptor_update_footprints(unsigned int ticks)`
- `Corruptor_check_footprint_burn_player(void)`
- Footprint render functions (or fold into existing bloom/light calls)

---

### Phase 7: Player Wrappers

**New files**: `sub_smolder.c/h`, `sub_scorch.c/h`, `sub_heatwave.c/h`, `sub_temper.c/h`

Each follows the established pattern (see sub_emp.c, sub_blaze.c as templates):
- Static singleton core state
- `try_activate()` — check equipped, check cooldown, check feedback, delegate to core, add feedback cost
- `update(ticks)` — delegate to core
- `render()` / `render_bloom()` / `render_light()` — delegate to core
- `initialize()` / `cleanup()` — audio lifecycle via core
- `get_cooldown_fraction()` — for skillbar UI
- Register in `skillbar.c` activation table and `mode_gameplay.c` update/render loops

**Feedback costs** (initial, subject to playtesting):

| Sub | Feedback | Cooldown | Notes |
|-----|----------|----------|-------|
| sub_smolder | 0 (gate: feedback must be 0) | 12s | Like stealth — free to enter, but feedback blocks entry |
| sub_scorch | 15 | 15s | Like sprint but with area denial bonus |
| sub_heatwave | 25 | 30s | Like EMP — big cooldown, big impact |
| sub_temper | 20 | 10s | Like resist — moderate cost/cooldown |

**Skill balance entries** (add to spec_skill_balance.md):

| Skill | Tier | POWER | SUSTAIN | REACH | Score | Notes |
|-------|------|-------|---------|-------|-------|-------|
| sub_smolder | Normal | 7 | +1 | −1 | **7** | Like stealth but time-limited + burn burst. Trade invisibility for area denial. |
| sub_scorch | Normal | 6 | +1 | +1 | **8** | Sprint + area denial trail. Offensive movement. |
| sub_heatwave | Normal | 7 | −1 | +3 | **9** | 3x feedback for 10s. Massive debuff, huge cooldown. |
| sub_temper | Normal | 6 | +1 | +1 | **8** | 50% fire reflection. Niche but devastating vs fire loadouts. |

---

### Phase 8: Integration & Polish

1. **mode_gameplay.c** — Add update/render calls for:
   - `Stalker_update_corridors()`, `Stalker_check_corridor_burn_player()`
   - `Corruptor_update_footprints()`, `Corruptor_check_footprint_burn_player()`
   - Player sub_smolder/sub_scorch/sub_heatwave/sub_temper update + render
   - Bloom source + light source calls for all new subs

2. **skillbar.c** — Register activation callbacks for 4 new subs

3. **catalog.c** — Descriptions and icons for 4 new subs

4. **progression.c** — Fragment thresholds for SMOLDER, SCORCH, HEATWAVE, TEMPER (already have SUB_IDs)

5. **spec_skill_balance.md** — Add 4 new entries

6. **Compile clean** — `make compile` with zero warnings after each phase

---

## Risk Areas

1. **Damage reflection (temper)** — Most complex new mechanic. Fire-type tagging doesn't exist yet. May need a `DamageType` enum or `is_fire` flag threaded through damage paths. Start simple: reflect direct fire hits, treat burn DOT as 50% reduction (no source to reflect to).

2. **Feedback multiplier (heatwave)** — Touches player_stats and enemy_feedback, both core systems. Changes to `PlayerStats_add_feedback()` affect everything. Test thoroughly with spillover edge cases.

3. **Smolder vs stealth interaction** — Player could have both sub_stealth and sub_smolder equipped. They should be mutually exclusive (same SubroutineType = STEALTH). Skillbar type exclusivity handles this.

4. **Corridor buffer sizing** — Stalker corridors + seeker corridors + corruptor footprints all active simultaneously in fire zones. Each pool needs its own buffer. Size conservatively, test in dense fire zones.
