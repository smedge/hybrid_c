# Spec: Subroutine (Skill) Balance Framework

## Purpose

This is a **living document** — updated whenever a skill is added, modified, or retuned. It provides a systematic framework for balancing subroutines against each other using a point-budget system inspired by Guild Wars skill design.

**Core principle**: Every subroutine attribute has a power cost. Powerful attributes (high damage, low cooldown, zero feedback) consume budget. Restrictive attributes (long cooldowns, high feedback, short range) free budget. Total budget is capped per tier. No skill should exceed its tier's cap without deliberate design justification.

**This framework is a starting point, not a replacement for playtesting.** The budget system gives new skills a principled baseline. Playtesting and player feedback refine the numbers. But without a framework, balancing 50+ skills becomes pure guesswork.

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

**Current examples**: sub_pea, sub_mgun, sub_mine, sub_egress, sub_mend, sub_aegis

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
| **TOTAL** | | | **11** |

**Assessment**: 1 over normal cap. The bread-and-butter weapon — intentionally slightly hot because new players need a reliable tool. The low feedback cost is what pushes it over. **Consider**: raising feedback to 2 would drop it to 10. Or accept the +1 overage as "starter weapon privilege."

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
| **TOTAL** | | | **11** |

**Assessment**: Also 1 over cap. Higher fire rate (+3) than pea (+2) but lower damage per hit (+1 vs +2). Same total — they're actually balanced against each other nicely. Same starter weapon consideration applies. **DPS comparison**: pea = 100 DPS (50dmg × 2/sec), mgun = 100 DPS (20dmg × 5/sec). Identical theoretical DPS — mgun trades burst for consistency.

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
| Pool size | 3 max deployed | Pool > 1 | +1 |
| **TOTAL** | | | **11** |

**Assessment**: 1 over cap, but the fuse delay is a significant real-world drawback — enemies can walk away, the mine may not detonate where you want it. The 250ms deploy cooldown is generous but you only get 3 mines total. Practical DPS is much lower than theoretical due to fuse timing. **Consider**: the fuse delay might warrant -2 instead of -1, which would bring this to 10. Or accept the +1 — mines require real skill expression to use effectively.

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
| **TOTAL** | | | **6** |

**Assessment**: Significantly under cap at 6. The 25 feedback cost + 2s cooldown for a 150ms window is expensive. In practice, egress feels like a panic button with punishing costs. **Consider**: This may be intentionally conservative (movement abilities are extremely powerful in bullet hell), but there's room to buff. Options:
- Lower feedback to 15 → +2 feedback → total 7
- Lower cooldown to 1s → +2 cooldown → total 7
- Add brief invulnerability during dash → +2 modifier → total 8
- Increase duration to 250ms → +1 duration → total 7
- Combination of the above to reach 9-10

---

### sub_mend — Healing (Instant, Normal)

| Attribute | Value | Tier | Points |
|-----------|-------|------|--------|
| Feedback cost | 20 | Medium | +1 |
| Cooldown | 10000ms | Slow | 0 |
| Healing | 50 HP | Moderate | +2 |
| Range | Self | Self | 0 |
| Duration | Instant | Instant | 0 |
| **TOTAL** | | | **3** |

**Assessment**: Very under cap at 3. The 10s cooldown (0 points) and 20 feedback (+1) mean this skill's power is almost entirely in the 50HP heal. In practice, mend feels like a rarely-available panic heal. **Consider**: This is the most underbudget skill by far. Options:
- Lower cooldown to 5s → +1 cooldown → total 4 (still low)
- Add minor AoE heal (heal nearby allies in co-op, future) → +1 AoE → total 4
- Add brief damage reduction after heal (1-2s, 50%) → +2 duration/effect → total 5
- Lower feedback to 10 → +2 feedback → total 4
- Increase heal to 75 → +3 healing → total 4
- **Aggressive**: Lower cooldown to 3s + lower feedback to 10 → total 6

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

## Balance Summary

| Skill | Category | Tier | Score | Cap | Status |
|-------|----------|------|-------|-----|--------|
| sub_pea | Instant | Normal | 11 | 10 | +1 over (starter privilege) |
| sub_mgun | Instant | Normal | 11 | 10 | +1 over (starter privilege) |
| sub_mine | Instant | Normal | 11 | 10 | +1 over (fuse drawback may warrant -2) |
| sub_boost | Toggle | Elite | 15 | 14 | +1 over (elite flagship) |
| sub_egress | Instant | Normal | 6 | 10 | **-4 under (candidate for buff)** |
| sub_mend | Instant | Normal | 3 | 10 | **-7 under (significantly underpowered)** |
| sub_aegis | Instant | Normal | 4 | 10 | **-6 under (invuln may exceed point value)** |

### Key Observations

1. **Weapons are balanced against each other**: Pea and mgun both score 11 with identical effective DPS (100 DPS). Mine scores 11 with higher burst but fuse delay. The weapon triangle works.

2. **Defensive/utility skills are all underbudget**: Egress (6), mend (3), and aegis (4) are all well below cap. This suggests either:
   - The scoring tables undervalue defensive effects (likely for aegis — invulnerability is worth more than +3)
   - These skills genuinely need buffs (likely for egress and mend)
   - Or the budget cap should be lower for defensive skills (separate caps by role?)

3. **The feedback + cooldown squeeze**: High feedback AND long cooldown is a double penalty. Aegis pays both (30 feedback + 30s cooldown), which craters its budget. Skills should generally pay one or the other heavily, not both.

4. **Toggles are inherently high-budget**: sub_boost scores 15 purely from having zero cost + unlimited uptime. Future toggles with feedback drain per second would score lower and open up interesting tradeoffs (powerful effect but drains resources).

5. **No passives exist yet**: The first passive added will test whether slot cost alone is a sufficient budget constraint, or whether passives need explicit budget penalties.

### Recommended Adjustments (Subject to Playtesting)

| Skill | Change | Effect on Score | Reasoning |
|-------|--------|-----------------|-----------|
| sub_egress | Reduce cooldown to 1.5s | 6 → 7 | Dash should be available more often in bullet hell |
| sub_egress | Add 150ms invuln during dash | 7 → 9 | Dashing through projectiles should be viable |
| sub_mend | Reduce cooldown to 5s | 3 → 4 | 10s is too punishing for a single heal |
| sub_mend | Increase heal to 75 HP | 4 → 5 | 50 HP when you have 100 max should feel impactful |
| sub_mend | Reduce feedback to 10 | 5 → 6 | Healing shouldn't cost as much as a dash |
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
