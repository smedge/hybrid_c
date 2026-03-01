# Corruptor Enemy Spec

## Fiction

A reckless counter-intrusion program that weaponizes the Hybrid's own feedback loop against it. Where defenders protect and healers sustain, the corruptor *disrupts* — sprinting into the fray to deliver devastating electromagnetic pulses that overload the player's subsystems. It also hardens its allies against damage, making the front line harder to crack.

The corruptor is an aggressive support. It doesn't hide behind the line — it *charges through it*.

## Visual Design

- **Shape**: Circle (distinct from all other enemy shapes)
- **Color**: Yellow
- **Size**: Similar to defender (medium)
- **Sprint visual**: Stretched/elongated in movement direction, speed lines or trail
- **EMP visual**: Expanding ring/pulse emanating from corruptor position (yellow → white flash)
- **Resist visual**: Brief yellow tether/beam connecting corruptor to buffed ally, then a subtle glow on the buffed target for the duration

## AI States

- **IDLE** — wander near spawn
- **SUPPORTING** — detected combat nearby, moves to stay near allies. Applies sub_resist to wounded/threatened allies when off cooldown.
- **SPRINTING** — sub_sprint activated, 3x speed, charging toward the player to deliver EMP. Committed movement — once started, the corruptor is going in.
- **EMP_FIRING** — brief windup at point-blank range, then EMP detonation. The "oh shit" moment.
- **RETREATING** — post-EMP, fleeing back toward allies. Vulnerable window.
- **DYING / DEAD** — standard death/respawn

## Core Behavior Loop

1. Corruptor hangs back near allies in SUPPORTING state, casting sub_resist on them
2. When sub_sprint and sub_emp are both off cooldown, and player is within engagement range, corruptor commits: activates sprint, charges toward player
3. Once within EMP range (point-blank), fires EMP
4. Immediately retreats at normal speed (sprint has worn off or is ending)
5. Cycle repeats when cooldowns allow

The key tension: the corruptor is **dangerous but predictable**. The sprint-in is a readable tell. The reward for the player is catching the corruptor during its retreat when it's exposed.

## Stats (Initial Tuning)

| Stat | Value | Notes |
|---|---|---|
| HP | 70 | Squishy — reward for catching it |
| Normal speed | 200 u/s | Slow when not sprinting |
| Sprint speed | 600 u/s | 3x normal (matches sub_sprint) |
| Flee speed | 300 u/s | Faster than idle, slower than sprint |
| Aggro range | 1200 | Same as defender |
| De-aggro range | 3200 | Standard |
| EMP half-size | 250 | Same as mine explosion (500x500 AABB) |
| Resist range | 800 | Can buff allies from a comfortable distance |
| Respawn | 30s | Standard |

## Carried Subroutines

The corruptor uses three subroutines in its AI, all of which are droppable for the player:

### sub_sprint (Movement, Normal Tier)

**Enemy usage**: Corruptor activates sprint to close distance for EMP delivery. The AI only sprints when it intends to EMP — it doesn't sprint randomly.

**Player usage**: Activate to boost movement speed by 3x for 5 seconds. 15 second cooldown. No feedback cost.

| Attribute | Value |
|---|---|
| Type | Movement |
| Tier | Normal |
| Speed multiplier | 3x |
| Duration | 5 seconds |
| Cooldown | 15 seconds |
| Feedback cost | 0 |

**Design note**: sub_sprint vs sub_boost — sprint is a timed burst with a cooldown (normal tier), boost is unlimited hold-to-use (elite tier). Sprint is the accessible version; boost is the premium upgrade. They're the same SubroutineType (MOVEMENT) so the player picks one or the other per loadout slot.

**Design note**: sub_sprint vs sub_egress — egress is a 150ms dash burst with i-frames and contact damage. Sprint is a sustained 5s speed buff with no invulnerability. Different use cases: egress is reactive (dodge an attack), sprint is proactive (reposition, chase, flee). Same type exclusivity applies.

### sub_emp (???, Rare Tier)

**Enemy usage**: After sprinting to point-blank range, the corruptor detonates an EMP that maxes the player's feedback to 100 and halves feedback decay rate for 10 seconds. 30 second cooldown.

**Player usage**: Point-blank AOE detonation. Affects all enemies in range. 30 second cooldown.

| Attribute | Value |
|---|---|
| Type | Area Effect (SUB_TYPE_AREA_EFFECT, hotkey V) |
| Tier | Rare |
| AOE half-size | 250 units (same as mine explosion, 500x500 AABB) |
| Player effect | Feedback spiked to 100, decay rate halved for 10s |
| Enemy effect | Feedback spiked to 100, decay rate halved for 10s |
| Cooldown | 30 seconds |
| Feedback cost | 30 (2x mine explosion) |
| Visual | Blue-white expanding explosion (same geometry as mine blast, blue palette) |

**Enemy Feedback**: See the Feedback Unification section below for full details. Enemies have the same feedback system as the player — universal 15/sec decay, spillover damages integrity, and per-instance aggression determines how far past the limit they'll push.

### sub_resist (Shield?, Normal Tier)

**Enemy usage**: Corruptor targets the most threatened nearby ally (lowest HP % or currently under attack) and grants them 50% damage resistance for 5 seconds. 15 second cooldown.

**Player usage**: Self-cast. Grants the player 50% damage resistance for 5 seconds. 15 second cooldown.

| Attribute | Value |
|---|---|
| Type | Shield (SUB_TYPE_SHIELD) |
| Tier | Normal |
| Damage reduction | 50% |
| Duration | 5 seconds |
| Cooldown | 15 seconds |
| Feedback cost | 25 |
| Target (enemy) | Single ally |
| Target (player) | Self |

**Design note**: sub_resist vs sub_aegis — aegis is full invulnerability for 10s with a 30s cooldown (expensive, powerful). Resist is 50% reduction for 5s with a 15s cooldown (cheaper, more frequent, less powerful). Aegis is the "oh shit" button; resist is the "I'm about to take sustained damage" choice. Same type exclusivity.

## AI Detail

### Target Selection for sub_resist
Priority order:
1. Ally currently taking damage (was hit in last 500ms)
2. Ally with lowest HP percentage
3. Ally closest to player (most threatened by proximity)
4. No valid ally → resist self

When solo, resist always targets self.

### Sprint-EMP Commit Logic
Corruptors are relentless. Conditions to trigger:
- Both sub_sprint and sub_emp are off cooldown
- Player is within engagement range (800-1200 units — close enough to sprint to)
- Player is NOT shielded (sub_aegis active) — waste of an EMP
- Solo or grouped, doesn't matter — charges in regardless

Once sprint is activated, the corruptor is **committed** — it doesn't abort mid-sprint. This gives the player a readable window: "yellow thing coming fast = EMP incoming."

When solo, the corruptor uses sub_resist on itself for survivability between EMP runs.

### Post-EMP Retreat
After EMP fires, the corruptor immediately enters RETREATING state:
- Moves away from player toward nearest ally cluster
- Normal speed (sprint is on cooldown)
- Very vulnerable during this window — reward for surviving the EMP

### Positioning (SUPPORTING state)
Similar to defender positioning:
- Stay within resist range of allies (800 units)
- Keep distance from player (preferred 600-800 units)
- Flee trigger if player gets within 400 units (unless sprinting for EMP)

## Player Counterplay

- **Read the sprint**: Yellow circle suddenly moving 3x speed = EMP incoming. Dash away, shield up, or burst it down before it arrives
- **Punish the retreat**: After EMP, corruptor is slow and exposed. Chase it down
- **Snipe from range**: sub_mgun/sub_pea can reach corruptors in their backline position
- **Break resist with burst**: If corruptor resists an ally, either wait 5s or switch targets
- **Kill order**: In a mixed group, corruptors should often be priority 2 (after defenders who are shielding them, or before defenders if the EMP pressure is worse than the healing)
- **sub_aegis timing**: If you see the sprint, popping aegis wastes the corruptor's 30s EMP cooldown

## Gating Role

Corruptor zones punish players who rely on subroutine spam. The EMP forces you to:
- **Fight with high feedback**: Can you survive 10 seconds of limited ability usage?
- **Bring sustain**: sub_mend or sub_resist to handle the ability-starved window
- **Prioritize targets**: Kill the corruptor before it EMPs, or handle the consequences
- **Manage cooldowns**: Don't blow everything before the EMP hits

Corruptor + Hunter composition: hunters provide sustained ranged DPS while the corruptor EMPs your ability to respond. You need to either burst the corruptor through the hunter screen or have enough passive defense to ride out the feedback spike.

Corruptor + Defender composition: defender shields/heals the corruptor as it retreats post-EMP. Need to break the defender support or catch the corruptor during its sprint approach before it reaches you.

## Fragment & Progression

- **Fragment type**: `FRAG_TYPE_CORRUPTOR` (yellow, matching enemy color)
- **Drops**: All three subroutines are droppable (sub_sprint, sub_emp, sub_resist)
- **Fragment thresholds**:
  - sub_sprint (normal): 10 fragments
  - sub_resist (normal): 10 fragments
  - sub_emp (rare): 15 fragments, only drops once both normal corruptor skills are collected (50% drop rate per kill after that)
- **First appearance**: Nexus Corridor (not a starter enemy)

---

## Feedback Unification

Enemies gain a feedback system identical to the player's. This is required for sub_emp to work symmetrically and enriches all combat.

### Universal Rules

| Parameter | Value | Notes |
|---|---|---|
| Feedback max | 100 | Same as player |
| Decay rate | 15/sec | Same as player, universal across all enemies |
| Grace period | 500ms | Decay pauses briefly after feedback is generated |
| EMP debuff | Decay halved (7.5/sec) for 10s | Applied by sub_emp |
| Spillover | Excess feedback above max deals 1:1 integrity damage | Same as player |

### Ability Gate
Before using any ability, an enemy checks: `feedback + cost <= max` (or aggression override, see below). If it can't afford the ability, it skips it — no frozen/stunned state, the enemy still moves, patrols, chases, flees. It just can't fire its abilities.

### Behavior When Feedback-Locked
Enemies are NOT stunned. They continue their normal movement/positioning AI:
- **Hunters**: Chase/patrol but can't fire bursts
- **Seekers**: Stalk/orbit but can't dash
- **Defenders**: Follow allies but can't heal or shield
- **Corruptors**: Position near allies but can't sprint, EMP, or resist
- **Mines**: Unaffected (passive, no ability cost)

The enemy appears impotent but not frozen. Experienced players learn to recognize and exploit these windows.

### Per-Enemy Ability Costs (Initial Tuning)

| Enemy | Ability | Cost | Notes |
|---|---|---|---|
| Hunter | 3-shot burst | 15 | ~6-7 bursts before feedback-locked at sustained fire |
| Seeker | Dash attack | 40 | One dash = significant commitment, 2 dashes = near max |
| Defender | Heal | 20 | Can heal ~5x before dry |
| Defender | Aegis (self-shield) | 30 | Expensive — can't shield AND heal freely |
| Corruptor | Sprint | 0 | Free — sprint is just movement |
| Corruptor | EMP | 30 | Big commitment |
| Corruptor | Resist | 15 | Moderate — can buff a couple allies between decays |

### Aggression Scale (1-10)

Each enemy instance spawns with a random integer aggression value from 1-10. This determines how they handle the feedback limit.

| Aggression | Behavior |
|---|---|
| 1-3 | **Conservative.** Stops using abilities at max feedback. Waits for decay. |
| 4-6 | **Moderate.** Will push past the limit for a couple extra ability uses. Takes some spillover damage but backs off before it gets dangerous. |
| 7-9 | **Aggressive.** Keeps firing well past the limit. Takes significant self-damage from spillover. Will fight through 40-60% HP loss from feedback alone. |
| 10 | **Kamikaze.** No limit. Will burn itself to death through spillover if it means finishing the player. Takes one for the team knowing it respawns in 30 seconds. |

**Implementation**: When `feedback + cost > max`, the enemy rolls against its aggression. Higher aggression = more likely to force the ability through and eat the spillover. Aggression 10 always forces through, no check.

**Spillover cap by aggression** (rough guideline):
- Aggression 1-3: 0 spillover tolerance (never push past max)
- Aggression 4-6: Up to ~20-30 spillover HP loss before stopping
- Aggression 7-9: Up to ~40-60 spillover HP loss before stopping
- Aggression 10: No cap — fires until dead

**EMP + Aggression interaction**: An EMP'd group of aggressive enemies becomes chaotic. Conservative enemies shut down for 10+ seconds. Aggressive ones keep firing, eating massive spillover from the halved decay. A kamikaze enemy that gets EMP'd is essentially on a self-destruct timer as it keeps attacking. This creates emergent variety in every encounter.

**Fragment drop rule**: Fragments only drop when the player lands the killing blow. Enemies that die from their own feedback spillover, or any other non-player source, do not drop fragments.

**Tuning note**: Aggression distribution can be adjusted per zone. Early zones might weight toward 1-5 (forgiving). Late zones might weight toward 5-10 (punishing). Boss encounters could have fixed high aggression.

### Feedback-Locked Visual Tell

The player needs to see when an enemy is tapped out. Options (pick one or combine):
- **Color desaturation**: Enemy color fades/greys out as feedback increases
- **Flicker/stutter**: Enemy geometry briefly flickers when at max feedback
- **Small bar**: Appears above enemy only when feedback > 50%, disappears when drained

Recommend: subtle desaturation + flicker at max. No bar initially — keep the HUD clean, let players learn to read the behavioral tell (it stopped shooting), then add the visual confirmation.

---

## Open Questions

1. ~~**sub_emp SubroutineType**~~: **RESOLVED** — New type `SUB_TYPE_AREA_EFFECT`, default hotkey V. All hotkeys will eventually be remappable via the Keybinds tab in settings.

2. ~~**Enemy feedback unification**~~: **RESOLVED** — Option A. Full feedback system for enemies. Universal 15/sec decay rate (same as player), 500ms grace period. Per-enemy tuning is only ability costs.

3. ~~**sub_emp feedback cost**~~: **RESOLVED** — 30 feedback (2x mine explosion cost of 15).

4. ~~**Corruptor solo behavior**~~: **RESOLVED** — Solo corruptors are just as aggressive. Sprint in, EMP, resist self for survivability, repeat. No allies? Doesn't matter. These things exist to disrupt, and they do it relentlessly. When solo, resist targets self instead of allies.

5. ~~**EMP friendly fire**~~: **RESOLVED** — No. Enemy EMP only targets the player. When the player uses sub_emp, it only affects enemies. No friendly fire in either direction.

6. ~~**Resist stacking**~~: **RESOLVED** — No stacking. Second cast refreshes duration only.

7. ~~**Visual priority**~~: **RESOLVED** — Corruptors are yellow, swarmers are magenta. No conflict.

8. ~~**Sprint + player movement type exclusivity**~~: **RESOLVED** — Yes, sub_sprint is Movement type. Competes with sub_egress and sub_boost for the same slot. Intended for now; skill activation system may evolve later.
