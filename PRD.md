# Hybrid — Product Requirements Document

## Setting & Fiction

**Hybrid** takes place in a digital world where a human brain has been interfaced with a computer system for the first time. The player character is the "Hybrid" — a fusion of human mind and AI consciousness navigating cyberspace.

- **Rebirth**: The game begins with a "rebirth" sequence — what was once a human is being reborn as a brain connected to a network, with the structure of that network projected into its consciousness. This is the player's first moment of awareness in cyberspace.
- **The Grid**: Represents the fabric of cyberspace. Movement across it shows traversal through digital space. Superimposed over the breathing, shifting background cloudscape.
- **Map Cells / Walls**: Give the system topography — structural geometry the player must navigate around. The architecture of the digital system.
- **Data Traces**: Faint glowing circuit-like patterns on traversable ground in generic zones. The visible flow of information through the system — subtle atmospheric detail that makes areas feel alive and active without gameplay effect. In themed zones, these become **biome hazards** (fire embers, frost patches, toxic ground, etc.) with actual gameplay impact.
- **Mines**: Security intrusion detection devices. Part of the system's defenses against the Hybrid.
- **Hunters**: Active security programs that patrol, chase, and fire machine-gun bursts at the player. Orange triangles.
- **Seekers**: Predatory security programs that stalk the player, orbit at dash range, then execute high-speed charge attacks. Green needles.
- **Defenders**: Support security programs that heal wounded allies and shield themselves with aegis when threatened. Light blue hexagons.
- **Stalkers**: Stealth assassination programs that approach nearly invisible, with a subtle tell before striking. Dark purple crescents. (Planned)
- **Swarmers**: Distributed security protocols that split into drone swarms to overwhelm by numbers. Yellow/amber clusters. (Planned)
- **Corruptors**: Counter-intrusion programs that degrade the Hybrid's subsystems — slowing feedback decay, disabling regen, reducing speed. Dark magenta. (Planned)
- **Viruses**: Planned as tools the player eventually uses *against* the system.
- **The Alien Entity**: The ultimate antagonist — a foreign alien intelligence projecting into human Earth cyberspace via a network connection. Its presence warps the final zone into something alien and unrecognizable.

## Genre

Bullet hell shooter × Metroidvania

- **Bullet hell**: Dense enemy patterns, projectile-heavy combat, reflexive gameplay
- **Metroidvania**: Interconnected world, ability-gated progression, exploration with fog of war, boss encounters

**What makes this different from traditional metroidvania**: There are no colored doors. No keys. No hard locks (except the final zone). Progression is gated by **enemy composition difficulty** — specific enemy types and combinations that are near-impossible without the right subroutine loadout, but manageable once you have it. The player doesn't find the weapon that opens the orange door. They find the combination of subroutines that are deadly against the enemy type that's been handing them their ass every time they try to push into a new area. That realization — "oh, THIS is how I beat that" — is the core discovery loop.

## Core Mechanics

### The Ship (Player Avatar)

The player's representation in cyberspace. A red triangle that moves with WASD, aims with mouse.

- **Movement**: WASD directional, with speed modifiers (Shift = fast, Ctrl = warp)
- **Death**: All damage funnels through Integrity. Walls and mines instantly zero Integrity via `PlayerStats_force_kill()`. Enemy projectiles reduce Integrity via `PlayerStats_damage()`, which also generates Feedback at 0.5x the damage amount. When Integrity reaches 0, the ship is destroyed. Respawns at last save point (or origin) after 3 seconds. I-frames (from sub_egress dash) block both damage and feedback generation.
- **Death FX**: White diamond spark flash + explosion sound
- **On respawn**: All enemies (hunters, seekers, defenders, mines) silently reset to full health at their spawn points. This is seamless — the player doesn't see the reset happen.

#### Integrity (Health)

The ship's structural health. Starts at 100, regens at 5/sec after 2 seconds without taking damage (10/sec when Feedback is at 0 — rewards discipline). Sub_mend can instantly activate regen (bypassing the 2s delay) and boost the rate to 3x for 5 seconds. At 0, the ship is destroyed. Damaged by feedback spillover, enemy projectiles, and environmental hazards (walls, mines). All damage sources funnel through the Integrity system — there is one unified death path.

#### Feedback (Overload Meter)

Feedback accumulates from two sources: **subroutine usage** (active cost of abilities) and **damage taken** (passive cost of getting hit, at 0.5x the damage amount). This dual-source design means feedback represents total combat intensity — both what you dish out and what you absorb. Decays at 15/sec after a 500ms grace period. When feedback is full (100) and a subroutine is used, the excess feedback spills over as direct Integrity damage — the damage equals the feedback the action would have added. Damage-generated feedback caps at 100 without spillover (no death spiral from getting hit). This creates a resource management layer: sustained combat has consequences, and players must pace their ability usage AND avoid taking hits to keep feedback manageable.

**Stealth interaction**: Sub_stealth requires 0 feedback to activate. Because damage taken now generates feedback, players can't simply stop shooting and wait to cloak — they must also avoid all incoming damage. This rewards skilled evasion and makes stealth activation a genuine achievement in combat, not just a timeout.

**Feedback costs per subroutine**:

| Subroutine | Feedback Cost |
|------------|--------------|
| sub_pea | 1 per shot |
| sub_mgun | 2 per shot |
| sub_mine | 15 per mine explosion |
| sub_egress | 25 per dash |
| sub_mend | 20 per heal |
| sub_aegis | 30 per activation |
| sub_boost | None |
| sub_stealth | 0 (requires 0 feedback to activate) |
| sub_inferno | 25 per second while channeling |
| sub_disintegrate | 22 per second while channeling |
| sub_gravwell | 25 per deployment |

**Spillover example**: Feedback is at 95, sub_mine adds 15. Feedback caps at 100, the remaining 10 spills over as 10 Integrity damage.

**Spillover feedback**: When spillover damage occurs, a bright red border flashes around the screen (fades over 200ms) and the samus_hurt sound plays. This gives clear visual/audio feedback that you're burning health.

**Damage-to-feedback ratio**: 0.5x — a 20-damage hit generates 10 feedback. This is tunable via `DAMAGE_FEEDBACK_RATIO` in player_stats.c. Shielded damage (aegis) and i-framed damage (egress dash) generate zero feedback since the damage is fully blocked before the feedback calculation.

**HUD**: Two horizontal bars in the top-left corner, with labels and numeric values to the left of each bar:
- **Integrity bar**: Green at full → yellow when low → red when critical
- **Feedback bar**: Cyan when low → yellow at mid → magenta when high. Flashes at 4Hz when full to warn of impending spillover damage.

### Subroutines (sub_ system)

Subroutines are abilities the Hybrid AI can execute to interact with digital space. They are the core progression mechanic.

**Endgame Vision**: 50+ subroutines across many types. The current set (pea, mgun, mine, boost, egress, mend, aegis, stealth, inferno, disintegrate, gravwell) are the foundation — general-purpose tools that work everywhere but excel nowhere specific. As the game expands, subroutines become increasingly specialized. No single loadout of 10 handles everything. The player must read the situation, understand what they're up against, visit their catalog, and build for the challenge. This build-craft layer — choosing, combining, and swapping subroutines to match the threat — is where the strategic depth lives. The skill expression isn't just aim and reflexes; it's knowing which tools to bring and when to swap them.

**Build Archetypes** (long-term direction — examples, not exhaustive):
- **Minion Master**: Drone summoning + drone healing + drone command + feedback management. Powerful army but demands multiple slots and careful resource management. Inspired by Guild Wars necromancer builds where you need supporting skills to make the core strategy viable.
- **Stealth Assassin**: Cloak + blink/teleport + burst damage. Get in, delete a target, get out unseen. High skill ceiling, punishes mistakes.
- **Control/Disruption**: Enemy debuffs + cleanse + area denial. Weaken enemies systematically rather than brute-forcing. Enables otherwise impossible fights through attrition.
- **AoE Specialist**: Plasma cannon + nuke + gravity well + damage dash. Crowd-clearing build that shreds swarms but lacks single-target burst for tanky enemies.
- **Tank/Sustain**: Aegis + mend + damage reduction + regen boost. Survive anything but kill slowly. Safe for exploration, too slow for timed encounters.

Each archetype requires commitment — dedicating multiple of your 10 slots to a strategy. Slot economy IS the build constraint. You can't have everything.

**Activation Categories**:
Every subroutine falls into one of three activation categories:
- **Toggle**: Activated/deactivated by the player. Untoggles other toggles of the same SubroutineType (only one movement toggle active, one weapon toggle active, etc.). May have ongoing feedback cost while active. Examples: sub_boost (hold for speed).
- **Instant**: One-time activation with a cooldown before next use. Feedback cost on activation. This is the most common category. Examples: sub_pea (fire), sub_egress (dash), sub_mend (heal), sub_aegis (shield).
- **Passive**: Always on while equipped in the skillbar. No activation, no cooldown, no per-use feedback cost. The cost IS the slot — dedicating one of 10 slots to a permanent effect instead of an active ability. Examples: (future) stat boosts, resistance effects, aura effects.

**Architecture**:
- Each subroutine has a **type**: `projectile`, `movement`, `shield`, etc.
- Only one subroutine of each type can be **active** at a time
- 10 equippable HUD slots (keys 1-9, 0)
- Pressing a slot's key activates that subroutine and deactivates any other of the same type
- Example: sub_pea (projectile) in slot 1, sub_plasma (projectile) in slot 2 — pressing 2 deactivates pea, activates plasma. LMB now fires plasma.

**Balance Framework**: All subroutines are balanced using a point-budget system. Each skill attribute (damage, cooldown, feedback cost, range, AoE, etc.) contributes points to a total. Normal skills are capped at 10 points, elite skills at 14. This ensures no single skill is overloaded with too many powerful attributes. See `plans/spec_skill_balance.md` for the full framework, scoring tables, and score cards for all implemented skills.

**Known Subroutines**:

| Subroutine | Type | Category | Description | Budget | Unlocked By | Status |
|------------|------|----------|-------------|--------|-------------|--------|
| sub_pea | projectile | Instant | Basic projectile weapon. Fires white dots toward cursor. 500ms cooldown, 1000ms TTL, up to 8 simultaneous. 50 damage per shot, 1 feedback per shot. | 11/10 | Default | Implemented |
| sub_mgun | projectile | Instant | Machine gun. Fires white dots toward cursor. 200ms cooldown, 1000ms TTL, up to 8 simultaneous. 20 damage per shot, 2 feedback per shot. Same DPS as sub_pea but easier to aim, burns feedback 5x faster. | 11/10 | 3 hunter kills | Implemented |
| sub_egress | movement | Instant | Shift-tap dash burst in WASD/facing direction. 150ms dash at 5x speed, 2s cooldown. 25 feedback per dash. I-frames for full dash duration (invuln to all damage including mines). 50 contact damage on enemy body collision (100-unit wide corridor). | 10/10 | 3 seeker kills | Implemented |
| sub_boost | movement (elite) | Toggle | Hold shift for unlimited speed boost. No cooldown, no feedback cost. Elite subroutine (gold border). | 15/14 | Elite fragment | Implemented |
| sub_mine | deployable | Instant | Deployable mine. 3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light. 15 feedback on explosion. | 11/10 | 5 mine kills | Implemented |
| sub_mend | healing | Instant | Instant heal + regen boost. Restores 50 integrity, immediately activates regen (bypasses 2s delay), and boosts regen to 3x for 5 seconds. 10s cooldown. 20 feedback. Activated with G key. | 7/10 | 5 defender kills (mend fragments) | Implemented |
| sub_aegis | shield | Instant | Damage shield. Invulnerable to all damage for 10 seconds. 30s cooldown. 30 feedback. Activated with F key. Cyan ring visual. | 4/10 | 5 defender kills (aegis fragments) | Implemented |
| sub_stealth | stealth | Instant | Cloak and ambush. Go invisible (0.15 alpha + pulse), 0.5x speed while cloaked. Requires 0 feedback to activate. 15s cooldown (starts on unstealth). Breaking stealth via attack grants 5x ambush damage for 1s within 500 units, pierces all shields. Ambush kill resets cooldown. Enemies detect player in 90° vision cone within 100 units. | 7/10 | TBD (placeholder) | Implemented |
| sub_inferno | projectile (elite) | Channel | Channeled fire beam. Hold LMB to unleash 125 blobs/sec, 10 damage each (~1250 DPS). ±5° spread, 2500 u/s speed, 500ms TTL, piercing. 25 feedback/sec while channeling — ~4s sustain window before spillover. 256 blob pool. | 15/14 | TBD (placeholder) | Implemented |
| sub_disintegrate | projectile (elite) | Channel | Precision carving beam. Hold LMB to channel a triple-layer purple-white beam. 2600 range, carves at 120°/sec sweep, full pierce. ~900 DPS, 22 feedback/sec. Dedicated bloom FBO for purple halo. 80 blobs, 3-layer render (purple outer, mid, white-hot core). | 15/14 | TBD (placeholder) | Implemented |
| sub_gravwell | deployable | Instant | Gravity well. Deploys a vortex that pulls enemies toward center and slows them (60% speed at edge, 5% at center). 600-unit radius, 8s duration, 20s cooldown, 25 feedback. 640 particle blobs with ghost trails. No direct damage — pure crowd control. | TBD | TBD (placeholder) | Implemented |

**Each enemy type has a corresponding subroutine** (or set of subroutines) unlocked by defeating enough of that enemy. This creates a progression loop: encounter enemy → learn its patterns → kill it → gain its ability. As the subroutine count grows, some enemies will unlock multiple subroutines (e.g., defenders unlock both sub_mend and sub_aegis), and later enemies will unlock subroutines that form the building blocks of entire playstyle archetypes.

### Progression System

**Philosophy: Enemies are gates. Subroutines are keys.**

Every enemy type exists to create demand for specific subroutines. Every subroutine exists to answer specific enemies. Neither makes sense without the other. An enemy that is trivially beatable with basic tools doesn't justify its existence. A subroutine that doesn't dramatically change how you handle a specific threat doesn't justify its slot. The power of this system is in enemy **compositions** — combinations of enemy types that create demand for specific **builds**, not just individual abilities.

Enemies drop **fragments** (small colored binary glyph collectibles) when destroyed by the player (only if the associated subroutine isn't already unlocked). Fragments last 10 seconds (fade begins at 8s), attract toward the player when nearby, and are collectible for their full lifetime. Collecting fragments from a specific enemy type progresses toward unlocking the subroutine associated with that enemy. Fragment colors match their enemy type.

| Fragment Type | Color | Source | Unlocks | Threshold |
|---------------|-------|--------|---------|-----------|
| Mine | Magenta | Mine kills | sub_mine | 5 |
| Hunter | Red-orange | Hunter kills | sub_mgun | 3 |
| Seeker | Green | Seeker kills | sub_egress | 3 |
| Mend | Light blue | Defender kills (50% drop) | sub_mend | 5 |
| Aegis | Light cyan | Defender kills (50% drop) | sub_aegis | 5 |
| Elite | Gold | Special encounters | sub_boost | 1 |
| Gravwell | TBD | TBD | sub_gravwell | TBD |

- The **Catalog Window** (P key) shows progression and allows equipping — see Catalog Window section below

### Security Programs (Enemies)

All security programs share common behaviors:
- **Line of sight**: Enemies require unobstructed LOS through map cells to detect the player. Walls block vision.
- **Near-miss aggro**: Player projectiles passing within 200 units of an idle enemy will trigger aggro, even without a direct hit.
- **De-aggro on player death**: All active enemies immediately return to idle when the player dies. Their in-flight projectiles are cleared.
- **Reset on player respawn**: All enemies (except bosses) silently reset to full health at their original spawn points when the player respawns. This happens during the death timer so it's invisible to the player.
- **Hit feedback**: Player projectiles hitting an enemy produce a spark + samus_hurt sound for clear damage confirmation.
- **Enemy Registry**: All enemy types register callbacks (find_wounded, find_aggro, heal) with a central registry. This allows support enemies (defenders) to automatically protect any enemy type — including other defenders — without hard-coded type knowledge. New enemy types get defender support by registering with the registry during initialization.

#### Mines (Intrusion Detection Devices)

Passive proximity threats. Dark squares with a blinking red dot.

| Property | Value |
|---|---|
| Behavior | Passive — arms on player proximity, explodes after 2s fuse |
| HP | N/A (destroyed by any player projectile hit) |
| Damage | Instant kill (contact during explosion) |
| Respawn | 10 seconds |
| Drops | Mine fragments (magenta) |

#### Hunters

Active ranged combatants. Orange triangles that patrol, chase, and shoot.

| Property | Value |
|---|---|
| Speed | 400 u/s (half player speed) |
| Aggro range | 1600 units (16 cells), requires LOS |
| De-aggro range | 3200 units (32 cells, 2 major grid squares) |
| Wander radius | 400 units around spawn |
| HP | 100 |
| Attack | 3-shot burst (100ms between shots), 1.5s cooldown between bursts |
| Projectile speed | 2000 u/s |
| Projectile damage | 15 per shot (45 per burst) |
| Projectile TTL | 800ms |
| Respawn | 30 seconds |
| Drops | Hunter fragments (red-orange) |

**State machine**: IDLE (random drift) → CHASING (move toward player) → SHOOTING (stop, fire burst, resume chase) → DYING (200ms flash) → DEAD (30s respawn)

**Damage to kill**: 2 sub_pea shots (50 each) or 5 sub_mgun shots (20 each) or 1 sub_mine

#### Seekers

Predatory dash-kill enemies. Green elongated diamonds (needles).

| Property | Value |
|---|---|
| Speed (stalking) | 300 u/s |
| Speed (orbit) | 500 u/s |
| Aggro range | 1000 units, requires LOS |
| De-aggro range | 3200 units |
| HP | 60 (glass cannon) |
| Orbit radius | 500 units (dash overshoots by 250 — can't just back up) |
| Dash speed | 5000 u/s |
| Dash duration | 150ms |
| Dash damage | 80 (near-lethal) |
| Respawn | 30 seconds |
| Drops | Seeker fragments (green) |

**State machine**: IDLE (random drift) → STALKING (approach player) → ORBITING (circle at dash range) → WINDING_UP (300ms telegraph, red glow) → DASHING (150ms charge through player) → RECOVERING (500ms pause) → DYING (200ms flash) → DEAD (30s respawn)

**Damage to kill**: 2 sub_pea shots (50 each) or 3 sub_mgun shots (20 each) or 1 sub_mine

#### Defenders

Support security programs. Light blue hexagons that heal wounded allies of any type (including other defenders) and shield themselves.

| Property | Value |
|---|---|
| Speed (normal) | 250 u/s |
| Speed (fleeing) | 400 u/s |
| Aggro range | 1200 units, requires LOS |
| De-aggro range | 3200 units |
| Heal range | 800 units |
| Heal amount | 50 HP per heal |
| Heal cooldown | 4 seconds |
| HP | 80 |
| Aegis (self-shield) | 10s duration, 30s cooldown, absorbs all damage |
| Respawn | 30 seconds |
| Drops | Mend OR Aegis fragments (random 50/50) |

**State machine**: IDLE (random drift) → SUPPORTING (heal allies, stay near them) → FLEEING (run from player if within 400 units) → DYING (200ms flash) → DEAD (30s respawn)

**Aegis shield**: Activated when taking damage or player gets close. All incoming damage absorbed for 10 seconds. Bright pulsing hexagon ring visual. Creates a timing puzzle — wait for shield to expire, or kill allies first.

**Heal beam**: Brief light-blue line connecting defender to healed target. Heals the most wounded alive ally of any registered enemy type within range every 4 seconds. Multiple defenders coordinating on the same group will stagger their aegis shields and spread across targets for maximum coverage.

**Damage to kill**: 2 sub_pea shots, 4 sub_mgun shots, or 1 sub_mine (when not shielded)

#### Stalkers (Planned)

Stealth assassins. Dark purple crescents that approach the player nearly invisible, with a brief decloak tell before a devastating strike. Glass cannons — fragile if spotted, lethal if not. The tell (bloom flicker + audio cue + proximity fade-in) must be subtle enough that new players miss it, but consistent enough that experienced players learn to react. Killing stalkers unlocks stealth/phase-shift subroutines.

**Detailed spec**: `plans/new_enemies_spec.md`

#### Swarmers (Planned)

Distributed security protocols. Yellow/amber hexagonal clusters that split into 8-12 fast, individually weak drones when aggroed. Intentionally oppressive with basic weapons — meant to create demand for AoE subroutines (plasma cannon, nuke, gravity well, damage dash) that don't exist yet. Swarmers are the gate that says "you need better AoE to get past here." Critically, swarmer fragments unlock drone-summoning subroutines — the foundation of the **minion master playstyle archetype**, where the player builds their own army with supporting sustain and command subroutines.

**Detailed spec**: `plans/new_enemies_spec.md`

#### Corruptors (Planned)

Debuff support programs. Dark magenta glitchy polygons that degrade the player's subsystems from range — slowing feedback decay, disabling health regen, reducing movement speed. They don't attack directly; they make everything else harder. Position behind allies, flee when approached. Force the player to make target priority decisions: kill the hunter shooting you NOW, or the corruptor behind it making the fight unwinnable? Killing corruptors unlocks disruption/debuff subroutines the player can use against enemies.

**Detailed spec**: `plans/new_enemies_spec.md`

#### Enemy Composition Gating

The real difficulty gating comes from enemy **combinations**, not individual types. Single enemy types test single skills. Compositions test builds.

| Composition | Challenge | What the Player Needs |
|---|---|---|
| Hunters + Defenders | Sustained DPS behind shields | Shield-breaking, burst damage, or heal disruption |
| Seekers + Stalkers | Multi-vector melee from visible and invisible threats | Detection + defensive mobility |
| Swarmers + Corruptors | Overwhelming numbers while debuffed | AoE + cleanse to maintain effectiveness |
| Hunters + Corruptors + Defenders | Ranged DPS, debuffs, AND shields | A real build — no single answer works |
| Swarmers + Defenders | Defenders healing/shielding the mother | Must prioritize defenders or burst mother pre-split |
| Stalkers + Corruptors | Speed-debuffed + invisible assassins | Speed reduction makes stalker tells harder to react to |

No single loadout handles all of these. The player must adapt their build to the zone.

### Damage Model

| Weapon | Damage/Shot | Fire Rate | DPS |
|---|---|---|---|
| sub_pea | 50 | 2/sec (500ms) | 100 |
| sub_mgun | 20 | 5/sec (200ms) | 100 |
| sub_mine | 100 (AoE) | 2s fuse, 3 max | burst |
| sub_egress | 50 (contact) | 2s cooldown | burst |
| sub_inferno | 10/blob | 125 blobs/sec | ~1250 |
| sub_disintegrate | 15/frame | channeled beam | ~900 |
| Hunter burst shot | 15 | 3-shot burst, 1.5s between | ~30 avg |
| Seeker dash | 80 | ~every 4s | ~20 avg |

### World Design

**Structure**:
- Each zone is a complete 1024×1024 map (100-unit cells, ~102,400 world units per side)
- Zones are always accessible (no hard locks) except the **final zone** which requires all 6 themed zone bosses defeated
- Zones are **difficulty-gated through enemy composition**: without the right subroutine loadout, areas are effectively impossible to survive. The gate isn't a locked door — it's a room full of enemies that counter your current build. The key isn't an item — it's the right combination of subroutines and the knowledge of when to use them.
- This creates natural metroidvania progression — encounter a wall → explore elsewhere → unlock abilities → return with the right build → push deeper
- The player will never steamroll everything with one setup. Different zones demand different loadouts, and mastery means knowing how to read a zone and adapt.
- Portals connect zones — stepping on a portal unloads the current zone and loads the destination

**Zone Map** (11 zones):

```
                        [Fire Zone] ←── Boss 1
                       /
            [Generic A]
           /            \
          /              [Ice Zone] ←── Boss 2
         /
[ORIGIN] ── [Generic B] ── [Poison Zone] ←── Boss 3
         \               \
          \               [Blood Zone] ←── Boss 4
           \
            [Generic C]
           /            \
          /              [Electric Zone] ←── Boss 5
                        \
                         [Void Zone] ←── Boss 6

                All 6 cleared → [Alien Zone] ←── Final Boss
```

| Zone | Theme | Role |
|------|-------|------|
| The Origin | Neutral hub | Central hub, player spawn, safe structures, tutorial area. All paths start here. |
| Generic A/B/C | Standard cyberspace | Training grounds with base enemy types. Where players learn mechanics, build initial subroutine collection, and access themed zone portals. Each contains 2 portals to themed zones. |
| Fire, Ice, Poison, Blood, Electric, Void | Themed zones | 6 distinct themed zones, each with custom cell types, themed enemy variants, and a major boss encounter. Clearing a boss is permanent progression. |
| Alien Zone | Alien | The alien entity's domain — visually alien and unrecognizable. Locked until all 6 themed zone bosses are defeated. Contains the final boss. |

**Themed Enemy Variants**:

Each themed zone features the same base enemy types (hunters, seekers, defenders, stalkers, swarmers, corruptors) but with elemental modifiers layered on top. The base AI doesn't change — same patrol, chase, burst, orbit, dash state machines. The twist is in damage types, secondary effects, and visual theming.

Examples (illustrative, not final):
- **Fire hunters**: projectiles leave burning ground on impact
- **Ice seekers**: dash leaves a frozen trail that slows the player
- **Poison stalkers**: backstab applies damage-over-time
- **Electric seekers**: dash chains lightning to nearby targets on contact
- **Blood defenders**: heal beam drains player integrity to fuel ally healing
- **Void corruptors**: corruption aura distorts spatial awareness (minimap interference?)

This approach multiplies content without multiplying AI code. Same state machines, different damage types and secondary effects. Players who know base enemy patterns must still adapt to the twist — you know how to dodge a seeker dash, but when it leaves a frozen trail, your positioning calculus changes. Each theme creates demand for different defensive subroutines (fire resistance, poison cleanse, electric grounding), feeding the 50+ subroutine ecosystem.

**Themed Terrain (Effect Cells)**:

In addition to themed enemies, each zone's **effect cells** (the third tile type) change to match the biome. In generic zones these are purely atmospheric data traces. In themed zones they become gameplay hazards:

- **Fire**: Ember cells — faint orange glow, damage-over-time while standing on them
- **Ice**: Frost cells — pale blue shimmer, reduced friction / momentum sliding
- **Poison**: Toxic cells — sickly green pulse, slow feedback decay while standing on them
- **Blood**: Drain cells — dark red veins, slow integrity drain
- **Electric**: Charge cells — crackling blue-white, periodic damage pulses
- **Void**: Distortion cells — visual warping, minimap interference

Effect cells are generated by the same noise system as walls (using the middle noise band between wall and empty thresholds), so they appear organically throughout the terrain. Players must read the ground as well as the enemies — kiting through fire cells versus clean ground changes the fight. This adds a positioning layer that costs zero AI code and comes free from the procgen system.

**AI Difficulty Tuning**:

Base enemy AI has tunable parameters that scale difficulty across zones without changing behavior logic. The same hunter code runs everywhere — only the numbers change.

Tuning knobs per zone (planned):
- **Aggro range**: how far enemies detect the player
- **Reaction speed**: delay before state transitions (idle → chase, etc.)
- **Projectile speed/accuracy**: how fast and precise enemy shots are
- **HP scaling**: enemy health multiplier
- **Damage scaling**: enemy damage multiplier
- **Cooldown scaling**: how frequently enemies attack
- **Group coordination**: how aggressively support enemies (defenders, corruptors) respond

The Origin and generic zones use relaxed tuning — enemies are slower to react, less accurate, lower damage. Players can explore, die, learn, and make incremental progress. Themed zones ramp up. The alien final zone pulls out all the stops — maximum aggro range, fastest reactions, highest damage, tightest coordination. The final zone should feel genuinely brutal, demanding mastery of both build-craft and mechanical skill.

**Death Philosophy**:

Death is an annoyance, not a punishment. The game is designed to be hard — brutally hard in late zones — but dying should never feel like lost progress. The player should always be able to throw themselves at a challenging area, make incremental gains, die, respawn, and try again.

Core principles:
- **No fragment/skill loss on death**: fragments collected and subroutine progress are permanent. Dying does not roll back progression. The player can push into a zone that's over their head, pick up a fragment or two, die, and keep that progress.
- **Fast respawn**: death → respawn is quick (currently 3s). Get the player back in the action immediately.
- **Respawn at save points**: save points are checkpoints, not progress-save gates. They exist to reduce backtracking, not to create risk/reward tension around saving.
- **Encourage exploration of hard content**: because death is cheap, players are encouraged to probe zones they're not ready for. They might discover what enemies are there, what subroutines they need, and pick up a few fragments before dying. This IS the progression loop — reconnaissance through death.

This philosophy enables the extreme difficulty curve. The alien final zone can be ruthlessly hard because dying 50 times while learning it doesn't cost the player anything except time. The challenge is the reward. Getting through it means you earned it through skill and build-craft, not through hoarding lives or save-scumming.

**Endgame (Vision — Not Yet Designed)**:

Post-final-boss endgame will leverage the procedural generation system to create replayable content. The noise + influence field architecture is built for this — seed-based generation means every seed produces a deterministic but unique world, enabling:
- **Seeded challenge runs**: Daily/weekly seeds where all players get the same world layout, competing on completion time or score
- **Deep network runs**: Randomized zone remixes with escalating difficulty, enemy compositions, and theme crossovers (fire enemies in ice terrain, etc.)
- **Escalating corruption**: Post-game corruption levels that progressively increase enemy AI parameters, add new enemy compositions, and introduce environmental hazards
- **Roguelike mode**: Randomized starting loadouts with permadeath, testing build-craft improvisation
- **Leaderboards by seed**: Shared seeds enable direct competition on identical worlds

This is long-term vision — the building blocks (procgen infrastructure, themed variants, difficulty tuning) need to exist first.

**Bosses**:
- Each themed zone has 1 major boss encounter
- The alien zone contains the final boss
- Boss encounters drive major progression milestones
- Boss design is not yet specified

**Cell types per zone**: Each zone defines its own visual theme through custom cell type definitions. Two categories: **wall types** (solid + circuit, blocking movement and LOS) and **effect types** (traversable, with visual properties and optional gameplay effects). The engine provides global defaults but zones override colors, patterns, and define zone-specific types. Wall types use an influence-proximity placement system: circuit walls concentrate as geometric edge patterns near landmarks (architectural feel) and scatter organically in the wilds (raw, natural feel). Effect types: data traces (generic zones), ember cells, frost cells, toxic cells (themed zones).

### Procedural Level Generation

Zones use a **noise + influence field approach** to level generation. The designer authors a minimal zone skeleton — a center anchor (savepoint + entry portal), landmark definitions with terrain influence tags, and global tuning parameters. The generator procedurally builds the entire 1024×1024 zone around that skeleton. Every generation run is **seed-deterministic** — same zone + same seed = identical world. This is foundational for multiplayer, shared seeds, and reproducible runs.

**Detailed spec**: `plans/spec_procedural_generation.md`
**Implementation phases**: `plans/spec_procgen_phases.md`

**Design philosophy**: No fixed regions, no fixed quadrants, no predetermined layout structure. The world organically reorganizes itself around wherever the landmarks land. Players can't memorize layouts — the labyrinth isn't always in the upper-left, the boss isn't always through the same corridor. Exploration is genuine.

**Key concepts**:

- **Noise-driven whole-map terrain**: 2D simplex noise (seeded, multi-octave) covers the entire 1024×1024 map, creating organic wall patterns. No rectangular regions, no visible boundaries between generated areas. The map is one continuous environment with smooth density variation.

- **Hotspot-carried terrain influences**: Each landmark (boss arena, portal room, safe zone) carries a **designer-configured terrain influence** (dense, moderate, sparse, or structured) that radiates from its position. A boss designed for claustrophobic combat gets dense labyrinthine terrain. A boss designed for an open arena brawl gets sparse terrain. Any landmark can use any influence type — the designer decides per landmark. Since hotspot positions change per seed, the terrain character reorganizes every seed.

- **Hotspot position generation**: Landmark candidate positions are generated per seed (not hand-placed), constrained by minimum separation, edge margins, and center exclusion. Players know a zone *has* a boss — they have to explore to find it.

- **Center anchor rotation/mirroring**: The hand-authored center geometry (savepoint, entry portal, surrounding structure) is rotated in 90° increments and/or mirrored before generation begins (1 of 8 transformations per seed). Even the starting area feels different each run.

- **Connectivity guarantee**: After terrain generation, flood fill validates all landmarks are reachable from center. Unreachable landmarks get corridors carved to them — these feel like intentional data conduits in cyberspace, not ugly tunnels.

- **Progression gates**: The metroidvania's core enforcement mechanism. The designer defines `gate` lines in the zone file declaring that certain landmarks are ONLY reachable by passing through a specific gate landmark (e.g., `gate swarmer_gate boss_arena`). The generator guarantees this — gate landmarks use dense influence to create natural wall barriers, and a sealing pass closes any alternate paths that noise leaves open. Players cannot bypass difficulty gates by walking around them. The encounter inside the gate chunk IS the progression wall.

- **Chunk templates**: Hand-crafted room geometry for landmark locations. Landmarks include boss arenas, **scripted encounter gates** (specific enemy compositions designed as difficulty gates — the metroidvania progression walls), portal rooms, safe zones, and any other curated encounter. Each chunk contains `spawn_slot` lines for guaranteed enemy placement. These are the recognizable set pieces that persist across runs even though their positions vary. Optional structured sub-areas within dense terrain can also use chunk-based fill.

- **Three tile types** (wall / effect / empty): The noise generator uses two thresholds to produce three distinct tile types instead of binary wall-or-empty. Walls block movement. Effect cells are traversable with visual properties and optional gameplay effects. Empty space is pure traversable grid over the background cloudscape. The middle band (effect cells) naturally concentrates at terrain transitions — the edges where dense walls meet open space — adding organic visual texture where it matters most. In generic zones, effect cells are **data traces** (faint glowing circuit patterns, purely atmospheric). In themed zones, they become **biome hazards** (fire damage, ice friction, poison DOT, etc.), creating a tactical terrain-reading layer where players think about WHERE they fight. Same generation system, same noise, different cell type per biome.

- **Wall type refinement**: Two wall types (solid and circuit) are distributed by influence proximity. Near landmarks, walls use **edge detection** — exposed wall faces become circuit type, creating geometric, architectural edges. In the wilds, circuit tiles appear as **organic scatter** — random placement that creates the natural, untamed feel of raw digital terrain. The transition is smooth, driven by influence falloff.

- **Obstacle block scatter**: Small pre-authored wall patterns (3×3, 4×4, etc.) scattered in moderate-to-sparse terrain for tactical cover and visual interest. Each block is flagged as **structured** (geometric, placed near landmarks) or **organic** (random-looking, placed in the wilds), ensuring visual consistency with the surrounding terrain character.

- **Influence-biased enemy population**: Each landmark's influence includes an enemy composition bias (stalker-heavy, swarmer-heavy, etc.), creating organic variation in enemy types across the map. Budget-controlled spawning with spacing enforcement.

- **Content investment**: Center anchor chunks + landmark chunks per zone (5-10 per biome: boss arenas, encounter gates, portals, safe zones) + obstacle blocks (15-30 across biomes, flagged structured or organic). The noise layer generates the vast majority of terrain procedurally. A fraction of hand-authoring every room across 11+ zones at 1024×1024 scale.

**Prerequisites** (all complete):
1. ~~God mode editing tools for procgen content authoring~~ (done)
2. ~~Portals and zone transitions~~ (done)
3. ~~At least one active enemy type~~ (done — mines, hunters, seekers, defenders)

### Map & Navigation

**Minimap** (the HUD square at bottom-right):
- Shows nearby wall/map cell layout
- **Fog of war**: tracks where player has and hasn't explored
- Updates as player moves through the world

**Full Map** (planned, keypress to open):
- Larger detailed view, Super Metroid style
- Shows explored areas, fog of war boundaries
- More detail than minimap

### HUD

- **Stat Bars** (top-left): Integrity and Feedback bars with labels and numeric values
- **Skill Bar** (bottom): 10 slots, shared between gameplay and god mode (see Unified Skill Bar below)
- **Minimap** (bottom-right): 200×200 radar showing nearby geometry with fog of war

### Unified Skill Bar

The 10-slot skill bar at the bottom of the screen is the central interaction mechanism for both gameplay and god mode. It maintains two independent loadouts that swap when toggling modes:

- **Gameplay loadout**: Equipped subroutines. Activate a slot (click-release or number key) then use with the input appropriate to its type. Click activates on mouse-up to prevent accidental firing.
- **God mode loadout**: Equipped placeables. Activate a slot (click or number key) then LMB to place in the world.
- **Slot rearranging**: Drag skills between slots via the Catalog window (P). Drag-to-swap only works while the catalog is open — the skill bar is locked during gameplay.

**Skill activation by type** (gameplay mode):

| Skill Type | Activation Input |
|------------|-----------------|
| Projectile | LMB (fires toward cursor; hold for inferno channel) |
| Movement | Shift (triggers dash/burst) |
| Shield | F key (triggers activation) |
| Healing | G key (triggers heal) |
| Deployable | Space (deploys at position) |
| Stealth | Skill slot key toggles stealth on/off |

**Placeable activation** (god mode): Activate slot, LMB to place at cursor with grid snapping.

Only one subroutine of each type can be active at a time. Activating a sub deactivates any other of the same type. This type-exclusivity system is inspired by Guild Wars (ArenaNet), where limited skill slots and type restrictions make loadout choices meaningful.

### Catalog Window (P key)

A modal window for browsing and equipping skills or placeables, context-sensitive to the current mode.

**Layout**:
- **Left side**: Vertical tabs filtering by category
  - Gameplay mode tabs: Projectile, Movement, Shield, Healing, Deployable (more to come)
  - God mode tabs: Map Cells, Enemies, Portals, Decorations (more to come)
- **Right side**: Vertical scrollable list of items matching the active tab
  - Each entry: square icon (sized to match a skill bar slot) on the left, name + description + metadata on the right
  - Progression info shown for skills (e.g., "73/100 mines killed")
  - Obfuscated entries for undiscovered skills

**Equipping**: Drag an item from the catalog list and drop it onto a skill bar slot. The slot now holds that item.

**Future**: In gameplay mode, skill changes will be location-restricted — the catalog can only be opened at safe structures (spawn point, save stations). God mode has no such restriction.

### God Mode

God mode is the in-game level editor. Toggle with **O** key. God mode and gameplay mode are mutually exclusive — skills cannot be used in god mode, and placeables cannot be used in gameplay.

**When entering god mode**:
- Skill bar swaps to the god mode placeable loadout
- Camera becomes free (unrestricted pan/zoom)
- Ship collision and death disabled
- Catalog window (P) switches to showing placeables

**When exiting god mode** (press O again):
- Skill bar swaps back to the gameplay skill loadout
- Normal gameplay resumes immediately with existing skill configuration
- Camera returns to following the ship

**Editing workflow**:
1. Open catalog (P), browse placeable tabs (Map Cells, Enemies, Portals, etc.)
2. Drag a placeable onto a skill bar slot
3. Activate the slot (click or number key)
4. LMB to place at cursor position (grid-snapped for map cells)
5. Each edit persists immediately to the zone file

**Grid snapping**: Map cells snap to the 100-unit cell grid. Other placeables (enemy spawn points, portals) can be placed freely or snapped depending on type.

**Undo**: Stepwise undo (Ctrl+Z) reverts edits one at a time, updating the zone file with each undo.

**Placeable types** (initial):

| Type | Tab | Description |
|------|-----|-------------|
| Solid cell | Map Cells | Purple solid wall — blocks movement |
| Circuit cell | Map Cells | Blue circuit pattern wall — blocks movement |
| Mine spawn | Enemies | Mine spawn point marker |
| Portal | Portals | Zone transition point (future) |

More types will be added as new cell types, enemy types, and world features are implemented.

## Current Implementation State

### Working
- Ship movement (WASD + speed modifiers)
- Unified death system (all damage → Integrity → death check in one place)
- Enemy reset on player respawn (all enemies silently return to spawn at full health)
- Sub_pea projectile system (pooled, 8 max, 500ms cooldown, swept collision, 50 damage)
- Sub_mgun machine gun (pooled, 8 max, 200ms cooldown, swept collision, 20 damage, 2 feedback/shot)
- Sub_mine deployable mine (3 max, 250ms cooldown, 2s fuse, Space to deploy, steady red light)
- Sub_boost elite movement (hold shift for unlimited speed boost, no cooldown)
- Sub_egress dash burst (shift-tap, 150ms at 5x speed, 2s cooldown, i-frames during dash, 50 contact damage)
- Sub_mend instant heal + regen boost (G key, 50 integrity, 3x regen for 5s, 10s cooldown, 20 feedback)
- Sub_aegis damage shield (F key, 10s invulnerability, 30s cooldown, 30 feedback, cyan ring visual)
- Sub_stealth cloak and ambush (invisible + 0.5x speed, 0 feedback gate, 5x ambush multiplier, shield pierce, cooldown reset on ambush kill, 15s cooldown, enemy vision cone detection)
- Sub_inferno channeled fire beam (elite, hold LMB, 125 blobs/sec, ~1250 DPS, 25 feedback/sec, piercing, 256 blob pool)
- Sub_disintegrate precision beam (elite, hold LMB, carving sweep at 120°/sec, ~900 DPS, 22 feedback/sec, dedicated purple bloom FBO, full pierce)
- Sub_gravwell gravity well deployable (640 blobs, ghost trails, 600-unit radius pull + slow, 8s duration, 20s cooldown, 25 feedback)
- Player stats system (Integrity + Feedback bars, spillover damage, damage-to-feedback (0.5x), regen with 2x rate at 0 feedback, damage flash + sound, shield state, i-frame system)
- Hunter enemy (patrol, chase, 3-shot burst, LOS requirement, near-miss aggro, deaggro on player death)
- Seeker enemy (stalk, orbit, telegraph, dash-charge, 60HP glass cannon)
- Defender enemy (support healer, generic ally protection via enemy registry, aegis self-shield on hit/nearby shots, shield staggering, target deconfliction, flees player, random mend/aegis fragment drops)
- Enemy registry (callback-based type registration — find_wounded, find_aggro, heal — enabling generic enemy interactions)
- Mine state machine (idle → armed → exploding → destroyed → respawn)
- Fragment drops and collection (typed colored binary glyphs, attract to player, 10s lifetime)
- Subroutine progression/unlock system (fragment counting, per-enemy thresholds, discovery + unlock notifications)
- Zone data file format and loader (line-based .zone files with cell types, placements, spawns, portals, save points)
- Zone persistent editing and undo system (Ctrl+Z, auto-save on edit)
- Portals and zone transitions (warp pull → acceleration → flash → arrive cinematic)
- Save point system (checkpoint persistence, cross-zone respawn)
- God mode (O toggle — free camera, placement modes for cells/enemies/savepoints/portals, Q/E to cycle modes, Tab to cycle types, zone jump J, new zone N)
- Skill bar (10 slots, number key activation, type exclusivity, geometric icons, cooldown pie sweep)
- Catalog window (P key — tabbed sub browser, drag-and-drop equipping, slot swapping, right-click unequip, notification dots)
- Map cell rendering with zoom-scaled outlines
- Grid rendering
- View/camera with zoom and pixel-snapped translation
- Main menu (New / Load / Exit)
- HUD skill bar and minimap (live map cell display + player blip)
- Cursor (red dot + white crosshair)
- 7 gameplay music tracks (random selection) + menu music
- OpenGL 3.3 Core Profile rendering pipeline
- FBO post-process bloom (4 instances: foreground entity glow, background diffuse clouds, disintegrate beam, weapon lighting)
- Stencil-based cloud reflection on solid map blocks (multi-value stencil, parallax mirrored sky)
- Weapon lighting on map cells (stencil-tested light FBO composite from weapon emissive sources)
- Subroutine unification (shared mechanical cores for shield, heal, dash, mine, projectile — used by both player subs and enemy AI)
- Centralized damage routing and fragment drop system in enemy_util.c
- GPU instanced particle rendering (shared quad renderer for gravwell, inferno, disintegrate)
- Motion trails (ship boost ghost triangles, projectile thick line trails)
- Background parallax cloud system (3 layers, tiled, pulsing, drifting)
- Background zoom parallax (half-rate zoom for depth perception)
- Vertex reuse rendering (flush_keep/redraw for tiled geometry)
- Rebirth sequence (zoom-in cinematic on game start / zone load)

### Not Yet Implemented
- Unified skill bar Phase 3 (two-loadout system for gameplay/god mode)
- God mode placeable catalog (cell types, enemy spawns, portals via catalog drag-and-drop)
- Boss encounters
- ~~Diagonal walls~~ (scrapped — too many rendering edge cases, square cells are the architecture going forward)
- Procedural level generation (noise + influence field approach — see spec at `plans/spec_procedural_generation.md`)
- Stalker enemy (stealth assassin — see `plans/new_enemies_spec.md`)
- Swarmer enemy (splitting drone swarm — see `plans/new_enemies_spec.md`)
- Corruptor enemy (debuff support — see `plans/new_enemies_spec.md`)
- Themed enemy variants (elemental modifiers per biome zone)
- AI difficulty tuning parameters per zone
- Minimap fog of war
- Full map view
- Intro mode
- Virus mechanics (player tools against the system)
- Spatial partitioning for collision (grid buckets — see Technical Architecture)
- Zone/area design beyond current test layout
- Custom key mapping

## Audio

**Sound Effects**:

| Sound | Used For |
|-------|----------|
| statue_rise.wav | Ship respawn, aegis activation (player + defender) |
| bomb_explode.wav | Ship death, mine explosion, hunter/seeker/defender death, inferno fire loop |
| long_beam.wav | Sub_pea fire, sub_mgun fire, hunter shots |
| ricochet.wav | Projectile wall hit (pea, mgun), shielded defender hit |
| bomb_set.wav | Mine armed |
| door.wav | Mine/hunter/seeker/defender respawn, aegis deactivation, stealth break |
| samus_die.wav | Ship death |
| samus_hurt.wav | Integrity damage (spillover, enemy hits, hit feedback) |
| refill_start.wav | Defender heal beam, stealth activation |
| heal.wav | Sub_mend activation |
| samus_pickup.wav | Fragment collection |
| samus_pickup2.wav | Subroutine unlock |

**Music**: 7 deadmau5 tracks for gameplay (random), 1 for menu

## Technical Architecture

- **Language**: C99
- **Graphics**: SDL2 + OpenGL 3.3 Core Profile
- **Audio**: SDL2_mixer
- **Text**: stb_truetype bitmap font rendering
- **Pattern**: Custom ECS (Entity Component System)
- **UI**: Custom immediate-mode GUI (imgui)

### Abstraction Layers

The codebase is structured with platform abstraction in mind, enabling future ports to other rendering backends and platforms.

**Rendering abstraction** (`render.h` / `render.c`):
- All game entities call `Render_*` functions — never OpenGL directly
- Entity files (ship.c, mine.c, sub_pea.c, map.c) and subroutine core files have zero GL includes
- The `Render_*` API provides: `Render_point`, `Render_triangle`, `Render_quad`, `Render_line_segment`, `Render_thick_line`, `Render_filled_circle`, `Render_quad_absolute`, `Render_flush`, `Render_flush_keep`, `Render_redraw`, `Render_clear`
- Swapping OpenGL for Vulkan/Metal/WebGPU requires only reimplementing render.c, batch.c, shader.c, and bloom.c — entity code is untouched

**Windowing and input abstraction**:
- `Input` struct (`input.h`) abstracts all player input into a platform-neutral struct (mouse position, buttons, keyboard state)
- SDL2 event polling translates platform events into the `Input` struct in the main loop
- All game systems receive `const Input*` — they never call SDL directly
- `Screen` struct (`graphics.h`) abstracts window dimensions
- Porting to another windowing system (GLFW, native platform) requires only changing the SDL calls in `graphics.c` and the main event loop

### Subroutine Unification (Core Extraction)

Player subroutines and enemy abilities that share the same mechanics (shields, heals, dashes, mines, projectiles) are implemented using **shared mechanical cores**. Each core is a pure state machine with no knowledge of player input, skillbar, audio, or progression — the caller (player sub wrapper or enemy AI) handles those concerns.

**Pattern**: Core struct (state) + Config struct (tuning constants) + pure API functions.

| Core | Shared Between | Key Config Differences |
|------|---------------|----------------------|
| `sub_shield_core` | sub_aegis ↔ defender aegis | Duration, cooldown, break grace period, ring size, light |
| `sub_heal_core` | sub_mend ↔ defender heal | Cooldown (10s vs 4s), beam visual duration, heal amount applied by caller |
| `sub_dash_core` | sub_egress ↔ seeker dash | Speed (4000 vs 5000), cooldown (2s vs 0), damage (50 vs 80) |
| `sub_mine_core` | sub_mine ↔ enemy mine | Fuse time (2s vs 500ms), dead phase duration (0 vs 10s for respawn) |
| `sub_projectile_core` | sub_pea, sub_mgun ↔ hunter projectiles | Velocity, damage, cooldown, color, pool size, light radii |

**Damage routing**: `Enemy_check_player_damage()` in `enemy_util.c` checks all player weapons against an enemy hitbox. Each weapon's `check_hit()` returns the damage value from its config (not hardcoded constants), and stealth ambush multiplier is applied centrally.

**Drop system**: `Enemy_drop_fragments()` in `enemy_util.c` handles the shared "pick a random locked subroutine from a carried list, spawn fragment" pattern. Each enemy type defines a static `CarriedSubroutine` table mapping which subroutines it drops.

**Benefits**: Adding a new enemy that uses an existing mechanic (e.g., a new enemy with a shield) requires only writing the AI wrapper with a config — the shield rendering, timing, and state machine come free from the core. Same for new player subs that reuse existing mechanics with different tuning.

### Entity Component System (ECS)

Custom ECS with pointer-based components and static singleton pattern. See `spec_000_ecs_refactor.md` for detailed audit and refactoring roadmap.

**Entity** (`entity.h`): A container holding optional pointers to components and a void* state:
```c
typedef struct {
    bool empty, disabled;
    void *state;                        // Entity-specific data (MineState, ShipState, etc.)
    PlaceableComponent *placeable;      // Position + heading (per-instance)
    RenderableComponent *renderable;    // Render function pointer (shared singleton)
    CollidableComponent *collidable;    // Bounding box + collide/resolve functions (shared singleton)
    DynamicsComponent *dynamics;        // Physics (unused, reserved)
    UserUpdatableComponent *userUpdatable;  // Input-driven update (ship only)
    AIUpdatableComponent *aiUpdatable;      // Autonomous update (mines, etc.)
} Entity;
```

**Component types**:

| Component | Data | Function Pointers | Purpose |
|-----------|------|-------------------|---------|
| PlaceableComponent | position, heading | — | Spatial transform (per-instance) |
| RenderableComponent | — | render(state, placeable) | Draw the entity |
| CollidableComponent | boundingBox, collidesWithOthers | collide(state, placeable, bbox), resolve(state, collision) | Collision detection and response |
| UserUpdatableComponent | — | update(input, ticks, placeable) | Player input handling |
| AIUpdatableComponent | — | update(state, placeable, ticks) | Autonomous behavior |
| DynamicsComponent | mass | — | Physics (reserved, unused) |

**Static singleton pattern**: Components that contain only function pointers (Renderable, Updatable) are declared as `static` singletons shared by all instances of an entity type. Per-instance data lives in separate arrays (state[], placeables[]).

**System loops** (`entity.c`): Iterate all non-empty entities and call component functions:
- `Entity_render_system()` — calls renderable->render for each entity
- `Entity_user_update_system(input, ticks)` — calls userUpdatable->update
- `Entity_ai_update_system(ticks)` — calls aiUpdatable->update
- `Entity_collision_system()` — O(n^2) pairwise collision, queues resolve commands

**Creating a new entity type** (convention):
1. Define a state struct (e.g., `MineState`) with entity-specific data
2. Declare static singleton components for shared behavior (renderable, collidable, updatable)
3. Declare static arrays for per-instance data (states[], placeables[])
4. Write a factory function that calls `Entity_initialize_entity()`, wires up all component pointers, and calls `Entity_add_entity()`
5. Implement component callback functions (render, collide, resolve, update)

**Entity pool**: Fixed array of 2048 entities. Slots are reused when marked empty.

### Immediate-Mode GUI (imgui)

Custom lightweight immediate-mode UI system (`imgui.h` / `imgui.c`). Currently provides button widgets used in the main menu.

**Pattern**: Each frame, UI state is recomputed from input. No persistent widget tree — the caller owns the state.

**ButtonState** struct:
```c
typedef struct {
    Position position;
    int width, height;
    bool hover, active, disabled;
    char* text;
} ButtonState;
```

**Usage**: Call `imgui_update_button(input, &state, on_click)` each frame. Returns updated state with hover/active resolved. Fires `on_click` callback on mouse release within bounds.

**Future**: The catalog window, skill bar interaction, drag-and-drop, and god mode UI will all extend this imgui pattern. Widgets needed: scrollable lists, tabs, draggable icons, tooltips.

### Rendering Pipeline

**Batch renderer** (`batch.h` / `batch.c`):
- Accumulates vertices in CPU-side arrays (65,536 max per primitive type)
- Three primitive batches: triangles, lines, points
- `Batch_flush` uploads to VBO via `glBufferData` and draws with `glDrawArrays`
- Flush order: lines → triangles → points (opaque fills cover grid lines, points on top)

**Shader programs** (`shader.h` / `shader.c`):
- Two programs: color shader (points/lines/triangles) and text shader (textured quads)
- GLSL 330 core, embedded as string literals in source
- Uniforms: projection matrix, view matrix

**Render passes per frame**:
1. **Background bloom pass**: Render background blobs into bg_bloom FBO → gaussian blur → additive composite
2. **World pass**: Grid, map cells, entities (ship, mines, projectiles) with world projection + view transform
3. **Cloud reflection pass**: Stencil mask solid blocks → fullscreen quad samples bg_bloom with mirrored parallax UVs → additive composite
4. **Weapon lighting pass**: Render weapon emissive geometry into light_bloom FBO → blur → stencil-tested additive composite on map cells
5. **Foreground bloom pass**: Re-render emissive elements into bloom FBO → gaussian blur → additive composite
6. **Disintegrate bloom pass**: Re-render beam into disint_bloom FBO → gaussian blur → additive composite (only when beam active)
7. **UI pass**: HUD, minimap, skill bar, text with UI projection + identity view

**Vertex reuse** (`Render_flush_keep` / `Render_redraw`):
- For tiled geometry (background clouds), vertices are pushed once per tile pattern
- `Render_flush_keep` uploads and draws but preserves the vertex data in the VBO
- `Render_redraw` sets new uniforms (offset view matrix) and redraws without re-uploading
- `Render_clear` resets batch counts after all tiles are drawn
- This makes vertex count O(blobs_per_layer) instead of O(blobs × tiles) — the 65k vertex cap is no longer a constraint regardless of zoom level

### FBO Post-Process Bloom

Four-instance bloom system replacing the old geometry-based glow (which hit vertex budget limits with 10 concentric shapes per object).

**Architecture** (`bloom.h` / `bloom.c`):
- 3 FBOs per instance (source, ping, pong) with `GL_RGB16F` textures
- Configurable `divisor` (FBO resolution = drawable / divisor), `intensity`, `blur_passes`
- Embedded GLSL 330 core shaders: fullscreen vertex, 9-tap separable gaussian blur, additive composite
- Fullscreen quad VAO/VBO for post-process passes

**Four bloom instances** (initialized in `graphics.c`):

| Instance | Purpose | Divisor | Intensity | Blur Passes |
|----------|---------|---------|-----------|-------------|
| bloom (foreground) | Neon halos on entities | 2 (half-res) | 2.0 | 5 |
| bg_bloom (background) | Diffuse ethereal clouds + block reflection source | 8 (eighth-res) | 2.5 | 20 |
| disint_bloom (disintegrate) | Purple beam halo | 2 (half-res) | 3.0 | 7 |
| light_bloom (weapon lighting) | Weapon light cast on map cells | 4 (quarter-res) | 1.5 | 6 |

**Bloom sources**: Map cells, ship, ship death spark, sub_pea/sub_mgun projectiles + sparks, sub_inferno fire blobs (1.5x brightness), sub_disintegrate beam (dedicated disint_bloom FBO), sub_aegis shield ring, sub_gravwell vortex particles, mine blink/explosion, hunter body + projectiles, seeker body + dash trail, defender body + aegis ring + heal beams, portals, save points, fragments. Each entity type provides a `*_render_bloom_source()` function that re-renders emissive elements into the FBO.

**Key design decision**: Background renders ONLY through the bg_bloom FBO (no raw polygon render). Additive bloom on top of sharp polygons doesn't hide edges — rendering exclusively through blur produces the desired diffuse cloud effect.

**Bloom API**:
- `Bloom_begin_source(bloom)` — bind source FBO, set reduced viewport, clear
- `Bloom_end_source(bloom, draw_w, draw_h)` — unbind, restore viewport
- `Bloom_blur(bloom)` — ping-pong gaussian blur passes
- `Bloom_composite(bloom, draw_w, draw_h)` — fullscreen quad with additive blend, restore viewport and blend mode

### Cloud Reflection on Solid Blocks

Solid (non-circuit) map blocks act as polished metallic surfaces reflecting the cloud sky above. The blurred cloud texture from `bg_bloom` is sampled through a stencil mask and composited with additive blending, creating a subtle luminous sheen that slides across block surfaces as the camera moves.

**Architecture** (`map_reflect.c` / `map_reflect.h`):
- Stencil buffer (8-bit) requested via SDL, cleared each frame
- **Multi-value stencil write pass**: Renders fill quads for all map cells into stencil buffer — circuit cells write 1, solid cells write 2. Color writes disabled during stencil pass.
- **Reflection pass**: Fullscreen quad samples `bg_bloom->pong_tex` with mirrored UVs and parallax camera offset, drawn with additive blend where stencil == 2 (solid blocks only)
- **Weapon lighting** reuses the same stencil data: tests stencil >= 1 (all cells) for light composite
- Dedicated GLSL 330 core shader for reflection sampling
- Texture temporarily set to `GL_MIRRORED_REPEAT` during reflection to avoid UV seams

**Tunables** (constants in `map_reflect.c`):
- `REFLECT_PARALLAX` (0.001) — UV offset scale per camera pixel
- `REFLECT_INTENSITY` (3.0) — cloud brightness multiplier

**Pipeline position**: After world geometry flush, before weapon lighting pass.

### Weapon Lighting on Map Cells

Player weapons cast colored light onto nearby map cells, creating dynamic illumination as projectiles and beams pass over the terrain.

**Architecture** (`map_lighting.c` / `map_lighting.h`):
- Uses `light_bloom` FBO instance (divisor=4, intensity=1.5, blur_passes=6)
- Each weapon type provides a `*_render_light_source()` function that renders emissive geometry (filled circles at projectile positions) into the light FBO
- After blur, a stencil-tested fullscreen composite draws the blurred light onto map cells only (stencil >= 1, written by the reflection pass)
- Stencil data persists from the reflection pass — lighting re-enables stencil test for composite only

**Light sources**: sub_pea projectiles + sparks, sub_mgun projectiles + sparks, sub_mine armed blink + explosion, sub_inferno fire blobs, sub_disintegrate beam, hunter projectiles + sparks.

**Pipeline position**: After cloud reflection, before foreground bloom pass.

### Motion Trails

**Ship boost trail** (shift key): 20 ghost triangles stretched back along movement vector. Trail length = 4x frame delta. Ghost alpha fades from 0.4 to 0 along the trail. Rendered in both normal and bloom source passes.

**Sub_pea trail**: Thick line (3px) from previous position to current position at 0.6 alpha. Rendered in both normal and bloom source passes.

### Background System

**Tiled parallax clouds** (`background.c`):
- 3 layers with different tile sizes (14k, 17k, 21k) so repeats never align
- Each layer has parallax movement (0.05, 0.15, 0.30) relative to camera
- Blobs are irregular 12-segment polygons with per-vertex radius variation, smoothed
- Slow drift, sinusoidal pulse animation on radius
- Purple hue palette (4 colors)

**Zoom parallax**: Background zooms at half the rate of the foreground (in log space), anchored at default gameplay zoom (0.5). Formula: `bg_scale = 0.5 * pow(view_scale / 0.5, 0.5)`. Creates depth perception — background feels "further away" when zooming.

**Ambient drift**: Slow sinusoidal wander using incommensurate frequencies, independent of player movement. The background gently moves even when the player is still.

**Animation speed**: Background animation runs at 3x real time (pulse, drift, wander) for a dynamic "breathing" feel.

### Spatial Partitioning (Planned)

As enemy counts grow, brute-force collision checks (every projectile × every enemy per frame) will become a bottleneck. A **spatial hash grid** will be used to reduce this to near-constant-time lookups.

**Design**:
- A coarse 2D grid overlaying the world, with cells larger than any single entity (e.g., 500×500 units → ~26×26 grid for the current 12,800 unit world)
- Each frame: clear the grid, insert all active collidable entities (mines, future enemies) into the cell(s) they overlap
- Projectile collision queries walk only the cells the projectile's swept line passes through, testing against entities in those cells
- Ship-to-enemy collision queries check only the cell(s) the ship's bounding box overlaps

**Implementation Plan**:
- New files: `spatial.h` / `spatial.c`
- `Spatial_clear()` — reset all buckets each frame
- `Spatial_insert(entity_id, Position, Rectangle boundingBox)` — add entity to overlapping cell(s)
- `Spatial_query_line(x0, y0, x1, y1)` — return list of entity IDs in cells the line crosses
- `Spatial_query_rect(Rectangle)` — return list of entity IDs in cells the rect overlaps
- Each cell holds a small static array of entity pointers (e.g., 32 max per cell); overflow is ignored or asserted
- Called from `mode_gameplay.c` update loop: clear → insert all enemies → collision systems use spatial queries instead of brute-force iteration
- Grid cell size is tunable; 500 units is a starting point (5× the map cell size, large enough that most entities fit in one cell)

**Why grid buckets over quadtree**:
- The world is already grid-based; a spatial grid maps naturally
- ~50 lines of C with static arrays — no dynamic allocation, no tree rebalancing
- Cache-friendly flat memory layout
- Uniform distribution expected (enemies spread across zones, not heavily clustered)
- If density becomes highly non-uniform (e.g., boss arenas with swarms), quadtree can be swapped in behind the same query interface later

**When to implement**: When enemy density in procgen zones exceeds current brute-force performance budget

### Zone Data Files

Zones are fully data-driven — each zone is defined by a single `.zone` file. Zone files are the primary output of God Mode editing. Two zones currently exist: zone_001 (The Origin) and zone_002, connected by portals.

**Design**:
- One file per zone, 1:1 mapping (e.g., `resources/zones/zone_001.zone`)
- A zone file contains all data needed to fully load that zone
- A zone loader reads the file and populates the map grid, spawns entities, and registers portals
- The current hardcoded zone becomes the first zone file, serving as the reference format
- Map size can vary per zone (stored in the file, not hardcoded)

**File format** (line-based text, one entry per line, `#` comments):

```
# Zone metadata
name Starting Zone
size 128

# Cell type definitions (global defaults + zone overrides)
# celltype <id> <primary_r> <primary_g> <primary_b> <primary_a> <outline_r> <outline_g> <outline_b> <outline_a> <pattern>
celltype solid 20 0 20 255 128 0 128 255 none
celltype circuit 10 20 20 255 64 128 128 255 circuit

# Cell placements (grid coordinates, 0-indexed)
# cell <grid_x> <grid_y> <type_id>
cell 65 65 solid
cell 65 66 solid
cell 72 73 circuit

# Enemy spawn points (world coordinates)
# spawn <enemy_type> <world_x> <world_y>
spawn mine 1600.0 1600.0
spawn mine -1600.0 1600.0
spawn hunter 800.0 800.0

# Portals (grid coordinates + destination)
# portal <grid_x> <grid_y> <dest_zone> <dest_x> <dest_y>
portal 10 64 zone_002 64 64
```

**Cell type registry**: The engine provides built-in global cell types (solid, circuit) with default colors and patterns. Zone files can override these defaults or define entirely new zone-specific types. This enables themed zones (fire, ice, poison, blood) with custom visuals without engine changes.

**Grid coordinates**: Cell positions use 0-indexed grid coordinates (0 to size-1) mapped directly to the map array. The zone loader handles conversion to/from world coordinates. No centered coordinate quirks.

**Persistent editing**:
- Every God Mode edit (place, remove, modify) rewrites the zone file from in-memory state
- The zone file is the single source of truth — the in-memory world state is always a reflection of the file
- Stepwise undo modifies in-memory state and rewrites the zone file
- Undo history is maintained per editing session (not persisted across sessions)
- For expected data volumes (hundreds of cells, dozens of spawns), full file rewrites are sub-millisecond

**Benefits**:
- Zones can be authored, tested, and iterated independently
- Adding a new zone requires no code changes — just a new data file
- Zone transitions (portals) become simple: unload current zone, load target zone file
- God Mode is the zone editor — no external tooling needed
- Toggle G to instantly playtest what you just built
- Text format is human-readable, diffable, and version-control friendly

**Implementation Plan**:
- New files: `zone.h` / `zone.c` — zone loading, unloading, editing, and persistence
- `Zone_load(const char *path)` — parse file, populate map, spawn enemies, register portals
- `Zone_unload()` — clear map, destroy zone entities, reset state
- `Zone_save(const char *path)` — write current in-memory state to zone file
- `Zone_place_cell(grid_x, grid_y, type_id)` — place a cell, save
- `Zone_remove_cell(grid_x, grid_y)` — remove a cell, save
- `Zone_place_spawn(enemy_type, world_x, world_y)` — add spawn point, save
- `Zone_remove_spawn(index)` — remove spawn point, save
- `Zone_undo()` — revert last edit, save
- `mode_gameplay.c` initialization calls `Zone_load()` instead of inline spawning code
- Zone directory: `resources/zones/`
- `map.c` needs a public `Map_set_cell()` function and `Map_clear()` for the zone loader to use
