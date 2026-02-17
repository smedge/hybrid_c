# Spec: Subroutine (Skill) Balance Framework

## Purpose

This is a **living document** — updated whenever a skill is added, modified, or retuned. It provides a systematic framework for balancing subroutines against each other using a point-budget system inspired by Guild Wars skill design.

**Core principle**: Every subroutine attribute has a power cost. Powerful attributes (high damage, low cooldown, zero feedback) consume budget. Restrictive attributes (long cooldowns, high feedback, short range) free budget. Total budget is capped per tier. No skill should exceed its tier's cap without deliberate design justification.

**This framework is a starting point, not a replacement for playtesting.** The budget system gives new skills a principled baseline. Playtesting and player feedback refine the numbers. But without a framework, balancing 50+ skills becomes pure guesswork.

---

## Executive Summary

**Budget caps**: Normal = 10, Elite = 14. Over cap = powerful, under cap = room to buff.

| Skill | Type | Tier | Cat | Score | Cap | DPS | Feedback | Cooldown | Verdict |
|-------|------|------|-----|-------|-----|-----|----------|----------|---------|
| sub_pea | Projectile | Normal | Instant | **10** | 10 | 100 | 1/shot | 500ms | At cap. Reliable but must aim. High skill ceiling. |
| sub_mgun | Projectile | Normal | Instant | **10** | 10 | 100 | 2/shot | 200ms | At cap. Same DPS as pea, more forgiving spray. |
| sub_mine | Deployable | Normal | Instant | **10** | 10 | varies | 15/boom | 250ms | At cap. High burst (100dmg) but fuse delay + self-damage risk. |
| sub_boost | Movement | **Elite** | Toggle | **15** | 14 | — | 0 | None | +1 over. Free unlimited speed. The mobility fantasy. |
| sub_inferno | Projectile | **Elite** | Channel | **15** | 14 | ~1250 | 25/sec | None | +1 over. Melts everything. Self-destructs in ~4s from feedback. |
| sub_stealth | Stealth | Normal | Instant | **7** | 10 | — | 0 (gated) | 15s | -3 under. Invisibility power exceeds point value. High skill ceiling. |
| sub_egress | Movement | Normal | Instant | **10** | 10 | — | 25 | 2s | At cap. I-frames + 50 contact damage. Dodge-attack. |
| sub_mend | Healing | Normal | Instant | **7** | 10 | — | 20 | 10s | -3 under. Burst heal + 3x regen boost for 5s. Rewards clean play. |
| sub_aegis | Shield | Normal | Instant | **4** | 10 | — | 30 | 30s | **-6 under.** Invuln is binary — point system undervalues it. |

**Key takeaways**:
- **All normal weapons land at cap (10)**: pea, mgun, and mine are perfectly balanced once aiming difficulty is accounted for. Pea/mgun require precise aim against moving targets — their theoretical 100 DPS drops hard with missed shots. Mine avoids the aiming problem (AoE) but pays with fuse delay and friendly fire.
- **Inferno's DPS advantage is real but earned**: ~1250 DPS with forgiving aim (wide spread, 125 blobs/sec) vs 100 DPS that requires pixel-perfect tracking. The gap is intentional — inferno pays with elite exclusivity and a 4-second feedback death clock.
- **Elites are matched**: boost and inferno both at 15. Opposing fantasies (mobility vs destruction), one per bar.
- **Defense/utility is improving**: egress at cap (10) with i-frames + contact damage, mend at 7 with regen boost. Aegis (4) remains the outlier — the feedback+cooldown double penalty craters its budget.
- **Stealth is deceptive**: scores 7 but "enemies can't see you" + 5x ambush is qualitatively stronger than the points suggest. Similar to aegis — binary powers defy point budgets.

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

**Current examples**: sub_pea, sub_mgun, sub_mine, sub_egress, sub_mend, sub_aegis, sub_stealth

**Future examples**: AoE blasts, teleports, summon commands, cleanse/purify, gravity wells

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

Each attribute tier contributes points to the skill's total budget. The total is compared against the tier cap.

### Budget Caps

| Tier | Budget Cap | Description |
|------|-----------|-------------|
| **Normal** | **10** | Standard subroutines. Most skills live here. |
| **Elite** | **14** | Exceptional subroutines. Clearly more powerful, but balanced against other elites. Progression rewards — feel special without being game-breaking. |

### Scoring Tables

**Feedback Cost** (per activation for instants, per second for toggles):

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| Free | 0 | +4 | No resource cost is extremely powerful — unlimited spam |
| Negligible | 1-5 | +3 | Barely noticeable drain |
| Low | 6-15 | +2 | Manageable, allows sustained use |
| Medium | 16-25 | +1 | Noticeable cost, limits spam |
| High | 26-35 | 0 | Significant investment per use |
| Punishing | 36+ | -1 | Actively painful, risks spillover damage |

**Cooldown** (instants only, toggles get +1 for "no cooldown"):

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| None | 0 / toggle | +4 | Available on demand, always ready |
| Rapid | < 300ms | +3 | Effectively spammable |
| Fast | 300ms - 1s | +2 | High uptime weapon |
| Medium | 1s - 5s | +1 | Tactical timing required |
| Slow | 5s - 15s | 0 | Strategic resource — choose your moment |
| Very Slow | 15s - 30s | -1 | Once-per-fight ability |
| Glacial | 30s+ | -2 | Once-per-encounter, defines the fight |

**Damage Per Hit**:

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| None | 0 | 0 | Non-damage skill |
| Low | 1-25 | +1 | Chip damage, meant for sustained fire |
| Medium | 26-75 | +2 | Solid hit, meaningful per impact |
| High | 76-150 | +3 | Heavy hit, fight-changing |
| Devastating | 151+ | +4 | One-shot territory for weaker enemies |

**Range**:

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| Self | 0 (self-only) | 0 | No reach advantage |
| Melee | < 200 units | +1 | Must be close |
| Medium | 200-500 units | +2 | Comfortable fighting distance |
| Long | 500+ units | +3 | Engages before enemies can respond |

**AoE Radius**:

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| Single Target | 0 | 0 | Must aim each hit |
| Small | < 150 units | +1 | Tight cluster only |
| Medium | 150-300 units | +2 | Room-relevant AoE |
| Large | 300+ units | +3 | Encounter-wide effect |

**Duration / Uptime** (for sustained effects):

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| Instant | 0 / one-shot | 0 | No sustained benefit |
| Brief | < 500ms | 0 | Barely there |
| Short | 500ms - 2s | +1 | Meaningful window |
| Medium | 2s - 10s | +2 | Sustained tactical advantage |
| Long | 10s+ | +3 | Dominant for the encounter |
| Unlimited | Toggle/passive | +4 | Always available |

**Healing**:

| Tier | Range | Points | Reasoning |
|------|-------|--------|-----------|
| None | 0 | 0 | No healing |
| Minor | 1-25 HP | +1 | Chip heal |
| Moderate | 26-50 HP | +2 | Meaningful recovery |
| Major | 51-75 HP | +3 | Fight-saving heal |
| Full | 76+ HP | +4 | Near-complete recovery |

**Special Modifiers** (add to total):

| Modifier | Points | Reasoning |
|----------|--------|-----------|
| Full invulnerability | +3 | Negates all damage — godlike when active |
| Invuln during use | +2 | Safety window during activation |
| Area denial | +1 | Controls enemy movement/positioning |
| Escape/reposition | +1 | Gets out of danger |
| Full control during | +1 | No loss of agency while effect is active |
| Piercing | +1 | Hits multiple enemies per activation |
| Pool > 1 (concurrent instances) | +1 | Multiple simultaneous effects |

**Targeting Difficulty** (how hard is it to land hits?):

| Tier | Description | Points | Reasoning |
|------|-------------|--------|-----------|
| Self/Auto | Self-targeted, placed at feet, or homing | 0 | No aiming skill required |
| Forgiving | Wide spread, dense stream, large AoE — point in general direction | 0 | Generous hit zone compensates for aim |
| Precise | Single narrow projectile, must lead moving targets at range | -1 | Miss by a pixel = zero damage. Theoretical DPS ≠ practical DPS |
| Pinpoint | Slow projectile, tiny hitbox, or extreme range precision | -2 | Requires exceptional tracking to achieve stated DPS |

This axis captures the gap between *theoretical* DPS (assuming 100% hit rate) and *practical* DPS (accounting for human aim against moving targets). A 100 DPS weapon you hit 60% of the time is really 60 DPS. A 1250 DPS beam you hit 95% of the time is really 1187 DPS. The aiming tax matters.

**Drawbacks** (subtract from total):

| Drawback | Points | Reasoning |
|----------|--------|-----------|
| Activation delay / fuse | -1 | Enemies can dodge or escape |
| Self-root during cast | -1 | Vulnerable while using |
| LOS required | -1 | Terrain blocks effectiveness |
| Interruptible | -1 | Damage cancels the effect |
| Friendly fire risk | -1 | Can hurt yourself |

---

## Current Skill Score Cards

### sub_pea — Projectile (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 1 per shot | Negligible | +3 |
| Cooldown | 500ms | Fast | +2 |
| Damage per hit | 50 | Medium | +2 |
| Range | 3500 vel × 1000ms TTL = long | Long | +3 |
| AoE | Single target | Single | 0 |
| Duration/Uptime | N/A (projectile) | — | 0 |
| Pool size | 8 concurrent | Pool > 1 | +1 |
| Targeting difficulty | Single narrow projectile, must lead targets | Precise | -1 |
| **TOTAL** | | | **10** |

**Assessment**: Right at normal cap. The bread-and-butter weapon — reliable and cheap (1 feedback) but you have to actually hit things. The Precise targeting drawback reflects the reality that pea's 100 theoretical DPS drops significantly against fast-moving enemies at range. Practical DPS depends heavily on player aim skill, which makes pea a satisfying weapon to master — bad players waste shots, good players snipe consistently.

---

### sub_mgun — Projectile Variant (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 2 per shot | Negligible | +3 |
| Cooldown | 200ms | Rapid | +3 |
| Damage per hit | 20 | Low | +1 |
| Range | Same as pea (long) | Long | +3 |
| AoE | Single target | Single | 0 |
| Duration/Uptime | N/A | — | 0 |
| Pool size | 8 concurrent | Pool > 1 | +1 |
| Targeting difficulty | Single narrow projectile, must lead targets | Precise | -1 |
| **TOTAL** | | | **10** |

**Assessment**: Right at cap, same as pea. Higher fire rate (+3) than pea (+2) but lower damage per hit (+1 vs +2). The rapid fire partially compensates for aiming difficulty — more shots = more chances to connect — but each individual round is still a narrow projectile that can miss. **DPS comparison**: Both 100 theoretical DPS. Mgun's higher shot count means practical DPS degrades more gracefully with imperfect aim (missing 1 of 5 mgun shots loses 20 DPS; missing 1 of 2 pea shots loses 50 DPS). Mgun is more forgiving in practice despite the same Precise rating.

---

### sub_mine — Deployable (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 15 on explosion | Low | +2 |
| Cooldown | 250ms deploy | Rapid | +3 |
| Damage per hit | 100 (explosion) | High | +3 |
| Range | Self-place | Self | 0 |
| AoE | 250 unit radius | Medium | +2 |
| Duration/Uptime | N/A | — | 0 |
| Area denial | Controls space | Modifier | +1 |
| Fuse delay | 2000ms armed time | Drawback | -1 |
| Friendly fire | Player mines damage player (100 HP) | Drawback | -1 |
| Pool size | 3 max deployed | Pool > 1 | +1 |
| **TOTAL** | | | **10** |

**Assessment**: Right at normal cap. The friendly fire risk (100 damage — instant kill if you're hurt) and fuse delay are serious real-world drawbacks that earn their -2 combined. Enemies can walk away from a mine, and standing too close to your own detonation kills you. The 250ms deploy cooldown is generous but you only get 3 mines total. Practical DPS is much lower than theoretical due to fuse timing. Mines require genuine skill expression — placement, timing, and self-preservation all matter.

---

### sub_boost — Movement (Toggle, Elite)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 0 | Free | +4 |
| Cooldown | None (toggle) | None | +4 |
| Speed multiplier | 2x (800 → 1600) | — | +2 |
| Duration | Unlimited (hold) | Unlimited | +4 |
| Control during | Full steering | Modifier | +1 |
| **TOTAL** | | | **15** |

**Assessment**: 1 over elite cap (14). This is the flagship elite — it SHOULD feel powerful. Zero cost + unlimited duration + full control is the elite fantasy. **Consider**: If this needs tuning, adding a subtle feedback drain per second (even 0.5/sec) would drop Feedback from +4 to +3, bringing it to 14 exactly. But the "unlimited boost with no cost" is the elite identity — touching it should be a last resort.

---

### sub_egress — Movement (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 25 | Medium | +1 |
| Cooldown | 2000ms | Medium | +1 |
| Speed multiplier | 5x (4000 units/s) | — | +3 |
| Duration | 150ms | Brief | 0 |
| Escape/reposition | Gets out of danger | Modifier | +1 |
| Dash distance | 600 units (4000 × 0.15s) | — | 0 |
| Invuln during dash | Full i-frames for 150ms | Modifier | +2 |
| Contact damage | 50 HP on body collision (100-unit corridor) | Medium | +2 |
| **TOTAL** | | | **10** |

**Assessment**: At cap. The addition of i-frames and contact damage transforms egress from a punishing panic button into a proper action-game dodge-attack. The i-frames let you dash THROUGH projectiles, mine explosions, and enemy contact — rewarding aggressive positioning and precise timing. The 50 contact damage gives it offensive utility: dash into a seeker mid-charge for a counter-hit, or body-check a defender out of its heal cycle. The 25 feedback cost + 2s cooldown still gate it — you can't spam dashes, and each one eats a quarter of your feedback bar. But when you use it, it FEELS worth the cost.

---

### sub_mend — Healing (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 20 | Medium | +1 |
| Cooldown | 10000ms | Slow | 0 |
| Healing | 50 HP instant | Moderate | +2 |
| Regen activation | Bypasses 2s damage delay | Modifier | +1 |
| Regen boost | 3x regen rate for 5s (~75 bonus HP) | Duration effect | +2 |
| Range | Self | Self | 0 |
| Duration | 5s regen window | Short | +1 |
| **TOTAL** | | | **7** |

**Assessment**: Improved from 3 to 7. Mend now has a dual identity: burst heal for emergencies + sustained recovery for skilled play. The 50 HP instant heal saves you NOW. The immediate regen activation + 3x boost rewards disengaging cleanly — if you dodge everything for 5 seconds after mending, you recover up to 75 additional HP (at base regen) or 150 HP (at 0-feedback double rate, capped by max integrity). The catch: taking damage resets the regen delay as usual, interrupting the boost. Mend doesn't make you invincible — it gives you a powerful recovery window that you have to EARN by playing well.

**Synergies**:
- **Mend + Egress**: Dash to avoid the hit that would interrupt your regen boost. I-frames + regen boost = maximum recovery value.
- **Mend + Aegis**: Pop aegis → mend during invulnerability → guaranteed 5s of uninterrupted boosted regen.
- **Damage-to-feedback interaction**: Taking damage generates feedback (0.5x), which slows regen (base rate vs 2x rate at 0 feedback). Mend's regen boost partially compensates but doesn't eliminate this pressure — you still want to avoid hits.

---

### sub_aegis — Shield (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 30 | High | 0 |
| Cooldown | 30000ms | Glacial | -2 |
| Duration | 10000ms (10s) | Long | +3 |
| Full invulnerability | Complete immunity | Modifier | +3 |
| Uptime | 10s / 30s = 33% | — | 0 |
| **TOTAL** | | | **4** |

**Assessment**: Very under cap at 4 despite having the game's most powerful effect (full invulnerability). The glacial 30s cooldown (-2) and high feedback cost (0) do massive budget work. In practice, aegis defines one fight per cycle — incredibly powerful when active, absent for long stretches. **Consider**: The low budget score might be correct — 10 seconds of god mode IS worth a lot, and the 30s cooldown makes it strategic. But the score suggests room to buff. Options:
- Lower cooldown to 20s → -1 cooldown → total 5
- Lower feedback to 20 → +1 feedback → total 5
- Add a secondary effect (damage pulse on activation, brief speed boost when shield ends) → +1-2
- Or accept that aegis is deliberately underbudget because invulnerability's qualitative power exceeds what the point system captures

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
| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 0 (gated by 0-feedback requirement) | Free (conditional) | +3 |
| Cooldown | 15000ms (starts on unstealth) | Slow | 0 |
| Duration | Unlimited (until break) | Unlimited | +4 |
| Damage multiplier | 5x for 1s ambush window | — | +3 |
| Shield piercing | Ambush ignores all shields | Modifier | +1 |
| Cooldown reset on kill | Ambush kill → instant reset | Modifier | +2 |
| Speed reduction | 0.5x while stealthed | Drawback | -1 |
| Feedback gate | Must have 0 feedback to activate | Drawback | -2 |
| Breaks on attack | First attack ends stealth | Drawback | -1 |
| Breaks on contact | Physical collision with enemy/mine body ends stealth | Drawback | -1 |
| Vision cone detection | Enemies spot player in 90° forward cone within 100 units | Drawback | -1 |
| **TOTAL** | | | **7** |

#### Balance Notes
Sub_stealth is a high-skill-ceiling ability with a unique power curve. The 0-feedback gate is a severe activation constraint — you can't stealth mid-combat while feedback is decaying, forcing disengagement before cloaking. With the **damage-to-feedback system** (0.5x ratio), this gate is even harder to clear: taking hits generates feedback, so you can't just stop shooting and evade sloppily. You must disengage AND dodge all incoming damage to reach 0 feedback. This makes stealth activation a genuine skill expression — only players who can cleanly disengage earn the cloak.

The ambush window (5x damage, shield pierce, 1s duration) makes the opening strike devastating. Against a 100HP hunter within 500 units, a single pea shot deals 250 damage — instant kill. The cooldown reset on ambush kill enables chain assassinations if positioned well.

The speed penalty (0.5x) prevents stealth from doubling as a movement ability and creates a real cost to staying cloaked — you're slow and vulnerable to stumbling into enemies. The vision cone detection adds positional gameplay — you must approach from the sides or behind to avoid being spotted.

The cooldown doesn't start ticking until stealth breaks. This means longer stealth durations don't reduce effective cooldown — you always pay the full 15s after unstealthing regardless of how long you were cloaked.

**Scoring notes**:
- Feedback cost scored at +3 (not +4) because the 0-feedback gate is a meaningful restriction — it's "free" only when you're out of combat
- Feedback gate scored at -2 because it's a harder restriction than typical drawbacks — it prevents activation entirely in many situations
- The 5x ambush multiplier is scored at +3 (devastating tier equivalent) since it turns any weapon into a one-shot tool at close range
- Cooldown reset is +2 because it enables chain kills but requires execution (must actually get the kill within the ambush window)
- Vision cone detection scored at -1 because it adds a meaningful spatial awareness requirement — you can't just walk anywhere, you must read enemy facing

**Comparison to similar skills**: Stealth occupies a unique niche — no other skill combines invisibility with a damage amplifier. The closest comparison is sub_aegis (also defensive, also long cooldown) but stealth rewards aggression where aegis rewards survival. At budget 7, stealth is 3 under cap, which may be appropriate given the qualitative power of "enemies can't see you" — similar to how aegis's invulnerability may exceed its point value.

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
| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 25/sec | Medium | +1 |
| Cooldown | None (channeled) | None | +4 |
| Damage per hit | 10 per blob | Low | +1 |
| Range | ~1250 units | Long | +3 |
| Duration/Uptime | Unlimited (while held) | Unlimited | +4 |
| Piercing | Blobs pass through | Modifier | +1 |
| Full control during | Can move + aim freely | Modifier | +1 |
| **TOTAL** | | | **15** |

#### Balance Notes
Sub_inferno is the second elite and the game's first channeled weapon. At budget 15 it sits +1 over elite cap — identical to sub_boost's overage. This is deliberate: inferno is the "jaw drop" subroutine, the one that makes players go "holy shit."

The raw numbers are terrifying: 1250 theoretical DPS obliterates anything. A 100HP hunter dies in under 100ms of sustained beam contact. But the practical reality is harsher than it looks:

**The 4-second wall**: At 25 feedback/sec, you hit 100 feedback in 4 seconds from zero. Then spillover damage starts eating your integrity at 25 HP/sec. You're literally killing yourself to channel. Sustained use without a supporting loadout (sub_mend, sub_aegis, low-feedback weapons to let feedback decay between bursts) is suicide. This is the core balance lever — inferno's damage is unlimited but your health isn't.

**Feedback gating vs cooldown gating**: Unlike cooldown-gated weapons (pea, mgun, mine) that pace damage over time, inferno front-loads all its damage into a short window then forces you to stop or die. This creates a natural burst-and-recover rhythm that skilled players will optimize: channel 2-3 seconds → release → let feedback decay → channel again. The effective DPS over a full combat encounter is much lower than 1250.

**Stealth synergy**: Breaking stealth with inferno gives the 5x ambush multiplier to the first blob hits — 50 damage per blob for 1 second. That's 6250 theoretical DPS during the ambush window. Absurd, but it costs you your stealth AND starts the feedback clock. Glass cannon opening.

**Comparison to sub_boost**: Both elites score 15. Boost gives unlimited free mobility — you're untouchable. Inferno gives unlimited damage — everything else is touchable. They're opposing fantasies: one is about never being caught, the other is about nothing surviving. The elite exclusivity (one per bar) makes this a real choice.

**Scoring notes**:
- Damage scored at +1 (Low: 10 per blob) because the point system scores per-hit, not DPS. The fire rate is captured by the +4 cooldown score. The combined effect is devastating, which is the whole point of a channeled elite.
- Duration scored at +4 (Unlimited) because the channel has no inherent time limit — only feedback constrains it
- The 25/sec feedback drain is the primary balance lever. Raising it shortens the sustain window; lowering it makes inferno oppressive

**Potential concerns**:
- Ambush + inferno opening may one-shot bosses/elites in future content. Consider boss-tier damage resistance or ambush multiplier caps for high-HP enemies
- Piercing makes inferno devastating against clustered enemies — groups melt instantly. This may trivialize encounters where enemies spawn together. Consider spawn spacing in encounter design
- The 4-second sustain window may feel too short to casual players. The "loadout tax" (needing support skills to channel longer) is intentional class design — inferno builds sacrifice utility for damage
- Feedback spillover as a self-damage mechanic creates interesting skill expression: knowing exactly when to release the beam separates good players from dead ones

---

## Balance Summary

| Skill | Category | Tier | Score | Cap | Status |
|-------|----------|------|-------|-----|--------|
| sub_pea | Instant | Normal | 10 | 10 | At cap |
| sub_mgun | Instant | Normal | 10 | 10 | At cap |
| sub_mine | Instant | Normal | 10 | 10 | At cap |
| sub_boost | Toggle | Elite | 15 | 14 | +1 over (elite flagship) |
| sub_inferno | Channel | Elite | 15 | 14 | +1 over (elite flagship) |
| sub_stealth | Instant | Normal | 7 | 10 | -3 under (invisibility may exceed point value) |
| sub_egress | Instant | Normal | 10 | 10 | At cap (i-frames + contact damage) |
| sub_mend | Instant | Normal | 7 | 10 | -3 under (burst heal + 3x regen boost) |
| sub_aegis | Instant | Normal | 4 | 10 | **-6 under (invuln may exceed point value)** |

### Key Observations

1. **Normal weapons are perfectly balanced**: Pea, mgun, and mine all score exactly 10. Pea and mgun have identical theoretical DPS (100) but pay for precise aiming. Mine has higher burst (100 damage) but pays with fuse delay and friendly fire. The weapon triangle works — each has a distinct tradeoff identity.

2. **Elites are balanced against each other**: Boost and inferno both score 15 (+1 over elite cap). They represent opposing fantasies — unlimited mobility vs unlimited destruction. The one-elite-per-bar exclusivity makes this a defining build choice.

3. **Defensive/utility skills are improving**: Egress (10) is at cap with i-frames and contact damage. Mend (7) was brought up with regen boost. Aegis (4) remains the outlier — still well below cap. This suggests either:
   - The scoring tables undervalue defensive effects (likely for aegis — invulnerability is worth more than +3)
   - These skills genuinely need buffs (aegis remains the primary candidate)
   - Or the budget cap should be lower for defensive skills (separate caps by role?)

4. **The feedback + cooldown squeeze**: High feedback AND long cooldown is a double penalty. Aegis pays both (30 feedback + 30s cooldown), which craters its budget. Skills should generally pay one or the other heavily, not both.

5. **Inferno's practical DPS advantage over aimed weapons is massive**: Inferno delivers ~1250 DPS with forgiving aim (wide spread, dense blob stream). Pea/mgun deliver 100 theoretical DPS but require precise tracking on moving targets — practical DPS drops to maybe 50-70 for average players. The targeting difficulty scoring (-1 for precise aim) partially captures this, but the real gap is 15-25x in practical DPS, not the 1-point difference the budget shows. This is intentional — inferno is elite and pays with the 4-second feedback death clock, but future normal weapons should consider forgiving targeting as a significant power attribute.

6. **Binary effects defy point budgets**: Stealth (7) and aegis (4) both score low but have qualitatively unique power. "Enemies can't see you" and "you can't take damage" are on/off states — there's no half-measure. The point system inherently undervalues binary effects because it scores magnitude, not the strategic value of absolutes.

7. **No passives exist yet**: The first passive added will test whether slot cost alone is a sufficient budget constraint, or whether passives need explicit budget penalties.

8. **Damage-to-feedback (0.5x) creates hidden value in defensive skills**: Aegis and egress i-frames now prevent both integrity loss AND feedback generation. This hidden value isn't captured in the point budget but significantly increases the practical worth of defensive abilities. A well-timed egress dash through a hunter burst avoids 45 damage AND 22.5 feedback — that's the difference between being able to stealth and being locked out for 2+ seconds. The budget system should probably acknowledge this synergy when evaluating defensive skills.

### Recommended Adjustments (Subject to Playtesting)

| Skill | Change | Effect on Score | Reasoning |
|-------|--------|-----------------|-----------|
| sub_egress | ~~Reduce cooldown to 1.5s~~ | ~~6 → 7~~ | ~~DONE: i-frames + contact damage brought to cap~~ |
| sub_egress | ~~Add 150ms invuln during dash~~ | ~~7 → 9~~ | ~~DONE: implemented as i-frames + 50 contact damage = 10~~ |
| sub_mend | ~~Reduce cooldown to 5s~~ | ~~3 → 4~~ | ~~DONE: regen boost (3x for 5s) + instant regen activation brought to 7~~ |
| sub_mend | ~~Increase heal to 75 HP~~ | ~~4 → 5~~ | ~~DONE: effective total healing now 50 instant + up to 75 HoT~~ |
| sub_mend | ~~Reduce feedback to 10~~ | ~~5 → 6~~ | ~~Kept at 20 — regen boost compensates without cheapening the cost~~ |
| sub_aegis | Reduce cooldown to 20s | 4 → 5 | 30s means aegis is absent for most encounters |

These are suggestions, not mandates. The point system identifies WHERE imbalances exist. Playtesting determines the RIGHT fixes.

---

## Guidelines for New Skills

When designing a new subroutine:

1. **Fill out the score card first** — assign values to every relevant attribute, compute the total
2. **Check against the cap** — normal ≤ 10, elite ≤ 14
3. **Compare against existing skills of the same type** — a new projectile weapon should be in the same budget neighborhood as sub_pea and sub_mgun
4. **Identify the skill's identity** — what ONE thing makes this skill special? That attribute gets the budget priority. Everything else is tuned to fit.
5. **Apply the cooldown/feedback tradeoff** — pick one as the primary gate. High cooldown + high feedback is a double penalty that craters the budget.
6. **Consider category multipliers**:
   - Toggles: zero cooldown is +4 points — the effect itself needs to be moderate or the feedback drain substantial
   - Passives: "always on" is inherently high-value — the effect per-tick needs to be modest
   - Instants: most flexible — cooldown is the primary tuning lever
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
| Tier | Normal / Elite |
| Feedback cost | X |
| Cooldown | Xms |
| <Type-specific attributes> | ... |

### Score Card
| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| ... | ... | ... | ... |
| **TOTAL** | | | **X** |

### Balance Notes
<How does this compare to similar skills? Any concerns?>
```

---

## Open Questions

1. **Separate caps by role?** Should weapons, movement, defense, and utility have different budget caps? Weapons at 10, defense at 8? Or one universal cap?

2. **Invulnerability scoring**: Is +3 enough for full invulnerability? Aegis scores 4 total despite having the most powerful effect in the game. The point system may fundamentally undervalue "binary" effects (you're either invuln or not — there's no "medium invuln").

3. **Passive budget penalty**: Should passives get a flat +2 or +3 added to their score to account for "always on" value? Or is the slot cost (losing an active ability) sufficient?

4. **Toggle feedback drain scaling**: For future toggles with per-second feedback drain, how does the scoring table translate? A toggle draining 3 feedback/sec at 15 feedback/sec decay means net -3/sec feedback accumulation — how many seconds until spillover damage?

5. **Synergy scoring**: The budget system scores skills in isolation. But skills combine — sub_mine + sub_egress (dash-mine placement) is more powerful than either alone. Should synergies affect individual skill budgets, or is that a loadout-level concern?

6. **DPS normalization**: Should we score raw damage per hit, or effective DPS? Pea (50dmg × 2/sec = 100 DPS) and mgun (20dmg × 5/sec = 100 DPS) are identical in DPS but feel very different. The current system scores them separately (damage + cooldown) which produces the same total anyway.

7. **Movement power in bullet hell**: Movement abilities (egress, boost) may be systematically undervalued by the point system. In a bullet hell, repositioning is survival — a 150ms dash that dodges a lethal burst is worth more than any heal. Should movement skills get a genre-specific scoring bonus?

8. **Damage-to-feedback ratio tuning**: The 0.5x ratio is a starting point. Too high and combat becomes oppressive (can never activate stealth). Too low and it's meaningless. Playtesting will determine the sweet spot. Consider: should different damage sources have different ratios? (e.g., mine contact = 0x since it's a force_kill, environmental hazards = different ratio)
