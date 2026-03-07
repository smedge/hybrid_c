# Spec: Subroutine (Skill) Balance Framework

## Purpose

This is a **living document** — updated whenever a skill is added, modified, or retuned. It provides a systematic framework for balancing subroutines against each other using a point-budget system inspired by Guild Wars skill design.

**Core principle**: Every subroutine is scored on three axes — **POWER** (what it does), **SUSTAIN** (how long you can keep doing it), and **REACH** (how far and reliably you can deliver it). Total = POWER + SUSTAIN + REACH, capped per tier. Aim difficulty is baked into POWER via the aim discount table, not penalized separately. No skill should exceed its tier's cap without deliberate design justification.

**This framework is a starting point, not a replacement for playtesting.** The budget system gives new skills a principled baseline. Playtesting and player feedback refine the numbers. But without a framework, balancing 50+ skills becomes pure guesswork.

---

## Executive Summary

**Budget caps**: Normal = 10, Rare = 15, Elite = 20. Over cap = powerful, under cap = room to buff.

| Skill | Tier | POWER | SUSTAIN | REACH | Score | Cap | Verdict |
|-------|------|-------|---------|-------|-------|-----|---------|
| sub_pea | Normal | 4 | +3 | 0 | **7** | 10 | -3 under. Precise aim tax. Room to buff. |
| sub_mgun | Normal | 4 | +3 | 0 | **7** | 10 | -3 under. Same eDPS as pea, spray slightly more forgiving. |
| sub_mine | Normal | 5 | +2 | −1 | **6** | 10 | -4 under. Area denial utility, not a DPS weapon. |
| sub_cinder | Normal | 6 | +2 | 0 | **8** | 10 | -2 under. Fire mine + lingering burn pool. |
| sub_tgun | **Rare** | 6 | +3 | +3 | **12** | 15 | -3 under. Constant stream of death. Best DPS/feedback ratio. |
| sub_flak | **Rare** | 12 | +1 | +1 | **14** | 15 | -1 under. Fire shotgun + burn DOT. Nearly at cap. |
| sub_ember | Normal | 6 | +3 | +1 | **10** | 10 | At cap. AOE ignite on impact — group burn applicator. |
| sub_boost | **Elite** | 11 | +3 | +3 | **17** | 20 | -3 under. Free unlimited speed. Room for a damage component. |
| sub_sprint | Normal | 5 | +2 | +1 | **8** | 10 | -2 under. Timed boost — same speed, limited window. |
| sub_inferno | **Elite** | 14 | +3 | +2 | **19** | 20 | -1 under. Melts everything close-range + burn DOT. |
| sub_disintegrate | **Elite** | 14 | +3 | +3 | **20** | 20 | At cap. THE elite benchmark. Finger of god. |
| sub_gravwell | Normal | 7 | 0 | +3 | **10** | 10 | At cap. First CC skill. Pull + slow, pure force multiplier. |
| sub_emp | Normal | 6 | −1 | +3 | **8** | 10 | -2 under. AoE feedback debuff, once-per-fight. |
| sub_stealth | Normal | 7 | +1 | −1 | **7** | 10 | -3 under. High skill ceiling. Invisibility + ambush. |
| sub_egress | Normal | 7 | +1 | +2 | **10** | 10 | At cap. I-frames + 50 contact damage. Dodge-attack. |
| sub_mend | Normal | 5 | +1 | +1 | **7** | 10 | -3 under. Burst heal + 3x regen boost. Rewards clean play. |
| sub_aegis | Normal | 9 | −1 | +2 | **10** | 10 | At cap. 10s god mode. Binary invuln properly valued. |
| sub_resist | Normal | 5 | +1 | +1 | **7** | 10 | -3 under. 50% DR for 5s. Solid defensive option. |
| sub_blaze | Normal | 7 | +1 | +1 | **9** | 10 | -1 under. Fire dash + flame corridor. Area denial dodge. |
| sub_cauterize | Normal | 6 | +1 | +1 | **8** | 10 | -2 under. Fire heal + burn aura. Offensive healing. |
| sub_immolate | Normal | 8 | −1 | +1 | **8** | 10 | -2 under. Shorter shield + melee burn aura. Aggro invuln. |

**Key takeaways**:
- **Score = POWER + SUSTAIN + REACH**: Three axes, no separate modifiers. Everything folds into one of three questions: what does it do (POWER), how long can you keep doing it (SUSTAIN), how far can you deliver it (REACH).
- **Disintegrate is THE benchmark at 20/20**: POWER 14 (hitscan 900 eDPS, full pierce, carve sweep) + SUSTAIN +3 (toggle, natural burst recovery) + REACH +3 (2600u hitscan). Every other skill is calibrated against this.
- **Three-tier power curve**: Normal (cap 10) → Rare (cap 15) → Elite (cap 20). Each tier jump is meaningful.
- **Aim is baked into POWER**: The aim discount table converts theoretical DPS to effective DPS BEFORE scoring. Pea's 100 DPS × 0.55 aim = 55 eDPS (POWER 4). No separate targeting penalty — the discount is in the number.
- **Normal weapons are honestly scored**: Pea and mgun at 7, mine at 6. The 13-point gap to disintegrate (20) properly captures the massive power difference.
- **Every tier has room to grow**: Only disintegrate, egress, gravwell, and aegis sit at cap. Everything else has budget room for buffs.

---

## Skill Activation Categories

Every subroutine falls into one of three activation categories. This is a fundamental design axis that affects how the skill is used, budgeted, and balanced.

### Toggle

- **Activated/deactivated** by the player (press to toggle on, press again to toggle off — or hold to maintain)
- **Untoggles other toggles of the same SubroutineType** — only one movement toggle, one weapon toggle, etc. can be active at a time
- **May have ongoing feedback cost** while active (drains feedback per second)
- **No cooldown** between toggle on/off (but may have an activation delay)
- **Budget consideration**: Toggles are available on demand — the player chooses when the effect is active. This flexibility is inherently powerful. Budget must account for effective uptime.

**Current examples**: sub_boost (hold shift for speed, no feedback drain)

**Future examples**: damage auras (toggle on for AoE damage, drains feedback/sec), detection fields (toggle on to reveal cloaked enemies, drains feedback/sec), weapon modes (toggle between fire patterns)

### Instant

- **One-time activation** with a cooldown before next use
- **May have a cast time** (activation delay before effect)
- **Feedback cost on activation** (one-time cost per use)
- **Cooldown is the primary gating mechanism** — short cooldown = spammable, long cooldown = strategic
- **Budget consideration**: Power is delivered in bursts. The cooldown determines how often those bursts happen. DPS/HPS over time is what matters, not per-hit numbers in isolation.

**Current examples**: sub_pea, sub_mgun, sub_mine, sub_cinder, sub_egress, sub_mend, sub_aegis, sub_stealth, sub_gravwell, sub_emp, sub_sprint, sub_resist, sub_blaze, sub_cauterize, sub_immolate

**Future examples**: AoE blasts, teleports, summon commands, cleanse/purify

### Passive

- **Always on** while equipped in the skillbar — no activation, no cooldown, no per-use feedback cost
- **The cost IS the slot** — you're dedicating one of your 10 skillbar slots to a permanent effect instead of an active ability
- **Budget consideration**: Passives may seem low-impact per attribute, but they're multiplicative with everything else. A passive +10% damage boost affects EVERY attack across all your weapons. A passive +2 integrity regen is always healing you. The slot cost is significant — giving up an active skill for a passive is a real tradeoff. Budget must reflect the cumulative value of "always on."

**Current examples**: None yet

**Future examples**: Stat boosts (+damage, +speed, +regen), resistance effects (reduced feedback generation, cooldown reduction), aura effects (nearby enemy slow, ally buff radius)

---

## Attribute Taxonomy

Every subroutine has a subset of these tunable attributes. These are the "knobs" available for design and balancing.

### Universal Attributes (all skills)

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Feedback cost** | Resource cost per activation (instant), per second (toggle), or 0 (passive) | feedback points |
| **Cooldown** | Time before skill can be used again (instants only) | milliseconds |
| **Activation category** | Toggle / Instant / Passive | category |
| **Elite status** | Normal or Elite (higher budget cap) | boolean |
| **Slot type** | SubroutineType for exclusivity (projectile, movement, deployable, healing, shield, etc.) | type |

### Damage Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Damage per hit** | Direct damage dealt per activation/impact | HP |
| **DPS** | Effective damage per second (derived: damage × fire rate) | HP/s |
| **Hits per second** | Fire rate (1000 / cooldown_ms for instants) | hits/s |
| **Damage type** | Direct / DoT / Explosion / Contact | type |

### Delivery Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Range** | Maximum effective distance | world units |
| **AoE radius** | Area of effect (0 = single target) | world units |
| **Projectile speed** | Travel speed for ranged skills | units/s |
| **Pool size** | Maximum concurrent instances (projectiles, deployables) | count |
| **Targeting** | Auto-aim / Aimed (cursor) / Placed / Self | type |

### Timing Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Duration** | How long the effect lasts | milliseconds |
| **Uptime ratio** | Duration / (Duration + Cooldown) — effective % of time the effect is active | ratio |
| **Cast time** | Delay before effect triggers | milliseconds |
| **Fuse/Delay** | Time between deployment and effect (e.g., mine fuse) | milliseconds |

### Defensive Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Healing amount** | HP restored per activation | HP |
| **HPS** | Effective healing per second (derived) | HP/s |
| **Damage reduction** | Percentage or flat damage mitigation | % or HP |
| **Invulnerability** | Complete damage immunity | boolean + duration |
| **Shield strength** | Absorb X damage before breaking (future) | HP |

### Movement Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Speed multiplier** | Movement speed modifier | multiplier |
| **Dash distance** | Total distance covered (speed × duration) | world units |
| **Control during** | Can the player steer during the effect? | boolean |

### Utility Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Status effect** | Type + strength + duration of applied debuff/buff | varies |
| **Detection range** | Reveal radius for cloaked enemies | world units |
| **Area denial** | Does it control space? (mines, walls, zones) | boolean |
| **Escape value** | Does it reposition the player? | boolean |
| **Piercing** | Hits multiple targets on the same path? | boolean |

### Drawback Attributes

| Attribute | Description | Unit |
|-----------|-------------|------|
| **Self-root** | Player immobilized during cast | boolean |
| **Interruptible** | Can the effect be cancelled by damage | boolean |
| **Friendly fire** | Can damage the player | boolean |
| **Ramp-up** | Requires time to reach full effect | milliseconds |

---

## System-Level Mechanics Affecting Balance

### Damage-to-Feedback (0.5x Ratio)

Taking damage generates feedback at half the damage amount (e.g., 20 damage = 10 feedback). This feedback caps at FEEDBACK_MAX without spillover — no death spiral from getting hit. Shielded (aegis) and i-framed (egress dash) damage generates zero feedback since the damage is fully blocked.

**Balance implications**:
- **Feedback is now a universal combat pressure meter**, not just a skill-usage cost. Both offense (skill usage) and defense (taking hits) contribute.
- **Stealth gating becomes skill-based**: Sub_stealth requires 0 feedback to activate. Previously, players could stop shooting and wait for feedback to decay. Now they must ALSO avoid taking damage. This makes stealth activation a genuine achievement in combat.
- **Defensive skills gain hidden value**: Aegis and egress i-frames don't just prevent integrity loss — they prevent feedback generation. A well-timed dash through a projectile volley avoids both the HP damage AND the feedback that would delay your next stealth activation.
- **Healing (sub_mend) doesn't remove the feedback from the hit**: You can heal the integrity damage, but the feedback from getting hit still needs to decay naturally. Taking hits has a lingering cost beyond HP.

---

## Point Budget System

Every skill is scored as **POWER + SUSTAIN + REACH**. The total is compared against the tier cap.

### Budget Caps

| Tier | Budget Cap | Description |
|------|-----------|-------------|
| **Normal** | **10** | Standard subroutines. Most skills live here. |
| **Rare** | **15** | Themed zone subroutines. Stronger than normal, earned through zone progression. |
| **Elite** | **20** | Exceptional subroutines. Clearly more powerful, but balanced against other elites. Progression rewards — feel special without being game-breaking. |

### Three-Axis Scoring: POWER + SUSTAIN + REACH

**Score = POWER + SUSTAIN + REACH**

Three axes, no separate modifiers. Everything folds into one of three questions:
1. **POWER (3–14)**: What does it DO at peak? (total output with aim discount)
2. **SUSTAIN (−2 to +3)**: How long can you keep doing it? (cost + uptime)
3. **REACH (−2 to +3)**: How far and reliably can you deliver? (range × practical accuracy)

**Max = 14 + 3 + 3 = 20** (exactly the elite cap)
**Min = 3 + (−2) + (−2) = −1** (nothing should score here)

---

#### POWER Axis (3–14)

"What does this skill DO at peak?" One number capturing total effective output.

**For damage skills**: Start with theoretical DPS, apply aim discount, add DOT, factor in group scaling/pierce.

**Aim Discount Table** (applied BEFORE tiering — this is the key innovation):

| Delivery Method | Multiplier | Why |
|----------------|------------|-----|
| Hitscan/beam | ×0.95 | Near-perfect accuracy once on target |
| Dense spray (50+ hits/sec) | ×0.90 | Volume compensates for spread |
| Volume fire (5–10 hits/sec) | ×0.85 | Misses cost little time |
| Cone/multi-pellet | ×0.80 | Generous hitzone but range falloff |
| Aimed (2–5 shots/sec) | ×0.70 | Each miss costs real DPS |
| Precise (1–2 shots/sec, narrow) | ×0.55 | Miss = half your damage gone |

**Power Tier Guide (damage)**:

| POWER | Effective DPS | Examples |
|-------|--------------|---------|
| 3–4 | <100 eDPS | Starter weapons (pea 55, mgun 70) |
| 5–6 | 100–300 eDPS | Upgraded weapons (tgun 212, flak 750+burn) |
| 7 | + special properties | Burn persistence, DOT, burst combo |
| 8–11 | 500–1000 eDPS | Would-be elite single-target |
| 12–14 | 1000+ eDPS or group-wipe | Elite channeled weapons (inferno, disintegrate) |

**Power Tier Guide (non-damage)**:

| POWER | Effect | Examples |
|-------|--------|---------|
| 5 | Minor buff/heal (50% DR 5s, 50HP heal, timed speed) | resist, mend, sprint |
| 6 | Moderate control/debuff, or heal + utility | emp, cauterize |
| 7 | Strong control/utility (full CC trap, i-frames+contact, full invis+5x ambush) | gravwell, egress, stealth, blaze |
| 8 | Reduced binary defense + offensive component | immolate |
| 9 | Binary defensive power (10s full invulnerability) | aegis |
| 11 | Best-in-class utility (unlimited 2x speed, zero cost) | boost |

---

#### SUSTAIN Axis (−2 to +3)

"How long can you keep doing it?" Combines feedback cost + cooldown + uptime into one number.

| SUSTAIN | Description | Examples |
|---------|-------------|---------|
| −2 | Extreme tax: huge feedback + glacial cooldown. Once per fight AND painful. | (reserved for future skills) |
| −1 | Double-taxed: high feedback + long cooldown. Once per fight. | aegis (30fb + 30s cd), emp (30fb + 30s cd) |
| 0 | Strategic: significant cost OR long cooldown. Deliberate use. | gravwell (25fb + 20s cd) |
| +1 | Moderate: meaningful cost but manageable frequency. | egress (25fb + 2s), mend (20fb + 10s), stealth (0fb but gated + 15s), flak (16fb/sec), resist (25fb + 15s) |
| +2 | Efficient: low cost, decent uptime. | mine (15fb/boom, 3 pool), sprint (0fb but 25% uptime) |
| +3 | Free/unlimited: negligible cost, fire at will. | pea (1fb), mgun (1fb), tgun (10fb/sec but best ratio), boost (0fb toggle), inferno (25/sec toggle), disintegrate (22/sec toggle) |

Note: Channeled elites score +3 despite high fb/sec because they're toggles — you control the sustain window, and recovery between bursts is natural gameplay.

---

#### REACH Axis (−2 to +3)

"How far and reliably can you deliver?" Range × practical accuracy × delivery quality.

| REACH | Description | Examples |
|-------|-------------|---------|
| −2 | Self-only AND restricted (speed penalty, break conditions, positioning constraints) | (reserved) |
| −1 | Self-place with drawbacks (fuse delay, friendly fire) OR restricted self-buff | mine (self-place + fuse + FF), stealth (self + 0.5x speed + break conditions) |
| 0 | Long range but poor practical accuracy (precise aim at range = low hit%) | pea (3500u but ×0.55 aim), mgun (3500u but ×0.70 aim) |
| +1 | Medium range with decent delivery, or self-buff with temporal coverage | flak (2333u + cone), sprint (self + 5s window), mend (self + 5s regen), resist (self + 5s DR) |
| +2 | Long range with good delivery, or strong self-coverage | inferno (1250u + dense spray), egress (600u dash + i-frames), aegis (self + 10s invuln) |
| +3 | Extreme range + reliable targeting, or wide AoE, or permanent self-effect | disintegrate (2600u hitscan), tgun (3500u + volume fire), gravwell (cursor + 600u AoE), emp (400u AoE through walls), boost (permanent self) |

---

## Current Skill Score Cards

### sub_pea — Projectile (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 4 | 4 | 100 theoretical DPS × 0.55 aim = 55 eDPS. Starter weapon, each shot matters. |
| SUSTAIN | +3 | +3 | 1 fb/shot, can fire 100 shots before cap. Effectively free. |
| REACH | 0 | 0 | 3500u range but precise aim — practical hit rate at range is ~55%. Long range offset by poor accuracy. |
| **TOTAL** | | **7** | |

**Assessment**: 3 under normal cap. The aim discount (×0.55 for precise single-shot) is the defining constraint — against fast-moving seekers and strafing hunters, practical DPS drops to ~55. Pea's low POWER honestly reflects what playtesting shows: it struggles to keep up. Room to buff via more damage per shot, faster fire rate, or secondary effects.

---

### sub_mgun — Projectile Variant (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 4 | 4 | 100 theoretical DPS × 0.70 aim = 70 eDPS. 5 shots/sec means misses cost less — aimed, not precise. |
| SUSTAIN | +3 | +3 | 1 fb/shot, same free-fire economy as pea. |
| REACH | 0 | 0 | 3500u range but aimed fire — better practical accuracy than pea (×0.70 vs ×0.55) but still limited by narrow projectiles at distance. |
| **TOTAL** | | **7** | |

**Assessment**: 3 under cap, same as pea. Identical theoretical DPS (100), but mgun's 5 shots/sec earns a better aim multiplier (×0.70 aimed vs ×0.55 precise) — 70 eDPS vs 55. Both land at POWER 4 because neither crosses the 100 eDPS threshold. Room for differentiation via unique mechanics (spread pattern, armor shred, etc.).

---

### sub_mine — Deployable (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 5 | 5 | ~75 unreliable DPS + 250u AoE + area denial + 3-mine pool. Utility exceeds raw DPS. |
| SUSTAIN | +2 | +2 | 15 fb/boom, 3-mine pool with 250ms cooldown. Efficient — deploy all 3 cheaply, damage is delayed. |
| REACH | −1 | −1 | Self-place only + 2s fuse delay (enemies can escape) + friendly fire risk (100 HP self-damage). |
| **TOTAL** | | **6** | |

**Assessment**: 4 under normal cap. Mine's DPS is unreliable — enemies escape the fuse, standing too close kills you. The real value is area denial and burst on grouped enemies. At 6, mine has the most room to buff of any skill. Options: larger blast radius, shorter fuse, more mines.

---

### sub_tgun — Projectile (Instant, Rare)

#### Design Identity
Twin-barrel rapid fire. Two alternating barrels fire white projectiles in quick succession — a constant stream of death. The stalker's signature weapon, extracted for the player. The rare-tier projectile that bridges the gap between normal weapons and channeled elites.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Rare |
| Feedback cost | 1 per shot |
| Fire rate | 100ms between shots (alternating barrels, 200ms per barrel) |
| Damage per hit | 25 |
| Effective DPS | 250 (25 dmg × 10 shots/sec) |
| Projectile speed | 3500 units/s |
| Projectile TTL | 1000ms |
| Effective range | ~3500 units |
| Pool size | 16 per barrel (32 total) |
| Barrel offset | 8 units perpendicular to aim |
| Activation | Mouse left (hold to fire) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 6 | 6 | 250 theoretical DPS × 0.85 aim = 212 eDPS. Volume fire (10 shots/sec) — misses cost 100ms, not 500ms. |
| SUSTAIN | +3 | +3 | 1 fb/shot (10/sec sustained). Can fire 10s before cap. Best DPS/feedback ratio in the game. |
| REACH | +3 | +3 | 3500u range + volume fire delivery. 10 shots/sec at long range means consistent hits even at distance. |
| **TOTAL** | | **12** | |

#### Balance Notes
Sub_tgun scores 12 against a Rare cap of 15 — **3 under cap**. It's the most efficient sustained DPS weapon in the game: 250 DPS for just 10 feedback/sec means you can fire for a full 10 seconds before hitting feedback cap. No other weapon comes close to that damage-per-feedback ratio.

**Why it's clearly above pea/mgun**: 2.5x the DPS with the same feedback cost per shot. The twin-barrel volume means practical DPS is much closer to theoretical — miss one shot and you lose 100ms, not 500ms. Pea and mgun at 7 are starter weapons; tgun at 12 is the first real DPS upgrade.

**Comparison to flak (14/15)**: Both are rare projectile weapons. Tgun wins at range with sustained 250 DPS and nearly free feedback. Flak dominates burst with 900 DPS + burn DOT but eats feedback 4x faster and at 2/3 the range. Tgun is the reliable workhorse; flak is the aggressive close-range option.

**The bridge to elites**: At 250 DPS, tgun is still 5x below inferno (1250) and 3.6x below disintegrate (900). But unlike channeled elites, tgun doesn't self-destruct from feedback. The 3-point gap to rare cap means room for buffs — secondary effects (armor shred, slow) or raw damage increases.

**Synergies**:
- **Tgun + Gravwell**: High fire rate + clustered enemies = more shots landing. Volume fire into a gravity well is devastating.
- **Tgun + Stealth**: 5x ambush multiplier = 125 damage per shot for 1s. At 10 shots/sec, that's 1250 DPS during the ambush window — elite-tier burst from a rare weapon.
- **Tgun + Aegis**: 10s of invuln + tgun sustained fire = 2500 guaranteed damage with zero risk. The ultimate safe DPS window.

---

### sub_flak — Projectile (Instant, Rare)

#### Design Identity
Fire-themed shotgun blast. Fires a tight cone of 5 burning pellets — high burst at close range, falls off fast with distance. The fire hunter's signature weapon, extracted for the player. First themed subroutine in the game.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Rare |
| Feedback cost | 4 per shot |
| Fire rate | 250ms between shots (4 shots/sec) |
| Pellets per shot | 9 |
| Damage per pellet | 25 |
| Damage per shot | 225 (all pellets hit) |
| Effective DPS | 900 (point blank, all pellets) |
| Speed variation | +/-10% per pellet (cloud-of-shot spread) |
| Burn DOT | 10 DPS per stack, 3s duration, 3 max stacks |
| Burn DPS (sustained) | Up to 30 (3 stacks maintained) |
| Combined DPS | ~930 (point blank + burn) |
| Spread half-angle | 7.5° (15° total cone) |
| Projectile speed | 3500 units/s |
| Projectile TTL | 667ms |
| Effective range | ~2333 units (~2/3 of tgun) |
| Pool size | 32 |
| Activation | Mouse left (hold to fire) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 12 | 12 | 900 direct DPS × 0.80 cone aim = 720 eDPS + 30 burn DOT = ~750 eDPS. Burn persistence adds value beyond raw DPS — damage continues after disengaging. |
| SUSTAIN | +1 | +1 | 4 fb/shot (16/sec sustained). Burns through feedback fast — natural burst windows. Manageable but taxing. |
| REACH | +1 | +1 | 2333u medium range + cone spread forgives aim, but must close distance to be effective — risk exposure. |
| **TOTAL** | | **14** | |

#### Balance Notes
Sub_flak scores 14 against a Rare cap of 15 — **1 under cap**, nearly at ceiling as fire zone difficulty is tuned. The 16 fb/sec drain means sustained fire burns through feedback fast, creating natural burst windows.

**Comparison to sub_tgun (9)**: Flak now significantly outbursts tgun. Tgun wins at range with 250 sustained DPS and negligible feedback drain. Flak dominates close quarters with 900 direct DPS + burn DOT, but costs 4x the feedback and has 2/3 the range. The 15° cone spread forgives aim, but the short range means eating enemy fire.

**Comparison to sub_pea (7) and sub_mgun (7)**: Flak heavily outclasses starters in DPS (900 vs 100) plus burn DOT. The feedback cost and range tax are the balancing constraints.

**Burn DOT uniqueness**: Flak is the first weapon to apply persistent damage. Even if the player disengages after a burst, burn ticks continue for 3s. At 3 stacks (30 DPS), that's 90 total damage over the DOT duration — nearly a full extra shot's worth of damage that requires no aim. This rewards aggressive play patterns.

**Synergies**:
- **Flak + Egress**: Dash in, point-blank burst, dash out. I-frames on approach, burn continues after retreat.
- **Flak + Stealth**: 5x ambush multiplier on 9 pellets = 1125 damage in one shot. Plus burn DOT. Devastating opener.
- **Flak + Gravwell**: Pull enemies close → shotgun blast → burn stacks on clustered targets. The close-range fantasy realized.

---

### sub_ember — Projectile (Instant, Normal)

#### Design Identity
Fire-themed projectile with AOE ignite. Every impact (enemy or wall) splashes burn to nearby enemies in a 100-unit radius. Low single-target DPS, but excels at spreading burn DOT across groups. The fire hunter's area ignition tool.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 3 per shot |
| Fire rate | 300ms between shots (3.33 shots/sec) |
| Damage per hit | 15 |
| Theoretical DPS | 50 |
| Effective DPS | 35 (×0.70 aimed) |
| AOE ignite radius | 100 units on every impact |
| Burn DOT | 10 DPS per stack, 3s duration, 3 max stacks |
| Burn DPS (sustained) | Up to 30 per target (3 stacks maintained) |
| Projectile speed | 3500 units/s |
| Projectile TTL | 720ms |
| Effective range | ~2520 units |
| Pool size | 16 |
| Activation | Mouse left (hold to fire) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 6 | 6 | 50 theoretical DPS × 0.70 aimed = 35 eDPS. Weak single-target, but 100u AOE ignite on every impact spreads burn to groups. Against 3 targets, each shot applies 10 DPS burn × 3 = 30 DPS group DOT on top of direct damage. Group scaling + burn persistence push above raw eDPS. |
| SUSTAIN | +3 | +3 | 3 fb/shot (~10 fb/sec). Very cheap — fire at will. Nearly as free as pea/mgun. |
| REACH | +1 | +1 | 2520u range with aimed fire. 100u AOE ignite splash forgives positioning — don't need to hit the exact target to burn nearby enemies. Medium range + splash delivery. |
| **TOTAL** | | **10** | |

#### Balance Notes
Sub_ember scores 10 against a Normal cap of 10 — **at cap**. The identity is group burn application, not single-target DPS. Direct damage (35 eDPS) is lower than pea (55 eDPS), but the 100u AOE ignite on every impact makes it scale with target density.

**Comparison to sub_pea (7) and sub_mgun (7)**: Ember has lower direct DPS (35 vs 55/70), but every shot splashes burn to nearby enemies. Against solo targets, pea/mgun are better. Against groups, ember pulls ahead through burn stacking. Ember's 2520u range sits between pea's 3500u and flak's 2333u — longer than flak, shorter than pea, with the AOE splash compensating for the range gap.

**Comparison to sub_flak (14)**: Flak delivers massive burst DPS (900 point blank) with burn. Ember fires slower individual shots at lower damage but spreads burn further via the 100u AOE splash. Flak is the close-range burst weapon; ember is the mid-range burn spreader. Flak is Rare (cap 15), ember is Normal (cap 10) — the power gap is intentional.

**Synergies**:
- **Ember + Gravwell**: Pull enemies together → ember shots splash burn to the whole cluster. The 100u ignite radius covers a gravwell's pull zone.
- **Ember + Stealth**: 5x ambush multiplier on 15 damage = 75 damage opener + AOE ignite on the group. Good initiation tool.
- **Ember + Flak**: Different fire weapons for different situations. Ember spreads burn at range; flak melts at close range. Toggle between them based on engagement distance.

---

### sub_boost — Movement (Toggle, Elite)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 11 | 11 | Best-in-class utility: unlimited 2x speed, zero feedback, full steering. Outrun anything, kite indefinitely. No damage output keeps it below 14. |
| SUSTAIN | +3 | +3 | 0 fb toggle. Literally free. Available whenever you want, for as long as you want. |
| REACH | +3 | +3 | Permanent self-effect — always available, no downtime, no positioning constraints. |
| **TOTAL** | | **17** | |

**Assessment**: 3 under elite cap (20). Boost is pure mobility — zero cost, unlimited duration, full control. Against disintegrate at 20, boost's lack of ANY damage output is what keeps POWER at 11 instead of 14. The 3-point gap is real design space — boost could gain a damage component (ram damage, afterburner trail) to close the gap.

---

### sub_egress — Movement (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 7 | 7 | Strong utility: 5x speed burst + full i-frames for 150ms + 50 contact damage. Dodge-attack in one button. |
| SUSTAIN | +1 | +1 | 25 fb + 2s cooldown. Meaningful cost but manageable frequency. Can't spam, but recovers fast enough for tactical use. |
| REACH | +2 | +2 | 600u dash distance + i-frames mean strong delivery — you GO to the target through danger. |
| **TOTAL** | | **10** | |

**Assessment**: At cap. I-frames + contact damage transforms egress into a proper action-game dodge-attack. Dash THROUGH projectiles, mine explosions, and enemy contact. The 25 feedback + 2s cooldown still gate it — you can't spam, and each one eats a quarter of your feedback bar. But when you use it, it FEELS worth the cost.

---

### sub_blaze — Movement (Instant, Normal)

#### Design Identity
Fire dash with flame corridor. Dash through space leaving a burning trail that persists ~4s, applying burn DOT to enemies who walk through it. No contact damage — the corridor IS the damage. Area denial on a movement ability.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 20 |
| Cooldown | 3000ms (3s) |
| Dash duration | 200ms |
| Dash speed | 4500 units/sec |
| Dash distance | ~900 units |
| I-frames | Full duration (200ms) |
| Contact damage | 0 (none) |
| Corridor segments | ~48 deposited along dash path |
| Corridor lifetime | 4000ms (4s) |
| Corridor radius | 30 units per segment |
| Corridor burn | Applies burn stack every 500ms per segment on overlap |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 7 | 7 | Strong utility: i-frame dash (200ms, slightly longer than egress) + persistent flame corridor (~4s area denial with burn DOT). No contact damage, but corridor applies burn stacks to anything that walks through. The area denial identity is unique — you're not just dodging, you're reshaping the battlefield. |
| SUSTAIN | +1 | +1 | 20 fb + 3s cooldown. Slightly cheaper than egress (20 vs 25 fb) but longer cooldown (3s vs 2s). Net sustain is comparable — roughly one use per combat cycle. |
| REACH | +1 | +1 | 900u dash distance + i-frames provide decent self-delivery (longer than egress). But the corridor's value is POSITIONAL — it only damages enemies who walk through it. Unlike egress's contact damage (delivered on arrival), blaze's damage requires enemies to cooperate by pathing through the trail. REACH is lower than egress because the damage isn't guaranteed on use. |
| **TOTAL** | | **9** | |

#### Balance Notes
Sub_blaze trades egress's direct contact damage for persistent area denial. At budget 9, it's 1 under normal cap.

**Comparison to sub_egress (10, Normal)**: Both are movement instants with i-frame dashes, sharing type exclusivity:
- **Egress**: 150ms dash, 4000 speed, 25 fb, 2s cd, 50 contact damage. Dodge-attack — guaranteed damage on arrival.
- **Blaze**: 200ms dash, 4500 speed, 20 fb, 3s cd, 0 contact damage, 4s flame corridor with burn DOT. Dodge-deny — no guaranteed hit, but area denial persists long after the dash.

The 1-point gap (9 vs 10) reflects the reliability difference: egress guarantees 50 damage per use. Blaze's corridor may burn nothing (enemies avoid it) or burn many (enemies forced to path through). Ceiling is higher, floor is lower.

**Synergies**:
- **Blaze + Gravwell**: Pull enemies into the flame corridor. Gravwell holds them in the burn zone — devastating combo. The corridor's weakness (enemies can walk around it) becomes a non-issue when they're pinned.
- **Blaze + Inferno**: Dash through enemies to reposition, corridor burns behind you while you channel inferno forward. Two fire zones, one blocking retreat, one melting the front line.
- **Blaze + Stealth**: Stealth → position → unstealth with blaze dash through enemies → corridor blocks their escape route while ambush damage shreds them.

**Why 1 under cap?** The corridor's value is situational — in open areas enemies path around it, in corridors it's devastating. Room to buff: increase corridor lifetime, add a slow effect to corridor, or add minor contact damage (10-20) during the dash itself.

---

### sub_cauterize — Healing (Instant, Normal)

#### Design Identity
Fire heal with offensive burn aura. Heals less than mend (30 vs 50 HP) but cleanses burn, grants 3s burn immunity, and drops a damaging burn aura at the player's feet. The fire zone version of mend — trades raw healing for fire defense + offensive area denial.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 20 |
| Cooldown | 8000ms (8s) |
| Heal amount | 30 HP |
| Regen boost | 3x for 5000ms (same as mend) |
| Burn cleanse | Removes all burn stacks on activation |
| Burn immunity | 3000ms (3s) — blocks all burn application |
| Aura radius | 80 units |
| Aura duration | 3000ms (3s) |
| Aura burn interval | 500ms per overlap |
| Activation | G key |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 6 | 6 | 30 HP heal (weaker than mend's 50) + regen boost (same 3x for 5s) + burn cleanse + 3s burn immunity + 80u burn aura for 3s. Less raw healing, but adds offensive burn aura AND fire-zone-specific defense (burn immunity). Dual identity (heal + area damage) is unique among healing skills. |
| SUSTAIN | +1 | +1 | 20 fb + 8s cooldown. Slightly faster recovery than mend (8s vs 10s). Meaningful cost, manageable frequency — same sustain tier as mend. |
| REACH | +1 | +1 | Self-targeted heal + 80u burn aura at feet. The aura extends value beyond self (damages nearby enemies), but 80u is close range. 5s regen window provides temporal coverage. |
| **TOTAL** | | **8** | |

#### Balance Notes
Sub_cauterize scores 8 against a Normal cap of 10 — **2 under cap**. The identity is fire-zone healing: less raw HP but a richer package that includes burn defense and offensive area denial.

**Comparison to sub_mend (7, Normal)**: Both are healing instants with G key (type exclusivity means you pick one):
- **Mend**: 50 HP burst + 3x regen boost for 5s. Pure healing, no offensive component. Budget 7.
- **Cauterize**: 30 HP burst + 3x regen boost for 5s + burn cleanse + 3s burn immunity + 80u burn aura for 3s. Less healing, more utility + offense. Budget 8.

The 1-point gap reflects the added dimensions: cauterize trades 20 HP of raw healing for burn cleanse (fire zone defense), burn immunity (3s of burn prevention), and a burn aura (offensive component unique among heals). Against non-fire enemies, mend's +20 HP advantage matters more. In fire zones, cauterize's burn immunity + cleanse are essential.

**The burn aura identity**: Cauterize is the only healing skill that deals damage. The 80u burn aura at the player's feet creates an offensive zone for 3s — enemies within melee range catch fire. This rewards aggressive positioning: heal yourself, then stand your ground as enemies burn around you. The aura is too small for ranged value — you have to be in the fight to benefit.

**Fire defender version**: When fire defenders use cauterize on allies, the heal also cleanses burn + grants immunity to all allies within 100 units. The group cleanse is AI-only — player cauterize is self-only. This asymmetry is intentional: defender cauterize is a team support heal, player cauterize is a self-sustain + melee offense hybrid.

**Synergies**:
- **Cauterize + Blaze**: Blaze dash into enemies, cauterize to heal + drop burn aura on landing. Two fire zones overlapping — corridor behind you, aura at your feet.
- **Cauterize + Immolate**: Pop immolate (6s shield), then cauterize while invuln — burn aura + immolate burn aura stack for devastating close-range damage with zero risk.
- **Cauterize + Flak**: Close-range shotgun burst, cauterize to heal the hits you took while closing distance, burn aura adds persistent damage on top of flak's burst.

---

### sub_immolate — Shield (Instant, Normal)

#### Design Identity
Fire shield with melee burn aura. Shorter invulnerability than aegis (6s vs 10s) but while active, everything within 60 units takes burn DOT. The fire zone version of aegis — trades shield duration for offensive melee-range area denial. Aggression-while-invulnerable.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 30 |
| Cooldown | 30000ms (30s) |
| Shield duration | 6000ms (6s) |
| Invulnerability | Full (same as aegis) |
| Aura radius | 60 units |
| Aura burn interval | 500ms per overlap |
| Visual | Orange hexagon ring (fire-themed aegis) |
| Activation | F key |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 8 | 8 | 6s full invulnerability (40% less than aegis's 10s) + 60u burn aura while active. Binary defense is still powerful at 6s, and the burn aura adds offensive melee-range damage that aegis completely lacks. Trades duration for aggression. |
| SUSTAIN | −1 | −1 | 30 fb + 30s cooldown. Same double-tax as aegis — once per fight AND painful. |
| REACH | +1 | +1 | Self-targeted shield + 6s coverage (less than aegis's 10s = less temporal reach). The 60u burn aura adds spatial coverage but is so small it's melee-only. Net: weaker temporal reach than aegis, partially offset by spatial offense. |
| **TOTAL** | | **8** | |

#### Balance Notes
Sub_immolate scores 8 against a Normal cap of 10 — **2 under cap**. The identity is aggressive invulnerability: shorter god mode, but you burn everything near you while it's up.

**Comparison to sub_aegis (10, Normal)**: Both are shield instants with F key (type exclusivity means you pick one):
- **Aegis**: 10s full invulnerability, no offensive component. POWER 9, SUSTAIN −1, REACH +2. Budget 10.
- **Immolate**: 6s full invulnerability + 60u burn aura. POWER 8, SUSTAIN −1, REACH +1. Budget 8.

The 2-point gap honestly reflects the tradeoff. Aegis's 4 extra seconds of invulnerability are worth more than immolate's burn aura in most situations — 10s of god mode lets you do anything safely. Immolate's 6s is still powerful, but 40% less safety is a real cost. The burn aura partially compensates by adding offensive value, but 60u is melee range — you have to be in danger to benefit.

**The aggro-invuln fantasy**: Immolate rewards a fundamentally different playstyle than aegis. Aegis players pop shield and channel disintegrate from safety. Immolate players pop shield and wade into melee range, letting the burn aura damage everything around them while they're invincible. It's the brawler's shield — less safe, more violent.

**60u radius — deliberately small**: The burn aura radius (60u) is less than ember's AOE splash (100u) and much less than cauterize's aura (80u). This is intentional — immolate's burn is a melee-range effect. You are nose-to-nose with enemies. The invulnerability makes this safe, but you still need to position aggressively to get value.

**Fire defender version**: Fire defenders use immolate as their self-shield. While active, the defender's 60u burn aura damages the player if they're in melee range. This creates a "don't touch me" defensive zone — fire defenders with immolate active are dangerous to approach. Combined with their flee behavior, fire defenders use immolate as a melee deterrent while retreating to heal allies with cauterize.

**Synergies**:
- **Immolate + Cauterize**: Shield up → wade in → cauterize for heal + burn aura. Two burn zones stacking (60u + 80u) while invulnerable. Maximum close-range fire damage.
- **Immolate + Inferno**: Shield up → channel inferno while invulnerable. Burn aura handles melee enemies while inferno melts everything at medium range. Both fire-themed, both reward aggression.
- **Immolate + Gravwell**: Pull enemies in → immolate → burn everything in the gravity well while invulnerable. The pull keeps enemies in the 60u burn radius.

**Why 2 under cap?** The 6s duration is a meaningful sacrifice from aegis's 10s. The burn aura adds unique offensive value but the small radius limits its practical impact. Room to buff: larger aura radius (80u), longer shield (7s), stronger burn application (faster tick rate), or a secondary effect (movement slow on enemies in the aura).

---

### sub_cinder — Deployable (Instant, Normal)

#### Design Identity
Fire mine. Detonates on fuse or contact like a regular mine, but the explosion spawns a lingering fire pool (~3.5s, 100u radius) that burns enemies standing in it. The fire zone version of sub_mine — same deployment risk, but trades instant area denial for persistent burn zone control.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 15 (on detonation) |
| Cooldown | 250ms (placement cooldown) |
| Max mines | 3 |
| Fuse time | 2000ms (player) / 500ms (enemy) |
| Explosion damage | 100 |
| Explosion half-size | 250 units |
| Self-damage | 100 (if in own blast) |
| Detonation burn stacks | 2 (applied to enemies in blast) |
| Fire pool radius | 100 units |
| Fire pool duration | 3500ms |
| Fire pool burn interval | 500ms per overlap |
| Egress detonation | Yes (dash through triggers) |
| Activation | Space key |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 6 | 6 | 100 mine damage + 2 burn stacks on detonation + lingering fire pool (3.5s burn zone, 100u radius). More total damage than base mine through burn DOT + area denial persistence. Pool applies burn every 500ms to anything standing in it. |
| SUSTAIN | +2 | +2 | 15 fb/boom, 3-mine pool with 250ms cooldown. Same economy as sub_mine — deploy all 3 cheaply, damage is delayed. |
| REACH | 0 | 0 | Self-place + fuse delay + self-damage risk (same as mine). Fire pool extends effective time (3.5s zone control after blast) which partially offsets the placement risk, bringing REACH from mine's −1 to 0. |
| **TOTAL** | | **8** | |

#### Balance Notes
Sub_cinder scores 8 against a Normal cap of 10 — **2 under cap**. It's the fire zone upgrade to sub_mine: same deployment mechanics, same risks, but the detonation leaves behind a burning zone that extends the mine's utility window.

**Comparison to sub_mine (6, Normal)**: Both are space-key deployables with identical deployment mechanics:
- **Mine**: 100 blast damage, instant area denial, gone after explosion. Budget 6.
- **Cinder**: 100 blast damage + 2 burn stacks + 3.5s fire pool. Budget 8.

The 2-point gap comes entirely from POWER (+1, burn stacks + pool) and REACH (+1, fire pool extends effective duration from instant to 3.5s). The fire pool transforms the mine from a one-shot trap into an area denial tool — enemies must avoid the zone or eat burn DOT.

**Fire pool as area denial**: The 100u radius pool lasting 3.5s creates a zone that enemies must respect. Unlike blaze corridors (30u segments), cinder pools are large, stationary, and positioned by the player — tactical placement at chokepoints or escape routes. Multiple cinder mines can blanket an area with overlapping fire pools.

**Enemy fire mine version**: Fire mines in fire zones use cinder mechanics. Their pools burn the player on contact, creating "don't stand here" zones that force movement. Fire pools expire (3.5s) well before the mine's respawn timer (10s), so there's no overflow.

**Why 2 under cap?** The fire pool adds meaningful value over base mine but the self-placement limitation and fuse delay still constrain delivery. Room to buff: larger pool radius, longer pool duration, more burn stacks on detonation, or a secondary effect (enemies in pool take increased damage from other sources).

---

### sub_mend — Healing (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 5 | 5 | Minor buff/heal: 50 HP instant + bypasses 2s regen delay + 3x regen for 5s (~75 bonus HP). Dual identity — burst + sustained. |
| SUSTAIN | +1 | +1 | 20 fb + 10s cooldown. Meaningful cost, once per engagement cycle. |
| REACH | +1 | +1 | Self-targeted but the 5s regen window provides temporal coverage — effectiveness extends beyond the button press. |
| **TOTAL** | | **7** | |

**Assessment**: 3 under cap. Mend has a dual identity: burst heal for emergencies + sustained recovery for skilled play. The 50 HP heals you NOW. The regen boost rewards disengaging cleanly — up to 75 additional HP if you dodge everything for 5s. Taking damage resets the regen delay, interrupting the boost. Mend gives you a recovery window you have to EARN.

**Synergies**:
- **Mend + Egress**: Dash to avoid the hit that would interrupt your regen boost. I-frames + regen boost = maximum recovery value.
- **Mend + Aegis**: Pop aegis → mend during invulnerability → guaranteed 5s of uninterrupted boosted regen.
- **Damage-to-feedback interaction**: Taking damage generates feedback (0.5x), which slows regen (base rate vs 2x rate at 0 feedback). Mend's regen boost partially compensates but doesn't eliminate this pressure — you still want to avoid hits.

---

### sub_aegis — Shield (Instant, Normal)

| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 9 | 9 | Binary defensive power: 10s complete immunity to ALL damage. Prevents feedback from hits too. Nothing else in the game matches this effect. |
| SUSTAIN | −1 | −1 | 30 fb + 30s cooldown. Double-taxed — once per fight AND painful. The heaviest sustain cost of any normal skill. |
| REACH | +2 | +2 | Self-targeted but 10s duration provides strong coverage — a third of each fight cycle spent invulnerable. Full control during. |
| **TOTAL** | | **10** | |

**Assessment**: At cap (10). Aegis is the gold standard for defensive normals. 10 seconds of god mode — walk through mine fields, face-tank inferno beams, channel disintegrate without caring about incoming fire. The SUSTAIN −1 reflects the double tax (30 fb + 30s cd), but POWER 9 properly values binary invulnerability. When it's up, nothing else in the game matches it.

---

### sub_stealth — Stealth (Instant, Normal)

#### Design Identity
Cloak and ambush. Go invisible, position for a devastating opening strike, then fade back into combat.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 0 (but requires 0 feedback to activate) |
| Cooldown | 15000ms (starts on unstealth, not on activation) |
| Speed reduction | 0.5x while stealthed |
| Ambush damage | 5x multiplier within 500 units for 1000ms after attack-breaking stealth |
| Ambush shield pierce | Ignores all shields (defender aegis + defender protection) |
| Ambush kill reset | Killing during ambush window resets stealth cooldown to 0 |
| Stealth break | Attacking, physical contact with enemy/mine body, manual toggle (press key again), or enemy vision cone detection |
| Detection bypass | Enemies lose aggro, mine detection radius ignored (mine body contact still triggers instant explosion) |
| Enemy vision cone | Enemies detect player within 100 units AND inside 90° forward vision cone (±45° from facing) |
| Player mine self-damage | Player's own deployed mines deal 100 damage to player if in blast radius |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 7 | 7 | Strong utility: full invisibility + 5x ambush damage + shield pierce + cooldown reset on kill. One-shot tool at close range with any weapon. |
| SUSTAIN | +1 | +1 | 0 fb but gated by 0-feedback requirement (severe constraint). 15s cooldown starts on unstealth. The gate means "free" only when you've earned clean disengagement. |
| REACH | −1 | −1 | Self-only + 0.5x speed while stealthed + breaks on attack/contact/vision cone. Multiple restrictions on delivery. |
| **TOTAL** | | **7** | |

#### Balance Notes
Sub_stealth is a high-skill-ceiling ability with a unique power curve. The 0-feedback gate is a severe activation constraint — you can't stealth mid-combat while feedback is decaying, forcing disengagement before cloaking. With the **damage-to-feedback system** (0.5x ratio), this gate is even harder to clear: taking hits generates feedback, so you can't just stop shooting and evade sloppily. You must disengage AND dodge all incoming damage to reach 0 feedback. This makes stealth activation a genuine skill expression — only players who can cleanly disengage earn the cloak.

The ambush window (5x damage, shield pierce, 1s duration) makes the opening strike devastating. Against a 100HP hunter within 500 units, a single pea shot deals 250 damage — instant kill. The cooldown reset on ambush kill enables chain assassinations if positioned well.

The speed penalty (0.5x) prevents stealth from doubling as a movement ability and creates a real cost to staying cloaked — you're slow and vulnerable to stumbling into enemies. The vision cone detection adds positional gameplay — you must approach from the sides or behind to avoid being spotted.

The cooldown doesn't start ticking until stealth breaks. This means longer stealth durations don't reduce effective cooldown — you always pay the full 15s after unstealthing regardless of how long you were cloaked.

**Scoring notes**:
- POWER 7 captures the full package: invisibility + 5x ambush + shield pierce + cooldown reset on kill. Each piece alone is strong; together they define a high-skill-ceiling assassin kit
- SUSTAIN +1 (not +2 or +3) because the 0-feedback gate is a severe activation constraint — "free" only when you've earned clean disengagement. The gate plus the 15s cooldown makes it meaningfully limited
- REACH −1 because multiple restrictions stack: 0.5x speed, breaks on attack/contact/vision cone. You can go invisible, but delivering the ambush has real constraints
- Cooldown reset on kill is folded into POWER (not REACH) — it amplifies the skill's peak output, not its delivery range
- Vision cone detection is part of the REACH −1 penalty — it adds a spatial awareness requirement to delivery

**Comparison to similar skills**: Stealth occupies a unique niche — no other skill combines invisibility with a damage amplifier. The closest comparison is sub_aegis (also defensive, also long cooldown) but stealth rewards aggression where aegis rewards survival. At budget 7, stealth is 3 under cap — the POWER 7 rating captures the qualitative value of "enemies can't see you" + 5x ambush, while the REACH −1 penalty for break conditions and speed reduction keeps the total honest.

**Mine interactions**:
- Deploying a mine breaks stealth (attack action)
- Pre-placed mines detonating while stealthed do NOT break stealth — intended setup play
- Mine detection radius is bypassed while stealthed (walk through safely)
- Direct contact with mine body: instant explosion + stealth break (stealthed or not)
- Player's own deployed mines damage the player (100 damage) if caught in blast radius

**Potential concerns**:
- Ambush kill reset could enable degenerate chain-assassination loops in dense enemy groups. Monitor during playtesting — may need a minimum cooldown (e.g., 3s even after reset) if it trivializes encounters
- The 0-feedback gate means stealth synergizes strongly with low-feedback weapons (sub_pea at 1 feedback). High-feedback loadouts effectively can't use stealth, which may be intentional class differentiation
- **Damage-to-feedback interaction**: The 0.5x damage→feedback ratio makes stealth harder to activate under fire. A player taking 40 damage while retreating accumulates 20 feedback, requiring ~1.8s of clean evasion. This interacts with sub_egress i-frames — dashing through projectiles avoids both damage AND the feedback that would delay stealth activation. Egress+stealth becomes a natural combo: dash to avoid hits, clear feedback, activate stealth
- Vision cone creates interesting counterplay with enemy AI patterns — wandering enemies are harder to sneak past than stationary ones. Future "alert" states could widen the cone or add 360° detection when enemies are in combat

---

### sub_inferno — Projectile (Channel/Toggle, Elite)

#### Design Identity
Channeled devastation beam. Hold to unleash a continuous stream of fire blobs that blur into a white-hot hellbeam via bloom. Melts everything in your path — for as long as your feedback holds out.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Channel (toggle-like — hold mouse to sustain) |
| Tier | Elite |
| Feedback cost | 25/sec while channeling |
| Cooldown | None (channeled) |
| Damage per hit | 10 per blob |
| Effective DPS | ~1250 (125 blobs/sec × 10 damage) |
| Blob spawn rate | Every 8ms (125/sec) |
| Blob speed | 2500 units/s |
| Blob TTL | 500ms |
| Effective range | ~1250 units |
| Spread | ±5° from aim direction |
| Piercing | Yes — blobs pass through enemies |
| Blob pool | 256 max concurrent |
| Wall collision | Blobs die on wall impact |
| Stealth interaction | Breaks stealth on first frame (with ambush bonus) |
| Sustain window | ~4s before feedback spillover begins (at 0 initial feedback) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 14 | 14 | 1250 theoretical DPS × 0.90 dense spray = 1125 eDPS + 30 burn DOT = ~1155 eDPS. Piercing + burn persistence + instant max stacks. Elite group-wipe territory. |
| SUSTAIN | +3 | +3 | 25 fb/sec but toggle — you control the sustain window. ~4s before spillover, then natural recovery. Burst-and-disengage pattern is efficient. |
| REACH | +2 | +2 | 1250u range + dense spray delivery (±5°, 125 blobs/sec). Good accuracy at medium range, but must close to half disintegrate's range. |
| **TOTAL** | | **19** | |

#### Balance Notes
Sub_inferno scores 19 — **1 under elite cap (20)**. The burn DOT closes the gap with disintegrate significantly. Inferno applies max burn stacks almost instantly (125 hits/sec saturates 3 stacks in the first frame), and burn persists 3s after you stop channeling — 90 bonus damage on every target you touched, for free.

**Why 19 and not 20**: Disintegrate still wins on range (2600 vs 1250) and group scaling (carve sweep hits everything on a line simultaneously). Inferno's burn DOT partially compensates — the persistent damage means even brief sweeps across multiple targets leave them all burning. But disintegrate's ability to engage from safety and wipe groups in a single sweep keeps it 1 point ahead.

**Burn synergy with inferno's identity**: The burn DOT is thematically perfect — you're hosing enemies with fire, of course they burn. Mechanically, it rewards the burst-and-disengage pattern: channel 2s → release → burn ticks for 3s → feedback decays → channel again. The burn fills the gap between bursts, smoothing effective DPS across the full combat cycle.

**The 4-second wall**: At 25 feedback/sec, you hit 100 feedback in 4 seconds from zero. Then spillover eats integrity at 25 HP/sec. But now the burn keeps dealing 30 DPS while you recover, making those recovery windows less costly.

**Stealth synergy**: 5x ambush multiplier = 50 damage per blob for 1 second = 6250 theoretical DPS, plus instant max burn stacks. The ambush burst + lingering burn is devastating.

**Comparison to sub_disintegrate (20)**: Disintegrate is the precision group-wipe. Inferno is the close-range chaos weapon with lingering fire. Disintegrate's extra point comes from safe engagement range + carve sweep scaling. Inferno nearly matches it by combining raw DPS volume with persistent burn damage.

---

### sub_disintegrate — Projectile (Channel, Elite)

#### Design Identity
Finger of god. A focused hitscan laser that carves through everything in its path — sweep it across a group and watch them evaporate. Double inferno's range, deliberate sweep, perfect pierce. The sniper cannon to inferno's flamethrower.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Channel (hold mouse to sustain) |
| Tier | Elite |
| Feedback cost | 22/sec while channeling |
| Cooldown | None (channeled) |
| Damage per frame | 15 |
| Effective DPS | ~900 (15 × 60fps) |
| Range | 2600 units |
| Collision width | 24 units (12 half-width) |
| Carve speed | 120°/sec max rotation (snaps on first frame) |
| Piercing | Yes — hitscan beam hits all enemies on the line simultaneously |
| Wall collision | Beam terminates at wall impact (with particle splash) |
| Stealth interaction | Breaks stealth on first frame (with ambush bonus) |
| Sustain window | ~4.5s before feedback spillover (at 0 initial feedback) |
| Visual | Blob particle beam (purple glow → light purple → white-hot core) with dedicated bloom FBO, wall splash particles |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 14 | 14 | 900 theoretical DPS × 0.95 hitscan = 855 eDPS. But hitscan pierce + 120°/sec carve sweep = 3420 eDPS vs 4 targets. The group-wipe scaling pushes this to elite cap. |
| SUSTAIN | +3 | +3 | 22 fb/sec toggle, ~4.5s before spillover. Slightly better efficiency than inferno (22 vs 25). Natural burst recovery between sweeps. |
| REACH | +3 | +3 | 2600u hitscan — extreme range + perfect accuracy. Engages enemies before they aggro. Beam hits instantly along the full line. |
| **TOTAL** | | **20** | |

#### Balance Notes
Sub_disintegrate is **the elite benchmark** — the skill all other elites are measured against. At budget 20 it defines the elite cap. The finger of god fantasy: hitscan sweep that erases everything in its path, full pierce, 2x inferno range, dedicated bloom FBO for visual drama.

**Why the carve speed is NOT a drawback**: The previous scoring penalized the 120°/sec carve limit at -1, treating it as a restriction vs inferno's instant aim. Playtesting revealed the opposite — the carve sweep IS what makes disintegrate devastating. Sweeping the beam through a group of enemies means every single one on the line takes full 900 DPS simultaneously. The carve isn't a limitation, it's the mechanic that turns "single-target laser" into "finger of god erasing swaths of enemies." Against 4 enemies in a line, that's 3600 effective group DPS. Inferno's blob scatter means far less consistent multi-target damage.

**Comparison to sub_inferno**: Both are channeled projectile elites (mutually exclusive). The design identity split:
- **Inferno**: close-range chaos. ±5° spray, 1250 DPS, 1250 unit range, instant aim. The flamethrower — get in their face and melt everything. Devastating at close range, blobs scatter and dilute at distance.
- **Disintegrate**: long-range precision. Hitscan beam, 900 DPS, 2600 unit range, 120°/sec carve. Finger of god — erase enemies from across the map. Perfect pierce means every target on the line takes full damage. Sweep clears corridors.

**The range gap is massive**: 2600 vs 1250 means disintegrate engages enemies before they can even aggro. You can sweep a corridor clean from complete safety. Inferno has to close to half that distance, putting you in danger. This alone justifies equal scoring despite lower raw DPS (900 vs 1250).

**Hitscan vs projectile**: Disintegrate's damage is instant along the entire beam. Inferno's blobs travel at 2500 units/sec — at max range (1250 units), there's 500ms of travel time. Against moving targets at range, disintegrate's hitscan is categorically more reliable.

**Pierce quality**: Both pierce, but disintegrate's is far more effective. Inferno blobs spread ±5°, so at range they scatter across and between targets. Disintegrate's beam is a perfect line — if 5 enemies are lined up, all 5 take full 900 DPS simultaneously. The effective group DPS scales linearly with targets in the beam path.

**Sustain advantage**: At 22 feedback/sec vs inferno's 25, disintegrate reaches spillover in ~4.5s vs ~4s. Half a second more channel time. Marginal per burst, but over repeated burst-and-recover cycles it adds up.

**Stealth synergy**: 5x ambush multiplier on beam = 75 damage per frame for 1s = 4500 DPS. Comparable to inferno's ambush (6250 DPS). But disintegrate's hitscan means every frame of the ambush window hits at full power — no wasted blobs scattering past the target.

**Scoring notes**:
- Carve sweep is part of POWER — the 120°/sec sweep turns single-target into group-wipe (3420 eDPS vs 4 targets). It's the mechanic that pushes POWER to 14, not a drawback.
- REACH +3 vs inferno's +2: the 2x range gap (2600 vs 1250) plus hitscan delivery (instant hit at full range) is the defining REACH advantage. Inferno's dense spray compensates but only at medium range.
- Hitscan pierce is qualitatively superior to projectile pierce — disintegrate hits all targets on the line simultaneously, while inferno blobs scatter between targets. Both are folded into their respective POWER scores.

**Potential concerns**:
- 2600 range allows engaging enemies before they aggro. This is a feature, not a bug — it's the elite fantasy. If it trivializes patrol encounters, the answer is encounter design (flanking spawns, ambush triggers) not nerfing the beam.
- Full hitscan pierce + 2600 range + carve sweep means a single sweep can delete an entire group. Monitor for encounter-trivializing patterns, but this is intended to feel godlike — it's an elite.
- The carve speed creates counterplay: enemies that flank or scatter force the player to choose targets, and a 180° turn takes 1.5s. Fast enemies (seekers) can exploit this window.

---

### sub_gravwell — Control (Instant, Normal)

#### Design Identity
Gravity well. Deploy a vortex at cursor position that pulls enemies inward and slows them to a crawl. Zero damage — pure crowd control. The ultimate force multiplier: group enemies up, slow them down, then melt them with your weapon of choice.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Type | Control (SUB_TYPE_CONTROL — first of its kind) |
| Feedback cost | 25 |
| Cooldown | 20000ms (20s) |
| Duration | 8000ms (8s) with 2s fade-out |
| Effect radius | 600 units |
| Pull force | 20 u/s (edge) → 300 u/s (center), quadratic ramp |
| Slow factor | 0.6x (edge) → 0.05x (center), quadratic ramp |
| Damage | None |
| Placement | Cursor world position (replaces existing well) |
| Fade-out | Last 2s: pull/slow linearly ramp to zero, blobs collapse inward |
| Stealth interaction | Breaks stealth on activation |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 7 | 7 | Strong control: pull (20–300 u/s quadratic) + slow (0.6x–0.05x quadratic) for 8s. Enemies in the well are functionally trapped. Zero damage, pure force multiplier. |
| SUSTAIN | 0 | 0 | 25 fb + 20s cooldown. Strategic — significant cost AND long cooldown. One well per engagement. Deliberate use only. |
| REACH | +3 | +3 | Cursor placement at any range + 600u AoE radius. Wide area, delivered from safety. |
| **TOTAL** | | **10** | |

#### Balance Notes
Sub_gravwell is the first control-type subroutine and the first skill with zero damage output. At budget 10 it sits right at normal cap. Its power is entirely indirect — it makes every other skill in your loadout dramatically more effective.

**The force multiplier identity**: Gravwell's value scales with what you pair it with. Gravwell + inferno = enemies get dragged into the fire stream. Gravwell + disintegrate = enemies clumped together for a single sweep to erase. Gravwell + mine = place mines in the well, enemies get pulled onto them. Gravwell alone does nothing. Gravwell with a weapon is devastating.

**Why zero damage earns -1**: A skill that deals no damage has a hard cap on what it can accomplish alone. You can trap enemies indefinitely, but you need another skill to actually kill them. This is a real limitation that prevents gravwell from being self-sufficient, which is the defining characteristic of a support/control skill. The -1 captures this.

**Crowd control scoring (+3)**: The pull + slow combination is scored as a single +3 effect ("fight-changing utility"). The pull itself is powerful — 300 u/s at center means enemies near the core are barely moving and continuously dragged back if they try to escape. The slow (0.05x at center = 95% speed reduction) means even if enemies resist the pull, they're crawling. Together, this is the strongest positional control in the game — enemies in the well are functionally trapped.

**The 20s cooldown tax**: At 20s cooldown (-1) with 8s duration, gravwell has 29% uptime. You get one well per engagement. This prevents "permanent crowd control" — you must choose WHEN to deploy for maximum impact. The cooldown starts when you activate, not when the well expires, so if the well lasts the full 8s, you're waiting 12s after it fades for the next one.

**Placement flexibility**: Cursor-based placement (Long range, +3) means you can drop the well into a group of enemies from safety. This is a major advantage over self-targeted skills (mine, aegis) — you don't have to be IN the danger zone to control it. However, the well is placed once and doesn't move — misplacement wastes the entire 20s cooldown.

**Fade-out design**: The 2s fade-out at end of duration is a grace mechanic — pull/slow ramp to zero instead of cutting abruptly. Enemies don't suddenly snap back to full speed. This also provides a visual countdown (blobs collapse inward, radius shrinks) so the player knows the well is expiring.

**Comparison to other normal skills at cap**: Egress (10) is the other normal at cap. Egress is a personal combat tool — dodge + damage in one button press. Gravwell is a strategic tool — deploy to control a fight. Both earn their 10 through different paths: egress through concentrated personal power, gravwell through broad positional control.

**Scoring notes**:
- SUSTAIN 0 captures the strategic cost: 25 fb one-time + 20s cooldown = deliberate use only. Not as punishing as aegis/EMP (−1) because the one-time cost is lighter than a channeled drain, but the long cooldown prevents spam.
- The 20s cooldown is the primary balance lever — gravwell would be oppressive at 10s, game-breaking at 5s.
- Zero damage is folded into POWER at 7 (strong control, but pure force multiplier). The well does nothing alone; it earns its POWER through pull + slow utility that amplifies everything else.

**Synergies**:
- **Gravwell + Inferno**: Pull enemies into a tight cluster, then hose them down. The slow keeps them in the fire stream. Devastating combo.
- **Gravwell + Disintegrate**: Enemies clumped together = one sweep erases the group. The slow prevents scatter. Perfect for corridor control.
- **Gravwell + Mine**: Place mines inside the well radius. Enemies get pulled onto the mines. Area denial squared.
- **Gravwell + Stealth**: Drop the well as a distraction, then stealth flank for an ambush. The well holds enemy attention and position while you reposition.
- **Gravwell + Egress**: Dash through the clustered enemies for multi-target contact damage. The slow prevents them from scattering before you arrive.

**Potential concerns**:
- Gravwell + channeled elite may trivialize encounters where enemies spawn in groups. The combo is intentionally powerful — it costs 2 of 10 skill slots and a 20s cooldown. If it's still too strong, the cooldown is the tuning lever.
- No damage means gravwell is dead weight against single tough enemies (bosses). This is by design — control skills should be encounter-dependent, not universally useful.
- The 600-unit radius is massive. Enemies at the edge experience modest pull (20 u/s) and modest slow (0.6x), but the center is essentially a trap. Monitor whether the effective trap radius is too generous.
- The "replaces existing well" mechanic means you can't stack wells. This prevents degenerate permanent-CC chains with cooldown reduction (if that ever exists).

---

### sub_emp — Control (Instant, Normal)

#### Design Identity
AoE feedback bomb. Detonate an electromagnetic pulse that maxes out enemy feedback and halves their feedback decay for 10 seconds. The corruptor's signature ability, now extracted for the player. Doesn't deal damage directly — it overloads the enemy's ability to use skills, forcing feedback spillover into their HP.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 30 |
| Cooldown | 30000ms (30s) |
| AoE radius | 400 units (square half-size) |
| Effect on enemies | Max feedback + halved decay for 10s |
| Effect on player (corruptor version) | Max feedback + halved decay for 10s |
| Visual | Expanding blue-white double ring (167ms) |
| Audio | bomb_explode.wav |
| Activation | V key |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 6 | 6 | Moderate control/debuff: max feedback + halved decay for 10s on all enemies in radius. Forces feedback spillover — enemy skills cost HP. No direct damage, but encounter-defining debuff. |
| SUSTAIN | −1 | −1 | 30 fb + 30s cooldown. Double-taxed like aegis — once per fight AND painful. The heaviest sustain cost for a control skill. |
| REACH | +3 | +3 | 400u AoE + hits through walls. No LOS required — reliable delivery in any geometry. |
| **TOTAL** | | **8** | |

#### Balance Notes
Sub_emp is the second control-type subroutine and the player version of the corruptor's EMP attack. At budget 8, it's 2 under normal cap. Its power is entirely indirect — it doesn't deal damage, but it forces enemies into feedback spillover, meaning their own ability usage damages them.

**The debuff mechanic**: EMP maxes enemy feedback to 100 and halves their decay rate. Since enemies use feedback to cast abilities (and excess feedback spills into HP damage), this effectively turns every enemy skill activation into self-harm for 10 seconds. Aggressive enemies like hunters and stalkers will burn themselves down trying to fight. Passive enemies like defenders are less affected since they cast less frequently.

**Symmetry with the corruptor**: The corruptor uses EMP offensively against the player — maxing player feedback and halving decay. The player version mirrors this against enemies. Same mechanic, symmetric application. The corruptor version forces the player to stop using skills or eat spillover damage. The player version does the same to enemies.

**The SUSTAIN −1 double tax**: Like aegis, EMP pays both a high feedback cost (30) and a glacial cooldown (30s). This is why both earn SUSTAIN −1. The double tax is appropriate because the effect is encounter-defining — 10s of halved feedback decay across all enemies in a 400-unit radius is devastating. You get one EMP per fight.

**Comparison to sub_gravwell**: Both are normal control skills. Gravwell (10) physically traps enemies — pull + slow. EMP (8) metabolically cripples them — feedback overload. Gravwell has better uptime (20s cd vs 30s) and is self-sufficient as a force multiplier. EMP's value scales with how aggressively enemies use abilities, making it situational. The 2-point gap seems right.

**Synergies**:
- **EMP + Gravwell**: Trap enemies together, then EMP the cluster. Enemies pulled tight, unable to use abilities effectively.
- **EMP + Inferno/Disintegrate**: EMP to disable enemy retaliation, then channel damage freely during the debuff window.
- **EMP + Stealth**: EMP to cause chaos, disengage, then stealth while enemies are debuffed and confused.

---

### sub_sprint — Movement (Instant, Normal)

#### Design Identity
Timed speed boost. Activate for 5 seconds of 2x movement speed, then wait 15 seconds to use again. The non-elite alternative to sub_boost — same top speed, but gated by duration and cooldown instead of always-on.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 0 |
| Cooldown | 15000ms (starts when sprint expires) |
| Duration | 5000ms (5s) |
| Speed | 1600 units/s (FAST_VELOCITY, same as boost) |
| Control during | Full steering |
| Activation | Shift key (edge-detect) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 5 | 5 | Minor buff: 2x speed for 5s with full steering. Same top speed as boost, but time-limited. |
| SUSTAIN | +2 | +2 | 0 fb (free activation), but 25% uptime (5s on / 15s off). Efficient when available, just not always available. |
| REACH | +1 | +1 | Self-buff with 5s temporal coverage. The speed window provides meaningful repositioning, but 15s of normal speed between uses. |
| **TOTAL** | | **8** | |

#### Balance Notes
Sub_sprint is the non-elite movement option — the "normal human" speed boost vs boost's "elite unlimited" fantasy. At budget 8, it's 2 under normal cap.

**Comparison to sub_boost (17, Elite)**: Both hit 1600 units/s (2x normal). The gap:
- **Boost**: POWER 11 + SUSTAIN +3 + REACH +3 = 17. Unlimited, zero cost, permanent.
- **Sprint**: POWER 5 + SUSTAIN +2 + REACH +1 = 8. 5s burst, 25% uptime.

The 9-point gap (17 vs 8) is the cost of "always available" vs "sometimes available." Sprint gives non-elite players access to the same top speed but requires timing — you burn your sprint window, then walk for 15 seconds. Boost users never have to think about it.

**Zero feedback cost**: Sprint is free to activate. This is appropriate because the gating is entirely temporal (5s on, 15s off). Adding feedback cost would double-tax an already limited skill. The free cost earns +4 points, which is the main budget contributor.

**The 25% uptime limitation**: Sprint's effective uptime (5s out of every 20s) is a genuine limitation. During the 15s cooldown, you're at normal speed — vulnerable to chasers, unable to reposition quickly. This is captured in SUSTAIN +2 (efficient but not unlimited) and REACH +1 (temporal coverage, not permanent). Playtesting will determine if 25% uptime feels too restrictive.

**Synergies**:
- **Sprint + Egress**: Sprint for positioning, egress for emergency dodges. Two movement tools, different use cases — sprint for sustained repositioning, egress for instant escape + i-frames.
- **Sprint + Stealth**: Sprint to disengage, then stealth during cooldown. The 0.5x stealth speed penalty makes the sprint→stealth transition feel natural — fast escape, then slow sneak.

**Why not score higher?** Sprint could justify a higher score if duration were longer or cooldown shorter. The 2-under-cap position gives room to buff: longer duration (6-7s), shorter cooldown (12s), or a secondary effect (brief i-frames on activation, speed lingers 1s after expiry).

---

### sub_resist — Defense (Instant, Normal)

#### Design Identity
Damage resistance aura. Activate for 5 seconds of 50% damage reduction. The corruptor's support ability — a warm yellow hexagonal glow that halves incoming damage. Doesn't make you invulnerable like aegis, but costs less and recovers faster.

#### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Instant |
| Tier | Normal |
| Feedback cost | 25 |
| Cooldown | 15000ms (starts when resist expires) |
| Duration | 5000ms (5s) |
| Damage reduction | 50% (all sources) |
| Visual | Warm yellow/orange hexagon aura (pulsing) |
| Activation | F key (type exclusivity with aegis) |

#### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | 5 | 5 | Minor buff: 50% damage reduction for 5s. Proportional defense — you still take hits, but they hurt half as much. Also halves feedback from damage (hidden efficiency). |
| SUSTAIN | +1 | +1 | 25 fb + 15s cooldown. Meaningful cost but manageable — can use roughly twice per fight vs aegis's once. |
| REACH | +1 | +1 | Self-buff with 5s temporal coverage. Full control during effect. 25% uptime is serviceable. |
| **TOTAL** | | **7** | |

#### Balance Notes
Sub_resist is the non-binary defensive option — 50% damage reduction instead of aegis's 100% immunity. At budget 7, it's 3 under normal cap.

**Comparison to sub_aegis (10, Normal)**: Both are defensive instants activated with F key (type exclusivity means you pick one):
- **Aegis**: 10s full invulnerability, 30s cooldown, 30 feedback. Binary — POWER 9 but SUSTAIN −1. Budget 10.
- **Resist**: 5s half damage, 15s cooldown, 25 feedback. Proportional — POWER 5 but SUSTAIN +1. Budget 7.

The 3-axis system captures the tradeoff clearly: aegis has massively more POWER (9 vs 5) but pays for it in SUSTAIN (−1 vs +1). Resist is the consistent, always-available defensive option. Aegis is the nuclear button you press once per fight.

**The type exclusivity tradeoff**: Since aegis and resist share F key and shield type, you can't equip both. This is a genuine build choice:
- **Aegis**: Longer protection (10s vs 5s), total immunity, but longer cooldown (30s vs 15s) and higher cost (30 vs 25 feedback). One-per-fight panic button.
- **Resist**: Shorter protection, partial mitigation, but faster recovery and lighter cost. Can use roughly twice per fight. Rewards sustained play over panic activation.

**Feedback cost interaction**: At 25 feedback cost, resist eats a quarter of your feedback bar. Combined with the damage-to-feedback system (hits generate 0.5x feedback), activating resist before a damage spike means you're starting from a higher feedback baseline. The 50% damage reduction also halves the feedback generated from damage (since less damage = less feedback), creating a hidden efficiency bonus.

**Synergies**:
- **Resist + Mend**: Pop resist to halve incoming damage, then mend to heal + boost regen during the damage reduction window. The reduced damage means the regen boost is more likely to survive without interruption.
- **Resist + Inferno**: Channel inferno while resist is active — you're still taking spillover damage from feedback, but incoming enemy damage is halved. Extends effective channel time by reducing the "other" damage source.
- **Resist + Sprint**: Sprint into danger with resist active — repositioning with a damage safety net.

**Why 3 under cap?** The 50% reduction for 5s is honestly strong but not extraordinary. Room to buff: longer duration (6-7s), stronger reduction (60%), or a secondary effect (brief speed boost on activation, damage reflection, feedback reduction while active).

---

## Balance Summary

| Skill | Tier | POWER | SUSTAIN | REACH | Score | Cap | Status |
|-------|------|-------|---------|-------|-------|-----|--------|
| sub_pea | Normal | 4 | +3 | 0 | 7 | 10 | -3 under (precise aim tax, room to buff) |
| sub_mgun | Normal | 4 | +3 | 0 | 7 | 10 | -3 under (same eDPS as pea, room to differentiate) |
| sub_mine | Normal | 5 | +2 | −1 | 6 | 10 | -4 under (area denial utility, unreliable DPS) |
| sub_cinder | Normal | 6 | +2 | 0 | 8 | 10 | -2 under (fire mine + lingering burn pool area denial) |
| sub_tgun | Rare | 6 | +3 | +3 | 12 | 15 | -3 under (best sustained DPS/feedback ratio) |
| sub_flak | Rare | 12 | +1 | +1 | 14 | 15 | -1 under (fire shotgun + burn DOT, nearly at cap) |
| sub_ember | Normal | 6 | +3 | +1 | 10 | 10 | At cap (AOE ignite on impact, group burn applicator) |
| sub_boost | Elite | 11 | +3 | +3 | 17 | 20 | -3 under (pure mobility, no damage) |
| sub_sprint | Normal | 5 | +2 | +1 | 8 | 10 | -2 under (timed boost, 25% uptime) |
| sub_inferno | Elite | 14 | +3 | +2 | 19 | 20 | -1 under (highest eDPS + burn DOT) |
| sub_disintegrate | Elite | 14 | +3 | +3 | 20 | 20 | At cap — THE elite benchmark |
| sub_gravwell | Normal | 7 | 0 | +3 | 10 | 10 | At cap (pull + slow, zero damage) |
| sub_emp | Normal | 6 | −1 | +3 | 8 | 10 | -2 under (AoE feedback debuff) |
| sub_stealth | Normal | 7 | +1 | −1 | 7 | 10 | -3 under (invisibility + ambush) |
| sub_egress | Normal | 7 | +1 | +2 | 10 | 10 | At cap (i-frames + contact damage) |
| sub_mend | Normal | 5 | +1 | +1 | 7 | 10 | -3 under (burst heal + 3x regen boost) |
| sub_aegis | Normal | 9 | −1 | +2 | 10 | 10 | At cap (binary invuln properly valued) |
| sub_resist | Normal | 5 | +1 | +1 | 7 | 10 | -3 under (50% DR, non-binary aegis alt) |
| sub_blaze | Normal | 7 | +1 | +1 | 9 | 10 | -1 under (fire dash + flame corridor, area denial dodge) |
| sub_cauterize | Normal | 6 | +1 | +1 | 8 | 10 | -2 under (fire heal + burn aura, offensive healing) |
| sub_immolate | Normal | 8 | −1 | +1 | 8 | 10 | -2 under (shorter shield + melee burn aura, aggro invuln) |

### Key Observations

1. **Three-axis system makes scoring reproducible**: POWER + SUSTAIN + REACH. Three questions, three numbers, one total. No more juggling 12 attribute tables with ad-hoc modifiers. Future skills plug into the same formula and get consistent results.

2. **Aim is baked into POWER, not penalized separately**: The aim discount table converts theoretical DPS to effective DPS BEFORE tiering. Pea's 100 DPS × 0.55 = 55 eDPS. Disintegrate's 900 DPS × 0.95 = 855 eDPS. The gap between "on paper" and "in practice" is now part of the number, not a separate penalty.

3. **Disintegrate is THE elite benchmark at 20**: POWER 14 + SUSTAIN +3 + REACH +3. Every elite is measured against it. Inferno (19) trades REACH for burn DOT persistence. Boost (17) trades POWER for pure mobility.

4. **Binary effects are properly valued in POWER**: Aegis scores POWER 9 (binary invulnerability) with SUSTAIN −1 (double-taxed). The old system undervalued aegis at 4 total; the new system correctly scores it at 10 by giving full credit to the effect's power while honestly accounting for the sustain penalty.

5. **SUSTAIN captures the cost/uptime tradeoff in one number**: No more separately scoring feedback cost, cooldown, and duration. Aegis and EMP both get SUSTAIN −1 (double-taxed: high fb + glacial cd). Pea and boost both get SUSTAIN +3 (effectively free). The single axis captures the full cost picture.

6. **SUSTAIN −1 is the "double tax" signal**: Aegis and EMP both pay high feedback + glacial cooldown, earning SUSTAIN −1. This is intentional — binary power (aegis POWER 9) or encounter-defining debuffs (EMP POWER 6) need heavy sustain costs to stay at cap. Skills should generally pay one or the other heavily, not both — paying both means the effect must be extraordinary.

7. **Damage-to-feedback (0.5x) creates hidden value in defensive skills**: Aegis and egress i-frames prevent both integrity loss AND feedback generation. This hidden REACH value isn't directly scored but significantly increases the practical worth of defensive abilities.

8. **No passives exist yet**: The first passive added will test whether slot cost alone is a sufficient budget constraint, or whether passives need explicit budget penalties.

9. **Control skills are force multipliers**: Gravwell (POWER 7) physically traps enemies. EMP (POWER 6) metabolically cripples them. Both earn POWER through enabling other skills, not standalone damage. The POWER gap (7 vs 6) reflects gravwell's more reliable control vs EMP's situational value.

10. **Corruptor symmetry**: Sprint, EMP, and resist are all dual-use — the corruptor uses them against the player, and the player can extract and use them back. The core extraction pattern ensures identical mechanics on both sides.

11. **Fire variants trade specialization for breadth**: Cauterize vs mend (+1 point, less healing but adds burn immunity + offensive aura) and immolate vs aegis (−2 points, less duration but adds melee burn). Fire variants reward aggressive close-range play in fire zones. Both sit under cap with room to buff.

### Recommended Adjustments (Subject to Playtesting)

**Normal weapons (biggest gap to fill)**:

| Skill | Change | Effect on Score | Reasoning |
|-------|--------|-----------------|-----------|
| sub_pea | Increase damage to 75 | POWER 4→5, total 7→8 | 150 DPS × 0.55 = 82 eDPS. Pushes toward POWER 5 threshold. |
| sub_pea | Add slight homing / wider hitbox | POWER 4→5, total 7→8 | Better aim discount (×0.55→×0.70), raises eDPS to 70. |
| sub_mgun | Increase fire rate to 8/sec (125ms) | POWER 4→5, total 7→8 | 160 DPS × 0.70 = 112 eDPS. Crosses into POWER 5. |
| sub_mgun | Add unique mechanic (armor shred, slow) | POWER 4→5, total 7→8-9 | Differentiates from pea beyond fire rate. |
| sub_mine | Reduce fuse to 1.5s | REACH −1→0, total 6→7 | Less time for enemies to escape — reduces REACH penalty. |
| sub_mine | Increase max mines to 5 | POWER 5→6, total 6→7 | More area denial, higher potential DPS. |
| sub_aegis | Reduce cooldown to 20s | SUSTAIN −1→0, total 10→11 | Less double-tax. Caution: pushes above cap. |

**Previously completed adjustments (for the record)**:
- ~~sub_egress: i-frames + 50 contact damage~~ → brought to 10 (at cap)
- ~~sub_mend: regen boost (3x for 5s) + instant regen activation~~ → brought to 7

These are suggestions, not mandates. The point system identifies WHERE imbalances exist. Playtesting determines the RIGHT fixes.

---

## Guidelines for New Skills

When designing a new subroutine:

1. **Score POWER first** — what does this skill DO at peak? For damage skills, compute theoretical DPS, apply the aim discount, add DOT/special effects. For utility skills, compare against the non-damage POWER tier guide.
2. **Score SUSTAIN** — how long can you keep doing it? Factor in feedback cost, cooldown, and uptime. Watch for the "double tax" trap (high fb + long cd = SUSTAIN −1, reserved for extraordinary effects).
3. **Score REACH** — how far and reliably can you deliver? Range × practical accuracy × delivery quality. Self-only skills start low; AoE and hitscan push high.
4. **Sum and check against the cap** — normal ≤ 10, rare ≤ 15, elite ≤ 20
5. **Compare against existing skills of the same type** — a new projectile weapon should be in the same POWER neighborhood as sub_pea (4) and sub_tgun (6)
6. **Identify the skill's identity** — what ONE thing makes this skill special? That axis gets the budget priority. Everything else is tuned to fit.
7. **Playtest, then retune** — the score card gets you to a reasonable starting point. Gameplay feel is the final arbiter.

### New Skill Template

```
## sub_<name> — <Type> (<Category>, <Tier>)

### Design Identity
<What makes this skill special? One sentence.>

### Stat Block
| Attribute | Value |
|-----------|-------|
| Category | Toggle / Instant / Passive |
| Tier | Normal / Rare / Elite |
| Feedback cost | X |
| Cooldown | Xms |
| <Type-specific attributes> | ... |

### Score Card
| Axis | Rating | Points | Reasoning |
|------|--------|--------|-----------|
| POWER | X | X | <theoretical DPS × aim discount = eDPS, or non-damage tier guide comparison> |
| SUSTAIN | +/-X | +/-X | <feedback cost + cooldown + uptime summary> |
| REACH | +/-X | +/-X | <range × accuracy × delivery quality> |
| **TOTAL** | | **X** | |

### Balance Notes
<How does this compare to similar skills? Any concerns?>
```

---

## Open Questions

1. **Passive budget penalty**: Should passives get a POWER floor or a SUSTAIN bonus to account for "always on" value? Or is the slot cost (losing an active ability) sufficient? The 3-axis system handles this naturally — a passive with SUSTAIN +3 (always on, free) needs lower POWER to stay under cap.

2. **Toggle feedback drain scaling**: For future toggles with per-second feedback drain, how does SUSTAIN score them? Current channeled elites (inferno, disintegrate) get +3 because you control the window. A toggle that drains passively without player control might score lower.

3. **Synergy scoring**: The 3-axis system scores skills in isolation. But skills combine — gravwell + inferno is more powerful than either alone. Should synergies affect individual skill budgets, or is that a loadout-level concern?

4. **Damage-to-feedback ratio tuning**: The 0.5x ratio is a starting point. Too high and combat becomes oppressive (can never activate stealth). Too low and it's meaningless. Should different damage sources have different ratios? (e.g., mine contact = 0x since it's a force_kill, environmental hazards = different ratio)

---

## Scoring Formula Reference

This section is the permanent reference for scoring future skills consistently. Someone reading just this section should be able to score any new skill.

### The Formula

**Score = POWER + SUSTAIN + REACH**

| Axis | Range | Question |
|------|-------|----------|
| POWER | 3–14 | What does this skill DO at peak? |
| SUSTAIN | −2 to +3 | How long can you keep doing it? |
| REACH | −2 to +3 | How far and reliably can you deliver? |

**Max = 14 + 3 + 3 = 20** (elite cap). **Min = 3 + (−2) + (−2) = −1** (nothing should score here).

### Budget Caps

| Tier | Cap |
|------|-----|
| Normal | 10 |
| Rare | 15 |
| Elite | 20 |

### Aim Discount Table

Applied to theoretical DPS BEFORE tiering POWER. This bakes aim difficulty into the power number — no separate penalty.

| Delivery Method | Multiplier | Why |
|----------------|------------|-----|
| Hitscan/beam | ×0.95 | Near-perfect accuracy once on target |
| Dense spray (50+ hits/sec) | ×0.90 | Volume compensates for spread |
| Volume fire (5–10 hits/sec) | ×0.85 | Misses cost little time |
| Cone/multi-pellet | ×0.80 | Generous hitzone but range falloff |
| Aimed (2–5 shots/sec) | ×0.70 | Each miss costs real DPS |
| Precise (1–2 shots/sec, narrow) | ×0.55 | Miss = half your damage gone |

### POWER Tier Guide (damage)

| POWER | Effective DPS | Examples |
|-------|--------------|---------|
| 3–4 | <100 eDPS | Starter weapons (pea 55, mgun 70) |
| 5–6 | 100–300 eDPS | Upgraded weapons (tgun 212, flak 750+burn) |
| 7 | + special properties | Burn persistence, DOT, burst combo |
| 8–11 | 500–1000 eDPS | Would-be elite single-target |
| 12–14 | 1000+ eDPS or group-wipe | Elite channeled weapons (inferno, disintegrate) |

### POWER Tier Guide (non-damage)

| POWER | Effect | Examples |
|-------|--------|---------|
| 5 | Minor buff/heal | resist (50% DR 5s), mend (50HP + regen), sprint (timed 2x speed) |
| 6 | Moderate control/debuff, or heal + utility | emp (AoE feedback overload + halved decay), cauterize (30HP + burn immunity + burn aura) |
| 7 | Strong control/utility | gravwell (CC trap), egress (i-frames + contact), stealth (invis + 5x ambush), blaze (i-frames + flame corridor) |
| 8 | Reduced binary defense + offensive component | immolate (6s invuln + 60u burn aura) |
| 9 | Binary defensive power | aegis (10s full invulnerability) |
| 11 | Best-in-class utility | boost (unlimited 2x speed, zero cost) |

### SUSTAIN Tier Guide

| SUSTAIN | Description | Examples |
|---------|-------------|---------|
| −2 | Extreme tax: huge fb + glacial cd | (reserved) |
| −1 | Double-taxed: high fb + long cd. Once per fight. | aegis (30fb + 30s), emp (30fb + 30s), immolate (30fb + 30s) |
| 0 | Strategic: significant cost OR long cd. | gravwell (25fb + 20s) |
| +1 | Moderate: meaningful cost, manageable frequency. | egress (25fb + 2s), blaze (20fb + 3s), mend (20fb + 10s), cauterize (20fb + 8s), flak (16fb/sec), resist (25fb + 15s), stealth (0fb gated + 15s) |
| +2 | Efficient: low cost, decent uptime. | mine (15fb/boom, 3 pool), sprint (0fb, 25% uptime) |
| +3 | Free/unlimited: negligible cost, fire at will. | pea (1fb), mgun (1fb), tgun (10fb/sec best ratio), boost (0fb), inferno (25/sec toggle), disintegrate (22/sec toggle) |

### REACH Tier Guide

| REACH | Description | Examples |
|-------|-------------|---------|
| −2 | Self-only AND restricted | (reserved) |
| −1 | Self-place + drawbacks, or restricted self-buff | mine (fuse + FF), stealth (0.5x speed + break conditions) |
| 0 | Long range but poor practical accuracy | pea (3500u × 0.55 aim), mgun (3500u × 0.70 aim) |
| +1 | Medium range + decent delivery, or self-buff + temporal coverage | flak (2333u + cone), ember (2520u + AOE splash), sprint (5s window), mend (5s regen), cauterize (self + 80u burn aura), resist (5s DR), immolate (self + 6s + 60u burn aura), blaze (900u dash + corridor requires enemy pathing) |
| +2 | Long range + good delivery, or strong self-coverage | inferno (1250u + dense spray), egress (600u + i-frames), aegis (10s invuln) |
| +3 | Extreme range + reliable targeting, wide AoE, or permanent self-effect | disintegrate (2600u hitscan), tgun (3500u + volume fire), gravwell (cursor + 600u AoE), emp (400u AoE through walls), boost (permanent) |

### Worked Example: sub_disintegrate

**Step 1 — POWER**: Theoretical DPS = 900. Delivery = hitscan beam → ×0.95 aim discount → 855 eDPS. Plus: hitscan pierce (all enemies on line take full damage) + carve sweep at 120°/sec (3420 eDPS vs 4 targets). Group-wipe scaling → POWER **14**.

**Step 2 — SUSTAIN**: 22 fb/sec toggle. Player controls the sustain window — channel, release, recover, repeat. ~4.5s before spillover from zero. Natural burst recovery. Toggle control → SUSTAIN **+3**.

**Step 3 — REACH**: 2600 unit hitscan. Extreme range + instant hit along full line. Engages before enemies aggro. Perfect accuracy at any distance. → REACH **+3**.

**Total**: 14 + 3 + 3 = **20**. At elite cap. The benchmark.

### Master Verification Table

All 20 skills — every score expressed as 3-axis decomposition.

| Skill | POWER | SUSTAIN | REACH | Total | Cap | ✓ |
|-------|-------|---------|-------|-------|-----|---|
| sub_pea | 4 | +3 | 0 | 7 | 10 | ✓ |
| sub_mgun | 4 | +3 | 0 | 7 | 10 | ✓ |
| sub_mine | 5 | +2 | −1 | 6 | 10 | ✓ |
| sub_cinder | 6 | +2 | 0 | 8 | 10 | ✓ |
| sub_tgun | 6 | +3 | +3 | 12 | 15 | ✓ |
| sub_flak | 12 | +1 | +1 | 14 | 15 | ✓ |
| sub_ember | 6 | +3 | +1 | 10 | 10 | ✓ |
| sub_boost | 11 | +3 | +3 | 17 | 20 | ✓ |
| sub_sprint | 5 | +2 | +1 | 8 | 10 | ✓ |
| sub_inferno | 14 | +3 | +2 | 19 | 20 | ✓ |
| sub_disintegrate | 14 | +3 | +3 | 20 | 20 | ✓ |
| sub_gravwell | 7 | 0 | +3 | 10 | 10 | ✓ |
| sub_emp | 6 | −1 | +3 | 8 | 10 | ✓ |
| sub_stealth | 7 | +1 | −1 | 7 | 10 | ✓ |
| sub_egress | 7 | +1 | +2 | 10 | 10 | ✓ |
| sub_mend | 5 | +1 | +1 | 7 | 10 | ✓ |
| sub_aegis | 9 | −1 | +2 | 10 | 10 | ✓ |
| sub_resist | 5 | +1 | +1 | 7 | 10 | ✓ |
| sub_blaze | 7 | +1 | +1 | 9 | 10 | ✓ |
| sub_cauterize | 6 | +1 | +1 | 8 | 10 | ✓ |
| sub_immolate | 8 | −1 | +1 | 8 | 10 | ✓ |
