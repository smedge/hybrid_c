# New Enemy Spec: Stalker, Swarmer, Corruptor

## Design Philosophy

**Every enemy is a gate. Every subroutine is a key.**

This game's metroidvania progression is NOT "find the red key for the red door." It's "find the combination of subroutines that counters the enemy composition blocking your path." Areas are gated by difficulty — specific enemy types and combinations that are near-impossible without the right loadout, but manageable once you have it.

The endgame vision is 50+ subroutines. No single loadout steamrolls everything. The player must read the situation, understand what they're up against, and adapt their build. Think Guild Wars build-craft meets bullet hell combat. A plasma cannon build shreds swarms but leaves you exposed to stalkers. A stealth/assassination loadout handles stalkers and corruptors but folds against a wall of hunters with defender support.

**Enemies exist to create demand for subroutines. Subroutines exist to answer enemies. Neither makes sense without the other.**

Current subroutines (sub_pea, sub_mgun, sub_mine, sub_boost, sub_egress, sub_mend, sub_aegis) are the foundation — general-purpose tools. Future subroutines become increasingly specialized, and these three new enemy types are designed to create demand for those specializations.

---

## Stalker (stealth assassin)

### Fiction
An advanced counter-intrusion program designed to silently track and eliminate unauthorized processes. It phases between visible and cloaked states, striking from the shadows.

### Core Mechanic
Stalkers approach the player while nearly invisible. They have a brief "decloak" tell before delivering a devastating backstab attack. Skilled players who spot the tell can react — unskilled players eat the full hit.

### AI States
- **IDLE** — wander near spawn, fully visible
- **STALKING** — cloaked, slowly closing distance to player
- **POSITIONING** — cloaked, circling to get behind the player (opposite of facing direction)
- **DECLOAKING** — the TELL: brief shimmer/distortion effect before strike (300-500ms?)
- **STRIKING** — lunges at player, high damage single hit
- **FLEEING** — post-strike escape, recloaks after reaching safe distance
- **DYING / DEAD** — standard death/respawn

### The Tell (Critical Design Point)
The tell needs to be:
- **Subtle enough** that new players won't always catch it
- **Consistent enough** that experienced players learn to react
- **Fair** — always present, never skipped

Ideas for the tell:
1. **Visual shimmer** — brief screen-space distortion or particle effect at stalker's position
2. **Audio cue** — quiet but distinct sound (like a blade being drawn) that plays during decloak
3. **Bloom flicker** — momentary bloom pulse at the stalker's cloaked position
4. **Trail artifact** — faint afterimage trail visible for ~200ms before the strike
5. **Proximity warning** — the closer the stalker gets, the more visible it becomes (gradient)

Recommended: Combination of #3 (bloom flicker) + #2 (audio cue) + #5 (proximity fade-in). The proximity fade means aware players can spot them early, but the bloom flicker is the "oh shit" moment right before the strike.

### Visibility Model
- **Cloaked**: rendered at very low alpha (0.03-0.05?) — nearly invisible but not zero
- **Proximity reveal**: alpha increases as stalker gets closer to player (ramps from 0.05 to 0.3 within 200 units?)
- **Decloak tell**: rapid alpha pulse (0.0 -> 0.8 -> 0.0) over ~200ms, bloom spike
- **Striking**: fully visible during the lunge
- **Post-strike**: rapid fade back to cloaked

### Stats (initial tuning)
- HP: 50 (glass cannon — fragile if you catch them)
- Cloaked speed: 200 (slow, patient approach)
- Strike damage: 60-80 (punishing but not instant kill from full health)
- Strike lunge speed: 4000 (faster than seeker dash but shorter range)
- Strike range: 300 (must get close)
- Decloak tell duration: 400ms (the reaction window)
- Post-strike flee speed: 500
- Recloak delay: 2s after fleeing
- Respawn: 30s

### Player Counterplay
- Spot the shimmer/proximity reveal and preemptively shoot the area
- React to the decloak tell with sub_egress (dash away) or sub_aegis (shield)
- Area denial with sub_mine forces stalkers to path around
- Shooting a cloaked stalker fully reveals it for 1-2s and interrupts the attack sequence
- Future subroutines: detection/scanner abilities, AoE reveal pulses

### Progression — Subroutine Unlock Direction
Killing stalkers drops fragments toward stealth/phase-shift subroutines:
- **sub_phase** / **sub_cloak** — player stealth, brief invisibility window
- **sub_blink** — short-range teleport (phasing through space rather than dashing through it)
- These create a stealth/assassination playstyle archetype — get in, burst, get out unseen

### Gating Role
Stalker-heavy zones punish players who rely purely on reaction-based combat. You need either:
- Detection subroutines to reveal them before the tell
- Defensive subroutines (aegis, phase) to survive the burst
- Area denial to control their approach paths

### Visual Design
- Shape: thin crescent or sickle (blade-like)
- Color: dark purple/violet when visible
- Cloaked: faint outline only, same purple but nearly transparent
- Strike: bright white flash along the lunge path

---

## Swarmer (splitting swarm)

### Fiction
A distributed security protocol that fragments itself into numerous sub-processes to overwhelm threats through sheer volume. Each fragment is weak individually but collectively they saturate the target's defenses.

### Core Mechanic
Swarmers exist in two forms: a single "mother" entity that patrols, and a swarm of small "drones" that split off when aggroed. The mother splits apart, and the drones converge on the player from multiple angles. Killing all drones (or the last few) ends the swarm. If enough drones survive, they can reform into the mother.

### Why Swarmers Matter (The Big Picture)
Swarmers are **intentionally oppressive** with the current basic subroutine set. Picking off 10 tiny fast drones one-by-one with sub_pea is miserable by design. That's the point — swarmers create demand for AoE and crowd-control subroutines that don't exist yet:

- **Plasma cannon** — wide beam that cuts through the whole swarm
- **Rotating plasma shield** — passive damage aura that melts drones on contact
- **Nuke** — room-clearing burst for when you're surrounded
- **Wormhole/gravity well** — pulls drones together into a tight cluster for an easy killing blow
- **Damage dash** — an egress variant that deals AoE along the dash path

Swarmers are the gate that says "you need better AoE to get past here." They're the reason players WANT those subroutines.

### The Minion Master Angle
Critically, swarmers also unlock the beginning of a **minion master playstyle**. Swarmer fragments lead to drone-summoning subroutines — the player gets their own swarm. But just like a Guild Wars minion master, summoning drones alone isn't enough. You need supporting subroutines to make the build viable:

- A drone summon subroutine (the core)
- A drone heal/sustain subroutine (keep them alive longer)
- A drone command subroutine (focus fire, formation control)
- Energy/feedback management to sustain the army

This is a full build archetype that requires multiple subroutine slots dedicated to the strategy. It's powerful when built correctly but demands commitment — you're giving up direct combat subroutines for your army. The skill expression is in build-craft and resource management, not just aim and reflexes.

### AI States — Mother
- **IDLE** — wander near spawn, slightly larger than other enemies
- **AGGROED** — detected player, begins splitting animation
- **SPLITTING** — expands and bursts into N drones (1-2s animation)
- **REFORMING** — drones converge back to reform mother (if enough survive)
- **DYING / DEAD** — standard

### AI States — Drone
- **SWARMING** — converge on player from split position, weaving/orbiting
- **ATTACKING** — close enough to player, kamikaze contact damage
- **RETREATING** — if player kills enough siblings, remaining drones flee to reform point
- **DYING** — individual drone death (small pop, no respawn — mother respawns)

### Swarm Behavior
- Split count: 8-12 drones per mother
- Drones don't all rush at once — they orbit/weave at different radii, taking turns diving in
- Creates a "bullet hell" feel where you're dodging bodies from multiple directions
- Drones are individually weak but their combined DPS is high if you stand still

### Stats (initial tuning)
**Mother:**
- HP: 120
- Speed: 150 (slow, lumbering)
- Aggro range: 1000
- Split time: 1.5s (telegraph — mother expands/pulses before bursting)

**Drone:**
- HP: 10-15 (one pea shot kills)
- Speed: 600 (fast, erratic)
- Contact damage: 5 per drone
- Orbit radius: 200-400 (varies per drone for visual chaos)
- Dive speed: 1200
- Reform threshold: if <= 3 drones remain, they try to flee and reform

### Player Counterplay
- Kill the mother before it splits (burst it down — reward for aggression)
- sub_mine is devastating against the swarm (area damage hits many drones)
- sub_egress to dash through the swarm ring and escape
- The split telegraph gives time to prepare (place mines, position)
- **Future subroutines are the real answer** — AoE weapons, gravity wells, damage auras

### Progression — Subroutine Unlock Direction
Killing swarmers (mothers) drops fragments toward drone/minion subroutines:
- **sub_drone** — summon attack drones that orbit and dive enemies
- **sub_sustain** / **sub_hive** — heal/reinforce your drone army
- **sub_command** — direct drone behavior (focus fire, defensive formation, scatter)
- This is the foundation of the **minion master archetype**

### Gating Role
Swarmer-heavy zones are the "you need AoE" gate. Players with only single-target weapons will be overwhelmed. Forces acquisition of:
- AoE damage subroutines (plasma cannon, nuke, damage dash)
- Crowd control subroutines (gravity well, slow field)
- Or the minion master path (fight swarm with swarm)

### Visual Design
- Mother: larger hexagonal cluster, pulsing/shifting like cells dividing
- Drones: tiny triangles or diamonds, each slightly different orbit phase
- Color: yellow/amber (distinct from hunter orange)
- Split animation: mother geometry fractures outward
- Reform: drones spiral inward, snap together

---

## Corruptor (debuff support)

### Fiction
A sophisticated counter-intrusion program that doesn't attack directly — instead it corrupts the Hybrid's subsystems, degrading processing efficiency, memory integrity, and subroutine execution. It makes everything else harder.

### Core Mechanic
Corruptors project an aura or apply targeted debuffs that degrade the player's stats. They stay at range, protected by allies (and defenders), making the player's life miserable. The player needs to prioritize killing corruptors or the fight becomes increasingly unwinnable.

### AI States
- **IDLE** — wander near spawn
- **SUPPORTING** — detected combat nearby, moves to stay near allies but away from player
- **CORRUPTING** — in range, actively applying debuffs to player
- **FLEEING** — player gets too close, runs away
- **DYING / DEAD** — standard

### Debuff System
When a corruptor is in CORRUPTING state and has line of sight to the player within its corruption range, it applies debuffs. Multiple corruptors stack (multiplicatively? additively? — TBD).

Possible debuffs (pick 2-3, not all):
1. **Feedback Corruption** — feedback decays slower (or generates faster). Makes skill spam punishing.
2. **Integrity Drain** — health regen is reduced or disabled entirely while in range
3. **Signal Degradation** — player projectile damage reduced by X%
4. **Process Throttle** — player movement speed reduced by X%
5. **Subroutine Interference** — cooldowns take longer to refresh
6. **Static Overlay** — visual noise/distortion on screen (risky — can be annoying)

Recommended core set:
- **Feedback corruption**: feedback decay rate halved (from 15/sec to 7.5/sec)
- **Integrity drain**: health regen disabled while in corruption range
- **Process throttle**: player speed reduced by 25%

These three create pressure without being un-fun. The player can still fight, just less effectively. It creates urgency to kill the corruptor.

### Corruption Model
- Range: 800-1000 units (large area of influence)
- Requires line of sight
- Debuffs apply instantly when in range, removed instantly when out of range or corruptor dies
- Visual indicator: corruption tendrils/beams connecting corruptor to player?
- Player HUD warning when corrupted (purple tint on integrity/feedback bars?)

### Positioning Behavior
- Corruptors behave similarly to defenders — stay near allies but keep distance from player
- They position themselves BEHIND other enemies relative to the player
- Flee trigger range: 500 (run if player gets close)
- Preferred operating distance: 600-800 from player

### Stats (initial tuning)
- HP: 70
- Speed: 200 (normal), 350 (fleeing)
- Aggro range: 1200 (same as defender)
- Corruption range: 800
- Respawn: 30s

### Player Counterplay
- Prioritize killing corruptors early (the MMO rule: kill the support first)
- Long-range pea/mgun can reach them behind enemy lines
- sub_egress to dash past the front line and burst the corruptor
- sub_mine placement to zone them out of good positions
- Breaking line of sight removes the debuffs
- **Future subroutines**: cleanse/purify abilities, debuff immunity, or turning corruption back on enemies

### Progression — Subroutine Unlock Direction
Killing corruptors drops fragments toward debuff/disruption subroutines:
- **sub_disrupt** — apply debuffs to enemies (slow, damage reduction, disable healing)
- **sub_purge** — cleanse active debuffs on self, brief immunity window
- **sub_corrupt** — turn enemy buffs against them (reverse defender healing, disable aegis)
- These create a **control/disruption playstyle** — weaken enemies systematically rather than brute-forcing them

### Gating Role
Corruptor zones are the "you need anti-debuff or burst priority" gate. Without cleanse subroutines or the damage to quickly eliminate corruptors through a defensive line, the accumulated debuffs make the area feel impossible. Forces:
- Target prioritization skills (player skill, not subroutine)
- Cleanse/immunity subroutines
- Mobility to bypass front-line enemies and reach the corruptor
- Or enough raw burst damage to kill through the debuffs

### Visual Design
- Shape: irregular, glitchy polygon (like corrupted data)
- Color: dark magenta/purple
- Corruption effect: visible tendrils/static connecting to player when debuffing
- Idle: occasional glitch/flicker in its geometry

---

## Enemy Composition & Gating Examples

The power of this system is in combinations. Individual enemy types create demand for specific subroutines, but COMPOSITIONS create demand for specific BUILDS:

| Zone Composition | Challenge | Required Adaptation |
|---|---|---|
| Hunters + Defenders | Sustained ranged DPS behind shields | Mine to break aegis, burst damage, or disrupt heals |
| Seekers + Stalkers | Multi-vector melee threats, visible and invisible | Detection + defensive mobility, can't just dodge one axis |
| Swarmers + Corruptors | Overwhelming numbers while debuffed | AoE clear under pressure, cleanse to maintain effectiveness |
| Hunters + Corruptors + Defenders | Ranged DPS, debuffs, AND shields | The "you need a real build" wall — no single answer |
| Swarmers + Defenders | Defenders healing/shielding the mother | Must kill defenders first or burst mother before split |
| Stalkers + Corruptors | Slowed + invisible assassins | Pure nightmare — speed debuff makes stalker tells harder to react to |

**No loadout handles all of these.** The player must read the zone, visit their catalog, and build for the challenge. THAT is the metroidvania loop.

---

## Implementation Priority

Suggested order:
1. **Corruptor** — simplest AI (positional support like defender), biggest gameplay impact, requires player_stats debuff hooks
2. **Stalker** — medium complexity (visibility system is new), most unique mechanic
3. **Swarmer** — most complex (mother/drone entity management, split/reform), highest entity count impact, but also unlocks the most future build diversity

## Open Questions

- Stalker tell timing: 400ms enough reaction time? Too much?
- Swarmer drone count: how many can we handle before vertex/entity limits matter? (current BATCH_MAX_VERTICES is 65536)
- Do stalkers interact with defender aegis? (aegis blocks the backstab?)
- Swarmer reform: does the mother come back at full HP or proportional to surviving drones?
- Corruptor stacking: additive or multiplicative? (2 corruptors = 25% speed each = 50% total, or 0.75 * 0.75 = 56% speed?)
- Should corruptor debuffs have a brief linger after breaking LOS? (prevents flickering at cover edges)
- Swarmer drone entity management: separate static pool like hunters, or dynamic sub-entities within the swarmer module?
- What are the fragment type names? FRAG_TYPE_STALKER, FRAG_TYPE_SWARMER, FRAG_TYPE_CORRUPTOR?
- How many fragments per unlock for these (higher than current enemies since they're deeper progression)?
