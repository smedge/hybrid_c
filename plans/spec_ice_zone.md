# Spec: The Archive (Ice Zone) — Enemies & Subroutines

## Combat Thesis

> **Momentum control. Everything slows, freezes, shatters. Commitment is punished.**

> *"CRYONIS does not iterate its programs the way PYRAXIS does. These designs have been unchanged since they were first created. CRYONIS does not improve things. It keeps them."*

Every ice sub answers: **"How does this punish commitment?"** — slow the player's movement, freeze them in place, reduce their ability to reposition, and shatter them when they're locked down.

---

## Prerequisites

### Chill Status Effect System (`chill.c/h`)

Ice zone requires a **chill** status effect system analogous to `burn.c/h`. This is the foundational system that most ice subs interact with.

**Design**:

```c
#define CHILL_MAX_STACKS 3
#define CHILL_DURATION_MS 2000       /* 2s per stack */
#define CHILL_SLOW_PER_STACK 0.25    /* 25% speed reduction per stack */
#define FREEZE_DURATION_MS 2000      /* 2s hard freeze at max stacks */

typedef struct {
    int stacks;
    int duration_ms[CHILL_MAX_STACKS];
    int freeze_ms;                   /* when > 0, target is frozen solid */
    int immune_ms;                   /* post-freeze immunity window */
} ChillState;
```

**Mechanics**:
- **1 stack**: 25% movement slow, 2s duration
- **2 stacks**: 50% movement slow, each stack has independent timer
- **3 stacks**: triggers **freeze** — target is rooted in place for 2s, all stacks consumed
- **Post-freeze immunity**: 2s window where new chill stacks cannot be applied (prevents perma-freeze)
- **Frozen targets take 2x damage** from the hit that triggers the freeze (shatter bonus)

**API** (mirrors burn.h pattern):
- `Chill_apply(ChillState *state, int duration_ms)` — add a chill stack
- `Chill_update(ChillState *state, unsigned int ticks)` — tick timers, trigger freeze at 3 stacks
- `Chill_get_slow_mult(const ChillState *state)` — returns speed multiplier (1.0 = no slow, 0.5 = 2 stacks)
- `Chill_is_frozen(const ChillState *state)` — true if currently frozen
- `Chill_reset(ChillState *state)` — clear all stacks + freeze
- `Chill_render(const ChillState *state, Position pos)` — frost crystal particles
- `Chill_tick_enemy(ChillState *chill, double *hp, unsigned int ticks)` — enemy helper
- Player singleton: `Chill_apply_to_player()`, `Chill_update_player()`, `Chill_render_player()`, `Chill_reset_player()`, `Chill_player_get_slow_mult()`, `Chill_player_is_frozen()`

**Visual FX**:
- **1 stack**: Subtle frost particle trail behind entity, pale blue tint
- **2 stacks**: Dense frost crystals, visible ice crackle overlay, blue-white tint
- **Frozen**: Entity turns blue-white, ice crystal shell rendered around body, completely still
- **Shatter**: Burst of ice crystal particles on freeze-trigger hit

**Audio**:
- Chill apply: crystalline chime (pitch rises with stack count)
- Freeze trigger: sharp ice crack
- Shatter (frozen target hit): glass shatter sound
- Ambient frozen state: quiet frozen wind loop

**Integration points**:
- `Ship_update()`: multiply movement speed by `Chill_player_get_slow_mult()`, skip input when `Chill_player_is_frozen()`
- Each enemy's update: multiply movement speed by `Chill_get_slow_mult()`, skip AI when `Chill_is_frozen()`
- `PlayerStats_reset()` / respawn: call `Chill_reset_player()`
- Registration system like burn: `Chill_register()`, `Chill_render_all()`, `Chill_render_bloom_source()`

---

## Ice Theme Identity

- **Colors**: Blue-white, pale cyan, deep frost blue, crystalline white highlights
- **Bloom**: Cold blue-white glow
- **Audio**: Ice crack/shatter, frozen wind, crystal chime on freeze, glass-like resonance
- **Ambient FX**: Frost crystal particles, cold vapor trails
- **Effect cells**: None — ice zone has no effect cells
- **Boss**: CRYONIS (separate boss spec)

---

## Ice Subroutines (10 new)

### Projectile Subs

#### sub_frost — Frost Bolt

> *"Projectiles apply 2-second movement debuff on hit. Sustained fire creates ice-locked targets."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_mgun | Hunter primary |
| **Type** | Projectile (instant) | |
| **Damage** | 8 per hit | Same as mgun |
| **Fire rate** | 200ms cooldown | Slower than mgun (150ms) — each hit compounds |
| **Projectile speed** | 400 u/s | Slower than mgun (500) — readable, dodgeable |
| **Range** | 3500u | Same as mgun |
| **Pool size** | 8 | Same as mgun |
| **Feedback cost** | 1 per shot | Same as mgun |
| **Status effect** | 1 chill stack per hit | 2s slow, 3 hits = freeze |
| **Chill duration** | 2000ms per stack | |
| **Visual** | Pale blue-white projectile, frost trail | Ice crystal particle trail |
| **Audio** | Crystalline whistle on fire, chime on chill apply | |

**Core file**: `sub_frost_core.c/h` — projectile pool + chill-on-hit logic. Shared between Ice Hunter (enemy) and player sub_frost.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 5 | 40 DPS (×0.70 aim = 28 eDPS, POWER 3-4 range) + chill stacking elevates to 5. Not about damage — about lockdown setup. |
| SUSTAIN | +3 | 1 feedback/shot, fire at will |
| REACH | 0 | 3500u but aimed projectiles (×0.70 aim) |
| **Total** | **8/10** | 2 under Normal cap. The power is in chill stacking, not raw DPS. |

---

#### sub_snowcone — Cone of Cold

> *"Narrow cone of slow ice pellets. Each pellet applies chill. Up close, one shot can nearly freeze."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_tgun | Hunter secondary |
| **Type** | Projectile (instant) | |
| **Damage** | 3 per pellet | Low per-pellet, high aggregate |
| **Pellets per shot** | 9 | Wide spread cone |
| **Fire rate** | 667ms cooldown (~1.5x pea rate) | Slower than tgun |
| **Projectile speed** | 350 u/s | Slow — readable, but wide cone compensates |
| **Range** | 2333u | 2/3 tgun range |
| **Pool size** | 18 (2 shots worth) | |
| **Feedback cost** | 3 per shot | |
| **Status effect** | 1 chill stack per pellet hit | 3+ pellets from one shot = instant freeze up close |
| **Visual** | Spray of pale blue ice shards in a narrow cone | Crystalline particles |
| **Audio** | Crackling ice burst on fire, chime per chill apply | |

**Core file**: `sub_snowcone_core.c/h` — cone projectile pool + chill-on-hit per pellet. Shared between Ice Hunter (enemy) and player sub_snowcone.

**Design note**: The ice shotgun. At close range, 5+ pellets landing means instant freeze from a single trigger pull. At range, the spread means only 1-2 pellets connect — applying chill but not freezing. This creates a natural danger gradient: let an Ice Hunter close the gap and you're getting frozen. Keep distance and you can manage the chill stacks.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 6 | 27 damage/shot (9×3) × 0.80 cone aim = ~32 eDPS. Low raw DPS but mass chill application is the real power. |
| SUSTAIN | +2 | 3 feedback/shot, manageable rate |
| REACH | +1 | 2333u, cone delivery — generous hitzone but range falloff |
| **Total** | **9/10** | 1 under Normal cap. Crowd control shotgun. |

---

### Movement Subs

#### sub_shatter — Shatter Dash

> *"Dash leaves frozen vapor trail. Dash shatters frozen enemies for 100 integrity damage."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_egress | Seeker dash, Stalker mobility |
| **Type** | Movement (instant) | |
| **Dash speed** | 5x movement speed | Same as egress |
| **Dash duration** | 150ms | Same as egress |
| **Cooldown** | 2500ms | Slightly longer than egress (2000ms) |
| **Feedback cost** | 25 | Same as egress |
| **I-frames** | Yes | Same as egress |
| **Contact damage** | 30 base | Against non-frozen targets |
| **Shatter damage** | **100** against frozen targets | Consumes freeze, massive burst |
| **Vapor trail duration** | 5000ms | Frozen vapor persists behind dash path |
| **Vapor trail width** | ~40u | |
| **Vapor trail effect** | 1 chill stack on first contact per segment | Walking through vapor = chill applied |
| **Visual** | Blue-white dash streak, frozen vapor trail, ice crystal explosion on shatter | |
| **Audio** | Sharp frozen wind whoosh on dash, glass shatter sound on frozen-target hit | |

**Core file**: `sub_shatter_core.c/h` — dash mechanics + vapor trail system + shatter-on-frozen contact. The dash IS the shatter delivery — not a projectile.

**Design note**: This is the ice zone's combo finisher delivered by the seeker. The seeker orbits laying vapor trails (each orbit dash leaves chill-applying vapor). The player accumulates chill stacks from the trails. Then the attack dash shatters them for 100 damage. The player learns this loop from fighting it — chill setup → shatter payoff — and can use the same tool offensively: chill enemies with frost/snowcone, then shatter-dash through them for 100 damage.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 8 | I-frames + 30 base contact + 100 conditional shatter + vapor trail. Highest burst in ice set, but requires frozen target. |
| SUSTAIN | +1 | 25 feedback, 2.5s cooldown. Same tier as egress. |
| REACH | +1 | 600u dash + vapor trail provides lingering area effect |
| **Total** | **10/10** | At Normal cap. The payoff skill — massive conditional burst justifies the cap. |

---

---

#### sub_permafrost — Permafrost Sprint

> *"Speed boost that freezes the ground. Ice Corruptors lay down frozen highways."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_sprint | Corruptor mobility |
| **Type** | Movement (instant) | |
| **Speed multiplier** | 1.5x | Same as sprint |
| **Duration** | 5000ms | Same as sprint |
| **Cooldown** | 20000ms | Same as sprint |
| **Feedback cost** | 0 | Same as sprint |
| **Trail effect** | Freezes ground along path — 1 chill stack on contact | Narrower than glaciate trail |
| **Trail duration** | Lasts until sprint ends, then fades over 3s | Longer persistence than glaciate |
| **Trail width** | ~30u | Narrow frozen strip |
| **Chill on contact** | 1 stack when first stepping on trail | |
| **Visual** | Frost spreading from feet as entity moves, ice crackle particles | |
| **Audio** | Sustained frozen crackle while sprinting, ice spreading sound | |

**Core file**: `sub_permafrost_core.c/h` — wraps `sub_sprint_core` + adds frost trail system.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 6 | 1.5x speed + frost trail area denial. Trail is the weapon, not the speed. |
| SUSTAIN | +2 | 0 feedback, 25% uptime (5s/20s) |
| REACH | +1 | Self-buff + trail affects area behind |
| **Total** | **9/10** | 1 under Normal cap. Sprint + area control. |

---

### Deployable Sub

#### sub_cryomine — Cryo Mine

> *"Detonation creates a cryo blast that slows everything in radius for 3 seconds."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_mine | Ice Mine enemy |
| **Type** | Deployable (instant) | |
| **Blast damage** | 30 | Much less than mine (100) — it's CC, not damage |
| **Blast radius** | 150u | Larger than mine (100u) — wide slow zone |
| **Chill on detonation** | 2 stacks to everything in radius | Almost-freeze in one blast |
| **Fuse time** | 2000ms | Same as mine |
| **Max deployed** | 3 | Same as mine |
| **Cooldown** | 250ms | Same as mine |
| **Feedback cost** | 15 | Same as mine |
| **Lingering field** | 3s cryo zone after blast — 1 chill/sec to anything inside | Frost fog persists |
| **Visual** | Pale blue pulsing mine, ice crystal detonation, lingering frost fog | |
| **Audio** | Ice crack on detonation, frozen wind for lingering field | |

**Core file**: `sub_cryomine_core.c/h` — wraps mine core mechanics + cryo blast + lingering chill field.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 6 | Low damage but 2-stack chill in AoE + lingering field. Pure CC. |
| SUSTAIN | +2 | 15 feedback, 3 pool. Same as mine. |
| REACH | -1 | Self-place + fuse delay. Same as mine. |
| **Total** | **7/10** | 3 under Normal cap. Pure setup — needs follow-up damage to kill. |

---

### Support Subs

#### sub_stasis — Stasis Heal

> *"Ice seals the wound. A brief freeze cleanses fire and restores integrity."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_mend | Ice Defender heal |
| **Type** | Healing (instant) | |
| **Heal amount** | 40 | Slightly less than mend (50) |
| **Micro-stasis** | 500ms | Target briefly freezes during heal (visual/thematic, not a real lockout) |
| **Burn cleanse** | Yes — clears all burn stacks | **Cross-zone counter**: the reason to bring this into The Crucible |
| **Burn immunity** | 3s post-heal | Mirrors cauterize's chill immunity duration |
| **Cooldown** | 10000ms | Same as mend |
| **Feedback cost** | 20 | Same as mend |
| **Range** | 300u | Same as mend (defender targets allies) |
| **Visual** | Brief ice crystallization over target, burn particles extinguished, frost shimmer on heal | |
| **Audio** | Crystalline chime on heal, sizzle/hiss as burn is extinguished | |

**Core file**: `sub_stasis_core.c/h` — heal + burn cleanse + burn immunity grant + micro-stasis visual. Mirrors `sub_cauterize_core` architecture.

**Design note**: Stasis heals by briefly freezing the target — ice preserves, ice seals wounds, ice stops the bleeding. The 500ms micro-stasis is mostly visual flavor (the ice crystallization effect), not a meaningful lockout. The real power is the burn cleanse — this is the tool you bring to The Crucible to survive fire DOTs. Cauterize cleanses chill (bring to The Archive), stasis cleanses burn (bring to The Crucible). Neither cleanses its own zone's status effect. The cross-zone dependency is baked into the heal slot.

**Enemy behavior**: Ice Defender targets wounded allies (same triage logic as mend). Heals them + cleanses any burn + grants 3s burn immunity. The brief freeze visual makes it readable — the player sees the ice flash and knows the enemy was healed and burn-cleansed.

**Player behavior**: Self-cast heal. 40 HP + burn cleanse + 3s burn immunity. The go-to survival tool in The Crucible. In The Archive it's a decent heal but the burn cleanse is wasted — you want cauterize there for chill cleanse.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 5 | 40 HP heal (slightly less than mend) + burn cleanse + burn immunity. Comparable to mend with status cleanse upside. |
| SUSTAIN | +1 | 20 feedback, 10s cooldown. Same as mend. |
| REACH | +1 | 300u on allies, self-cast for player |
| **Total** | **7/10** | 3 under Normal cap. Heal value is in the burn cleanse, not raw HP. |

---

#### sub_cryoshield — Cryo Shield

> *"Ice armor that shatters when broken — AoE burst + slow to everything nearby."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_aegis | Ice Defender shield |
| **Type** | Shield (instant) | |
| **Shield HP** | 80 | Absorbs 80 damage before breaking |
| **Duration** | 8000ms | Shorter than aegis (10s) — or until broken |
| **Shatter damage** | 40 | AoE burst when shield breaks |
| **Shatter radius** | 120u | |
| **Shatter chill** | 2 stacks to everything in radius | Almost-freeze on break |
| **Cooldown** | 25000ms | Shorter than aegis (30s) |
| **Feedback cost** | 25 | Less than aegis (30) |
| **Visual** | Crystalline ice armor shell, cracks appear as damage absorbed, explosive shatter on break | |
| **Audio** | Ice formation on apply, cracking as damage taken, glass shatter burst on break | |

**Core file**: `sub_cryoshield_core.c/h` — HP-based shield (not binary invuln like aegis), shatter-on-break mechanic.

**Design note**: This is fundamentally different from aegis. Aegis is binary invulnerability. Cryoshield is an HP pool that punishes the attacker for breaking through. The player must decide: burn through the shield (eat shatter + chill) or wait it out (8s of the enemy being protected). Patience vs aggression — the ice thesis in shield form.

**Enemy behavior**: Ice Defender casts on self when threatened (same trigger as aegis). Player must decide whether to break the shield (risk getting frozen by shatter) or back off and wait.

**Player behavior**: Cast on self. Absorbs 80 damage, and if enemies break it, they eat the shatter. Rewards getting hit. Anti-synergy with evasive play, synergy with tanking.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 7 | 80 HP absorb + 40 shatter damage + 2 chill stacks on break. Not invuln, but punishing. |
| SUSTAIN | 0 | 25 feedback, 25s cooldown. Strategic, once per fight. |
| REACH | +1 | Self + 120u shatter radius |
| **Total** | **8/10** | 2 under Normal cap. Different value prop than aegis — reactive, not preventive. |

---

### Stealth/Utility Sub

#### sub_flash_freeze — Flash Freeze

> *"Camouflage via frozen stillness. Ambush freezes the target solid."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_stealth | Ice Stalker stealth |
| **Type** | Stealth (instant) | |
| **Cloak duration** | 8000ms | Shorter than stealth (10s) |
| **Cloak condition** | Must be still — any movement breaks cloak | Different from stealth (can move slowly) |
| **Ambush damage** | 50 | Same as stealth (base) |
| **Ambush effect** | Instant freeze (2s) — no chill stacking needed | Hard freeze, no buildup |
| **Feedback gate** | 0 feedback required | Same as stealth |
| **Cooldown** | 15000ms | Same as stealth |
| **Speed while cloaked** | 0 (stationary) | Must be completely still |
| **Visual** | Entity becomes ice-colored, crystalline static — blends into frost environment | Not transparent like stealth — mimics scenery |
| **Audio** | Quiet crystallization on cloak, sharp ice crack on ambush | |

**Design note**: Stalker's flash_freeze uses different logic from player stealth, same as fire's sub_smolder. The Stalker's cloak is fundamentally a positional ambush — it freezes in place and waits for the player to walk past. The player version works the same: stand still to cloak, ambush freezes the first enemy you hit. Rewards patience and positioning over the aggressive stealth-walk-up playstyle.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 7 | 50 damage + guaranteed 2s freeze. Hard CC ambush. |
| SUSTAIN | +1 | 0 feedback gate, 15s cooldown. Same as stealth. |
| REACH | -2 | Must be stationary. Self-only. Extremely positioning-dependent. |
| **Total** | **6/10** | 4 under Normal cap. High skill ceiling — massive payoff for perfect positioning. |

---

### Debuff/Control Subs

#### sub_deep_freeze — Deep Freeze

> *"Expanding cold ring that suppresses all cooldown recovery for 5 seconds."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_emp | Corruptor area debuff |
| **Type** | Control (instant) | |
| **Ring expansion** | 0 → 400u over 500ms | Same pattern as EMP |
| **Effect** | Cooldown suppression — all skill cooldowns pause for 5s | Skills don't recharge |
| **Chill** | 1 stack to everything hit by ring | Slow + cooldown lock |
| **Duration** | 5000ms | |
| **Cooldown** | 30000ms | Same as EMP |
| **Feedback cost** | 30 | Same as EMP |
| **Visual** | Expanding blue-white frost ring, frozen crystalline wave | |
| **Audio** | Deep frozen wind whoosh as ring expands, crystalline resonance | |

**Core file**: `sub_deep_freeze_core.c/h` — expanding ring + cooldown suppression debuff state.

**Design note**: EMP disables skills entirely. Deep freeze is more insidious — your skills still work, but their cooldowns don't tick. You can use what's off cooldown, but once you've spent your ready abilities, you're stuck waiting. In a zone full of chill and freeze, losing cooldown recovery means your escape dash won't come back, your heal won't refresh, your shield stays down. The ice thesis: commitment is punished.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 6 | Cooldown suppression is less immediately devastating than EMP's full disable, but longer lasting and more strategically crippling. |
| SUSTAIN | -1 | 30 feedback, 30s cooldown. Once per fight. |
| REACH | +3 | 400u expanding ring, guaranteed hit if in range |
| **Total** | **8/10** | 2 under Normal cap. Strategic control, not burst. |

---

#### sub_arctic_aura — Arctic Aura

> *"Aura that slows all enemies entering range. Allies in range gain slow-on-hit."*

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Replaces** | sub_resist | Corruptor aura |
| **Type** | Control (instant) | |
| **Aura radius** | 200u | Same as resist |
| **Duration** | 8000ms | Longer than resist (5s) — the cold lingers |
| **Slow effect** | 30% speed reduction to enemies in range | Passive slow field |
| **Ally buff** | Allies in range apply 1 chill stack on hit | Grants chill-on-hit to nearby friendlies |
| **Cooldown** | 20000ms | Longer than resist (15s) |
| **Feedback cost** | 25 | Same as resist |
| **Visual** | Pale blue aura ring, frost particles drift outward, ice crystal orbit | |
| **Audio** | Quiet frozen wind while active, crystalline ambient hum | |

**Core file**: `sub_arctic_aura_core.c/h` — aura state + slow field + ally chill-on-hit buff.

**Enemy behavior**: Corruptor activates arctic aura, creating a slow field. All enemies within 200u gain chill-on-hit — their normal attacks also apply chill stacks. The entire squad becomes a freeze machine. Forces the player to kill the corruptor first or fight from outside the aura.

**Player behavior**: Self-centered aura. Enemies within 200u are slowed 30%. Player's own attacks apply chill. Turns the player into a frost turret — stand your ground and freeze everything nearby.

**Balance budget**:

| Axis | Score | Reasoning |
|------|-------|-----------|
| POWER | 6 | 30% passive slow + chill-on-hit for allies. Team force multiplier. |
| SUSTAIN | +1 | 25 feedback, 20s cooldown, 40% uptime. Moderate. |
| REACH | +1 | 200u radius, affects area around self |
| **Total** | **8/10** | 2 under Normal cap. Area control support. |

---

## Ice Enemy Variants

### Ice Mine

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_cryomine |
| **HP** | Same as base mine |
| **Colors** | Pale blue body, blue-white blink |
| **Bloom** | Cold blue-white |
| **AI tweaks** | None — same fuse/detonation behavior, different payload |

**Behavior**: Identical to base mine mechanically. Detonation creates cryo blast (2 chill stacks + lingering frost field) instead of raw damage. Sets up kills for hunters and seekers rather than killing directly. Walking through a cryo mine field stacks chill rapidly — 2 mines = frozen.

---

### Ice Hunter

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_frost (primary), sub_snowcone (secondary) |
| **HP** | 100 (same as base) |
| **Colors** | Blue-white body, pale cyan accents |
| **Bloom** | Cold blue-white |
| **AI tweaks** | Longer engagement range (1.2x), slower movement (0.8x), weapon switching |

**Behavior**: Ice Hunters are marksmen, not assault troops. They engage from further away and move more slowly — fitting the theme of deliberate, patient combat. Primary weapon is frost bolt (chill stacking at range), secondary is snowcone (close-range cone that can freeze in one blast).

**Weapon switching AI**: Ice Hunter fires frost bolts at range. When the player closes to within snowcone range (~2333u), switches to cone of cold for mass chill application. This creates a layered threat — at range you're getting chilled steadily, up close you're getting frozen.

**Implementation**: The variant config carries both sub_frost and sub_snowcone pools. AI checks distance to decide which pool to fire from (same pattern as base hunter mgun/tgun switching). Same state machine as base hunter.

---

### Ice Seeker

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_shatter |
| **HP** | 60 (same as base) |
| **Colors** | Blue-white needle, crystalline trail |
| **Bloom** | Cold blue-white |
| **AI tweaks** | Longer orbit phase (1.3x duration), vapor trail is primary weapon |

**Behavior**: Ice Seekers are arena controllers. Their orbit phase is longer — circling the player laying down frozen vapor trails that apply chill on contact. The attack dash shatters frozen targets for 100 integrity damage. The real threat isn't the dash — it's the closing vapor net around you.

**Combat loop**: Orbit → lay vapor trails → orbit → trails close in → attack dash through the trapped, frozen player for 100 shatter damage. If the player steps on vapor trails during orbit, they accumulate chill stacks. By the time the attack dash comes, they're likely frozen — and the shatter hits for 100.

**Implementation**: Same state machine as base seeker. All dashes (orbit and attack) use `sub_shatter_core` — each leaves a vapor trail. Attack dash checks `Chill_is_frozen()` on contact for the 100 damage shatter bonus.

---

### Ice Defender

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_stasis (heal replacement), sub_cryoshield (shield) |
| **HP** | 80 (same as base) |
| **Colors** | Pale blue hexagon, crystalline highlights |
| **Bloom** | Cold blue-white |
| **AI tweaks** | Same flee AI, stasis instead of heal, cryoshield instead of aegis |

**Behavior**: Ice Defender heals wounded allies with stasis — a brief ice flash that restores HP, cleanses burn, and grants burn immunity. The defender's cryoshield on self discourages aggression — breaking the shield punishes the attacker with shatter + chill.

**Player counterplay**: Stasis heals are fast (same speed as mend) so the player needs to burst targets down or focus the defender first. Cryoshield: either wait 8s for it to expire, or break it and eat the shatter — a genuine tactical decision.

**Implementation**: Same defender state machine. `sub_stasis_core` replaces mend logic (mirrors cauterize_core architecture). `sub_cryoshield_core` replaces aegis logic. Shield tracks HP instead of binary invuln.

---

### Ice Stalker

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_flash_freeze (stealth), sub_shatter (mobility) |
| **HP** | Same as base stalker |
| **Colors** | Pale blue crescent, ice crystal shimmer |
| **Bloom** | Faint cold blue (barely visible when cloaked) |
| **AI tweaks** | Frozen stillness cloak — stops moving to cloak, ambush freezes target |

**Behavior**: Ice Stalker is the perfect ambush predator for the ice thesis. It moves into position, then freezes still — becoming a crystalline formation barely distinguishable from the frost environment. When the player walks past, it strikes: 50 damage + instant 2s freeze. The player is now frozen in hostile territory with enemies closing in.

**Combat loop**: Approach (visible) → stop and cloak (stationary crystallization) → wait for player proximity → ambush (50 damage + hard freeze) → shatter dash away (leaving vapor trail) → reposition → repeat.

**Implementation**: Same stalker state machine structure. Sub_flash_freeze replaces sub_smolder/sub_stealth for the cloak. Sub_shatter replaces sub_blaze for dash — the escape dash leaves vapor trails and can shatter frozen targets on the way out. The "must be still to cloak" behavior is a stalker AI tweak, not a core change.

---

### Ice Corruptor

| Attribute | Value |
|-----------|-------|
| **Loadout** | sub_permafrost (sprint), sub_deep_freeze (EMP replacement), sub_arctic_aura (resist replacement) |
| **HP** | Same as base corruptor |
| **Colors** | Deep frost blue body, ice crystal protrusions |
| **Bloom** | Cold blue-white |
| **AI tweaks** | Lays ice highways with permafrost sprint, suppresses cooldowns with deep freeze, empowers squad with arctic aura |

**Behavior**: Ice Corruptor is the battlefield shaper. It sprints through the arena laying frost trails (permafrost), activates arctic aura to slow nearby enemies and grant chill-on-hit to allies, and uses deep freeze to suppress the player's cooldown recovery.

**Combat impact**: When an Ice Corruptor is active, the entire zone becomes a chill trap. Permafrost trails apply chill on contact, all nearby enemies apply chill on hit (arctic aura), and the player's escape abilities don't recharge (deep freeze). Kill the corruptor first — or freeze to death.

**Implementation**: Same corruptor state machine. Three core swaps: `sub_permafrost_core` replaces `sub_scorch_core`/`sub_sprint_core`, `sub_deep_freeze_core` replaces `sub_emp_core`/`sub_heatwave_core`, `sub_arctic_aura_core` replaces `sub_resist_core`/`sub_temper_core`.

---

## Ice Fragment Types (10 new)

```
FRAG_TYPE_FROST, FRAG_TYPE_SNOWCONE, FRAG_TYPE_SHATTER,
FRAG_TYPE_PERMAFROST, FRAG_TYPE_CRYOMINE, FRAG_TYPE_STASIS, FRAG_TYPE_CRYOSHIELD,
FRAG_TYPE_FLASH_FREEZE, FRAG_TYPE_DEEP_FREEZE, FRAG_TYPE_ARCTIC_AURA
```

All use cold blue/pale cyan fragment colors. Source enemy names: "Ice Hunter", "Ice Seeker", etc.

---

## Ice Enemy Variant Summary

| Enemy | Ice Variant | Loadout | Combat Role | AI Tweaks |
|-------|------------|---------|-------------|-----------|
| Mine | Ice Mine | sub_cryomine | CC setup — area chill | Same fuse behavior, cryo payload |
| Hunter | Ice Hunter | sub_frost, sub_snowcone | Ranged lockdown — chill stacking at range, freeze shotgun up close | 1.2x range, 0.8x speed, weapon switching |
| Seeker | Ice Seeker | sub_shatter | Arena controller — vapor trail net + shatter dash | 1.3x orbit duration, vapor trail focus |
| Defender | Ice Defender | sub_stasis, sub_cryoshield | Preserver — stasis allies, punish shield-breakers | Same flee AI, stasis replaces heal |
| Stalker | Ice Stalker | sub_flash_freeze, sub_shatter | Positional ambush — freeze-on-strike, shatter escape | Stationary cloak, hard freeze ambush |
| Corruptor | Ice Corruptor | sub_permafrost, sub_deep_freeze, sub_arctic_aura | Battlefield shaper — ice everything | Frost trails, cooldown suppression, squad chill buff |

---

## Balance Summary

| Skill | Tier | POWER | SUSTAIN | REACH | Score | Cap | Verdict |
|-------|------|-------|---------|-------|-------|-----|---------|
| sub_frost | Normal | 5 | +3 | 0 | **8** | 10 | -2 under. Chill stacker, not a damage dealer. |
| sub_snowcone | Normal | 6 | +2 | +1 | **9** | 10 | -1 under. Crowd control ice shotgun. |
| sub_shatter | Normal | 8 | +1 | +1 | **10** | 10 | At cap. Dash + vapor trail + 100 shatter burst. The payoff skill. |
| sub_permafrost | Normal | 6 | +2 | +1 | **9** | 10 | -1 under. Sprint + frost trails. |
| sub_cryomine | Normal | 6 | +2 | -1 | **7** | 10 | -3 under. Pure CC setup. Needs follow-up. |
| sub_stasis | Normal | 5 | +1 | +1 | **7** | 10 | -3 under. Heal value is in burn cleanse, not raw HP. |
| sub_cryoshield | Normal | 7 | 0 | +1 | **8** | 10 | -2 under. HP shield with punishing shatter. |
| sub_flash_freeze | Normal | 7 | +1 | -2 | **6** | 10 | -4 under. High skill ceiling ambush. |
| sub_deep_freeze | Normal | 6 | -1 | +3 | **8** | 10 | -2 under. Cooldown suppression, strategic control. |
| sub_arctic_aura | Normal | 6 | +1 | +1 | **8** | 10 | -2 under. Squad force multiplier. |

**Zone average: 8.4/11** — the ice set is slightly lower-scoring than fire (8.3) because ice power is conditional and synergistic. Individual ice subs are weaker in isolation but devastating in combination (frost/snowcone → chill stacks → shatter dash for 100). This is intentional — the zone's thesis IS about compound effects.

---

## Fire ↔ Ice Elemental Interaction

The Crucible and The Archive are a **paired zone system**. Each zone's heal sub cleanses the other zone's status effect. Beyond heals, fire and ice interact at every layer — status effects, shields, area denial, and boss mechanics. Neither zone is fully conquerable without tools from the other.

### Layer 1: Heal Sub Cross-Dependency

The heal slot is the player's declaration of which zone they're hunting in.

| Sub | Zone of Origin | Heal | Cleanses | Grants Immunity | Bring To |
|-----|---------------|------|----------|-----------------|----------|
| **sub_cauterize** | The Crucible (fire) | 35 HP | Chill stacks + freeze | 3s chill immunity | **The Archive** |
| **sub_stasis** | The Archive (ice) | 40 HP | Burn stacks | 3s burn immunity | **The Crucible** |

**Neither cleanses its own zone's status effect.** sub_mend (base heal) doesn't cleanse anything. To survive The Archive's freeze-heavy squads, you need cauterize from The Crucible. To survive The Crucible's burn DOTs, you need stasis from The Archive.

**sub_cauterize rework**: Currently cauterize cleanses burn + grants burn immunity. This changes — cauterize cleanses **chill/freeze** + grants **chill immunity**. The burn cleanse + burn aura mechanics move to its existing aura (the aura still burns enemies nearby), but the heal's cleanse target flips from burn → chill. Implementation: `SubCauterize_try_activate()` calls `Chill_reset_player()` + `Chill_grant_immunity_player()` instead of `Burn_grant_immunity_player()`.

### Layer 2: Status Effect Override

When opposing elements collide on the same target, they react — they don't coexist.

| Condition | Effect | Mechanical Result |
|-----------|--------|-------------------|
| Fire damage → frozen target | **Thermal shatter** | Freeze consumed, shatter bonus damage dealt |
| Chill applied → burning target | **Extinguish** | 1 burn stack removed per chill stack applied, chill consumed |
| Burn applied → chilled target | **Thaw** | 1 chill stack removed per burn stack applied, burn consumed |
| Hard freeze → burning target | **Flash extinguish** | All burn stacks removed, target freezes |

**Thermal shatter**: Any fire damage source (sub_ember, sub_flak, sub_cinder, sub_inferno) triggers the shatter damage bonus on frozen targets — the same bonus sub_shatter delivers on dash contact. In The Archive, fire subs serve double duty: they deal damage AND cash in freezes. The player walks into The Archive with ember, sees ice hunters freezing things, and discovers "my fire shots shatter frozen targets." That discovery moment is exactly the PRD's progression vision.

**Extinguish / Thaw**: Opposing status effects consume each other 1:1. A frost bolt on a burning target removes 1 burn stack and the chill is consumed (no chill applied). A cinder hit on a chilled target removes 1 chill stack and the burn is consumed (no burn applied). This means: in The Crucible, ice subs strip burn stacks. In The Archive, fire subs strip chill stacks. Each element is both a weapon and a cleanse tool.

**Flash extinguish**: Hard freeze (3rd chill stack or flash_freeze ambush) overrides burn entirely — all burn stacks removed, target freezes. Freeze always wins over burn when a freeze triggers.

**No coexistence**: A target is never simultaneously burning and chilled. The elements react on contact. This keeps the system clean and prevents weird stacking interactions.

### Layer 3: Shield Vulnerability

Themed shields are soft-countered by the opposite element.

| Shield | Vulnerability | Effect |
|--------|--------------|--------|
| **Cryoshield** (ice, HP-based) | Fire damage | **1.5x damage** to shield HP. 80 HP shield becomes effectively ~53 HP against fire. |
| **Immolate** (fire, timed) | Chill application | **Each chill stack strips 1s** from immolate duration. 3 frost bolts remove 3s of shield time. |

Fire breaks ice shields faster. Ice shortens fire shields. Both shields retain their dangerous secondary effects (cryoshield still shatters on break, immolate still burns nearby enemies) — the counter gives you a faster path through, not a free pass.

**Player implications**: Bring fire subs to The Archive and you can punch through defender cryoshields before they become a problem. Bring ice subs to The Crucible and you can strip immolate off defenders quickly. Without the counter element, those shields are brick walls you have to wait out or endure.

### Layer 4: Area Denial Counter

Fire and ice area denial effects neutralize each other on the battlefield.

| Interaction | Result |
|-------------|--------|
| Fire pool (cinder/ember) overlaps frost trail (shatter/permafrost) | **Both consumed** in overlap zone. Steam burst visual + hiss sound. |
| Flame corridor (blaze) crosses frost trail | **Both consumed** where paths cross. |
| Frost trail laid over fire pool | **Fire pool extinguished** in trail area. |
| Fire pool placed on frost trail | **Frost trail melted** in pool area. |

**In The Archive**: sub_blaze (fire dash corridor) carves safe paths through ice seeker vapor trail nets. sub_cinder fire pools melt frost trails. Fire area denial is your terrain counter.

**In The Crucible**: sub_shatter's vapor trail extinguishes ember burn zones. sub_permafrost frost trails melt fire pools. Ice area denial is your terrain counter.

This creates active counter-play on the battlefield — you're not just dealing damage, you're reshaping the arena. The seeker lays a vapor trail ring around you, you blaze-dash through it and burn an escape path. That's the kind of moment that sticks.

**Implementation**: Area denial effects already track position and radius. Overlap check: `distance < effectA_radius + effectB_radius`. On overlap, both shrink/expire in the collision zone. New system: `ElementalInteraction_check_area_overlap()` runs each frame, iterates fire pools vs frost trails.

### Layer 5: Boss Design Implications

Boss fights become loadout puzzles where cross-zone preparation is rewarded.

**CRYONIS (ice boss — bring fire subs):**
- Boss casts massive freeze waves → sub_cauterize cleanses freeze + grants chill immunity
- Boss shields with empowered cryoshield → fire subs break it 1.5x faster
- Boss creates frost zones restricting movement → fire area denial (blaze, cinder) melts safe paths
- Boss stasis-locks player → cauterize's chill immunity prevents re-freeze after cleanse
- **Without fire subs**: Every mechanic is a wall. Freezes last full duration, shields take forever, frost zones have no counter. Technically possible but miserable.
- **With fire subs**: Every mechanic has an answer. The fight is about execution, not endurance.

**PYRAXIS (fire boss — bring ice subs):**
- Boss fills arena with burn zones → sub_stasis cleanses burn + grants burn immunity
- Boss immolates → chill strips duration faster
- Boss ramps center burn → frost trails create safe patches by extinguishing fire
- **Without ice subs**: You're always on fire. Burn DOTs eat integrity, fire zones restrict movement. Pure attrition.
- **With ice subs**: Burn is manageable. You can extinguish fire zones, cleanse DOTs, strip immolate.

### The Progression Loop

How a new player experiences the cross-zone dependency:

1. **Enter The Crucible.** Everything burns. sub_mend heals HP but doesn't remove burn — DOTs keep ticking. Push through, unlock fire subs (ember, cinder, cauterize). Hit PYRAXIS, can't handle the sustained burn pressure. Retreat.

2. **Enter The Archive.** Everything freezes. But sub_cauterize cleanses chill — fire tools from The Crucible make the ice manageable. Push deeper, unlock ice subs (frost, shatter, stasis).

3. **Return to The Crucible with stasis.** Now burn has an answer. PYRAXIS's fire mechanics are manageable. Beat PYRAXIS.

4. **Return to The Archive with fire subs.** Ember shatters frozen targets. Blaze corridors melt frost trails. CRYONIS's ice mechanics have answers. Beat CRYONIS.

The player bounces between zones, each trip armed with new tools from the other. That's metroidvania progression through loadout discovery — not keys and doors, not colored gates. "Oh, THAT'S how I beat this."

---

## Implementation Order

The dependencies drive the implementation sequence:

### Phase 1: Foundation
1. **`chill.c/h`** — Chill status effect system (stacks, freeze, immunity, visual FX, audio)

### Phase 1.5: Fire Skill Rework (Cross-Zone Prerequisites)

Before ice subs can counter fire mechanics (and vice versa), the existing fire skills need rework to support elemental interactions.

**sub_cauterize rework** (`sub_cauterize_core.c/h`, `sub_cauterize.c`, `defender.c`):
- Currently: heal + burn cleanse + burn immunity
- Changes to: heal + **chill/freeze cleanse** + **chill immunity** (3s)
- `sub_cauterize.c:37`: `Burn_grant_immunity_player()` → `Chill_reset_player()` + `Chill_grant_immunity_player()`
- `defender.c:336-337`: `EnemyRegistry_cleanse_burn()` → new `EnemyRegistry_cleanse_chill()` (chill equivalent)
- Burn aura stays — cauterize still burns nearby enemies, it just cleanses chill instead of burn
- Config struct: `immunity_duration_ms` now refers to chill immunity, not burn immunity
- Requires: `chill.c/h` from Phase 1, plus `Chill_grant_immunity()` / `Chill_grant_immunity_player()` API
- Requires: new `EnemyRegistry_cleanse_chill()` registry function + per-enemy `*_cleanse_chill()` methods (mirrors existing `*_cleanse_burn()` pattern)

**sub_immolate chill vulnerability** (`sub_immolate_core.c/h`):
- Add: each chill stack applied to an immolated target strips 1s from remaining shield duration
- `SubImmolate_apply_chill_penalty(SubImmolateCore *core, int stacks)` — reduces `life_ms` by `stacks * 1000`
- Integration: `Chill_apply()` checks if target has active immolate, calls penalty if so
- Requires: `chill.c/h` from Phase 1

**Elemental interaction system** (`elemental.c/h` or inline in `chill.c`/`burn.c`):
- Status override rules: fire on frozen = thermal shatter, chill on burning = extinguish (1:1 stack consume), burn on chilled = thaw (1:1), hard freeze on burning = flash extinguish
- Area denial cancellation: fire pools vs frost trails neutralize on overlap
- Shield vulnerability: fire deals 1.5x to cryoshield HP (handled in Phase 3's cryoshield_core)
- Can be implemented as checks in `Burn_apply()` / `Chill_apply()` rather than a separate module

### Phase 2: Core Subs (enemy weapons first)
3. **`sub_frost_core.c/h`** — Frost bolt projectile pool + chill-on-hit
4. **`sub_snowcone_core.c/h`** — Cone of cold projectile pool + mass chill
5. **`sub_shatter_core.c/h`** — Shatter dash + vapor trail + frozen-bonus-damage on contact
6. **`sub_cryomine_core.c/h`** — Cryo mine + lingering frost field

### Phase 3: Support & Control Cores
7. **`sub_stasis_core.c/h`** — Heal + burn cleanse + burn immunity + micro-stasis visual
8. **`sub_cryoshield_core.c/h`** — HP-based shield + shatter-on-break
9. **`sub_deep_freeze_core.c/h`** — Expanding ring + cooldown suppression
10. **`sub_arctic_aura_core.c/h`** — Slow aura + chill-on-hit buff
11. **`sub_permafrost_core.c/h`** — Sprint + frost trail

### Phase 4: Stealth
12. **`sub_flash_freeze_core.c/h`** — Stationary cloak + freeze ambush (stalker-specific logic similar to sub_smolder)

### Phase 5: Enemy Integration
13. **Ice Mine** — `mine.c` THEME_ICE variant (cryomine core)
14. **Ice Hunter** — `hunter.c` THEME_ICE variant (frost + snowcone, range-based weapon switching)
15. **Ice Seeker** — `seeker.c` THEME_ICE variant (shatter dash + vapor trails)
16. **Ice Defender** — `defender.c` THEME_ICE variant (stasis + cryoshield)
17. **Ice Stalker** — `stalker.c` THEME_ICE variant (flash_freeze + shatter)
18. **Ice Corruptor** — `corruptor.c` THEME_ICE variant (permafrost + deep_freeze + arctic_aura)

### Phase 6: Player Wrappers
19. **`sub_frost.c/h`** — Player frost bolt (input + feedback + core delegation)
20. **`sub_snowcone.c/h`** — Player cone of cold
21. **`sub_shatter.c/h`** — Player shatter dash
22. **`sub_permafrost.c/h`** — Player permafrost sprint
24. **`sub_cryomine.c/h`** — Player cryo mine
25. **`sub_stasis.c/h`** — Player self-stasis
26. **`sub_cryoshield.c/h`** — Player cryo shield
27. **`sub_flash_freeze.c/h`** — Player flash freeze
28. **`sub_deep_freeze.c/h`** — Player deep freeze pulse
29. **`sub_arctic_aura.c/h`** — Player arctic aura

### Phase 7: Progression & Polish
30. **Fragment types** — Add 10 ice fragment types to fragment.c/h
31. **Progression entries** — Add ice sub progression thresholds
32. **Skill balance entries** — Add ice subs to spec_skill_balance.md
33. **Ice zone audio** — Zone ambient sounds, sub sound effects
34. **Mark spec_themed_enemies.md** — Mark ice section as implemented

### Phase 8: Cross-Zone Validation
35. **PYRAXIS counter test** — Verify ice subs counter PYRAXIS properly:
    - sub_stasis cleanses burn + grants burn immunity (survive fire DOTs)
    - Chill strips immolate duration (break fire defender shields faster)
    - Frost trails / shatter vapor trails extinguish fire pools (clear arena of burn zones)
    - Cryoshield absorbs PYRAXIS fire damage (tanking option)
36. **Archive counter test** — Verify fire subs counter ice enemies properly:
    - sub_cauterize cleanses chill/freeze + grants chill immunity (survive ice lockdown)
    - Fire damage on frozen targets triggers thermal shatter (bonus damage)
    - Fire pools melt frost trails / shatter vapor trails (clear arena of chill zones)
    - Fire deals 1.5x to cryoshield HP (break ice defender shields faster)
37. **Mixed loadout test** — Equip cross-zone loadouts (e.g. frost + cauterize + cinder + shatter) and verify all interactions work in both zones

---

## Open Questions for Revision

1. **Chill on player movement speed**: Should `Chill_player_get_slow_mult()` affect dash speed too, or only base movement? (Recommendation: base movement only — dashes are escape tools and should remain reliable)

2. **Freeze vs i-frames**: Can the player be frozen while i-framed (egress dash)? (Recommendation: no — i-frames block chill application, same as they block damage and burn application)

3. **Cryoshield HP value (80)**: Is 80 the right number? For reference, player integrity max is 100. An 80 HP shield means the player needs to dump most of a magazine into it before it breaks. May need tuning.

4. ~~Resolved~~: Stasis reworked to heal + burn cleanse (no longer self-freeze).

6. **Ice Seeker orbit trail density**: How many shatter dashes during orbit phase? Too few = easy to dodge, too many = inescapable ice net. Needs playtesting.

7. **Deep freeze vs EMP**: Cooldown suppression vs full disable. Is the distinction clear enough in gameplay? EMP is "your skills don't work," deep freeze is "your skills work but don't recharge." May feel too similar in practice.

8. **Flash freeze stationary requirement for player**: Does this make the player version too niche? Stealth lets you move (slowly). Flash freeze requires standing still. Could be a hard sell for the skillbar slot.
