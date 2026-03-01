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

**The Big Question: Enemy Feedback**

Currently, enemies do not have a feedback system. They have HP and that's it. sub_emp against the player is straightforward (spike their feedback bar), but what does sub_emp do when the PLAYER uses it against ENEMIES?

Options to explore:

**Option A: Enemies gain a feedback system**
- Every enemy gets a feedback stat (max 100, decays over time)
- Enemy subroutine usage generates feedback (hunter shooting, seeker dashing, defender healing/shielding)
- At max feedback, enemies can't use their subroutines (stunned/silenced)
- EMP on enemies = spike their feedback, disabling abilities temporarily
- **Pro**: True system unification, deep gameplay implications
- **Con**: Major architectural change, balance implications for ALL enemies, visual/UX complexity

**Option B: EMP is a "silence" effect on enemies**
- EMP disables enemy subroutine usage for N seconds (no sprint, no heal, no shield, no shooting)
- Functionally similar to Option A's outcome but without the full feedback system
- **Pro**: Simpler to implement, clear gameplay effect
- **Con**: Doesn't create the same systemic depth, "silence" is a different mechanic than feedback

**Option C: EMP deals feedback damage to enemies (simplified)**
- Enemies don't have a full feedback bar but EMP deals a burst of "disruption damage" (reduced HP damage + brief slow/stun)
- **Pro**: Easiest to implement
- **Con**: Feels like just another damage ability with extra steps

**Recommendation**: Option A is the most interesting long-term but is a significant architectural investment. Option B gets us 80% of the gameplay for 20% of the work. Worth discussing which path to take before implementation.

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
| Feedback cost (player) | 15-20? |
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

## Open Questions

1. ~~**sub_emp SubroutineType**~~: **RESOLVED** — New type `SUB_TYPE_AREA_EFFECT`, default hotkey V. All hotkeys will eventually be remappable via the Keybinds tab in settings.

2. ~~**Enemy feedback unification**~~: **RESOLVED** — Option A. Full feedback system for enemies. Universal 15/sec decay rate (same as player), 500ms grace period. Per-enemy tuning is only ability costs.

3. ~~**sub_emp feedback cost**~~: **RESOLVED** — 30 feedback (2x mine explosion cost of 15).

4. ~~**Corruptor solo behavior**~~: **RESOLVED** — Solo corruptors are just as aggressive. Sprint in, EMP, resist self for survivability, repeat. No allies? Doesn't matter. These things exist to disrupt, and they do it relentlessly. When solo, resist targets self instead of allies.

5. ~~**EMP friendly fire**~~: **RESOLVED** — No. Enemy EMP only targets the player. When the player uses sub_emp, it only affects enemies. No friendly fire in either direction.

6. ~~**Resist stacking**~~: **RESOLVED** — No stacking. Second cast refreshes duration only.

7. ~~**Visual priority**~~: **RESOLVED** — Corruptors are yellow, swarmers are magenta. No conflict.

8. ~~**Sprint + player movement type exclusivity**~~: **RESOLVED** — Yes, sub_sprint is Movement type. Competes with sub_egress and sub_boost for the same slot. Intended for now; skill activation system may evolve later.
