# Pyraxis Boss Fight Spec

> **Framework**: Node-in-Arena (see `plans/spec_boss_framework.md`)

## Overview

**Zone**: The Crucible — Fire Themed Zone
**Arena**: Dedicated boss chunk (~40x40 cells), circular/octagonal open space
**Node Behavior**: Stationary — Pyraxis is embedded in the architecture, enthroned at center
**Pacing**: Escalating — observation → inferno → everything
**Fight Duration**: 3-5 minutes total, full restart on death
**Reward**: sub_inferno (elite channeled fire beam)

## Design Philosophy

**"The room is trying to kill you."** Pyraxis doesn't chase the player. It doesn't need to. It commands every surface of the arena. Inferno turrets sweep patterns across the floor. Burn zones ignite and shift. Fire enemies pour in from spawn points in the walls. The node sits in the center of it all, watching, pulsing, waiting.

**"Preview Before Earn"** — Pyraxis's primary weapon system is inferno turrets — the same sub_inferno_core the player earns upon victory. The player experiences the weapon from the receiving end first, learning its sweep speed, its range, its sound. When they finally wield it, they already respect it.

**"Final Exam"** — Each phase tests a cluster of skills the player developed surviving the Crucible zone. The fight escalates from pattern reading to spatial management to everything-at-once.

## The Arena

Small dedicated chunk. Roughly 40x40 cells. Circular or octagonal.

- **Shape**: Open arena with architectural infrastructure around and through it — pillars, raised geometry, embedded turret housings. NOT empty. The architecture IS the threat delivery system.
- **Walls**: Ring of solid blocks forming the arena boundary — nowhere to flee
- **Turret housings**: 6-8 architectural structures around the arena perimeter, each containing an inferno turret emplacement. Turrets are visible as part of the geometry (nozzle shapes, glowing vents). They activate in different combinations per phase.
- **Burn zone plates**: 8-12 floor areas marked with faint grating patterns. These are the hazard zones — they telegraph briefly (glow brighter) before igniting. Different plates activate on rotating timers.
- **Spawn alcoves**: 4 recessed areas in the arena walls where fire enemies emerge during later phases.
- **No cover from node**: The center node has clear LOS to the entire arena. The player can use pillar architecture to break LOS from turrets, but never from the node's gaze.
- **Visual**: Intense orange/red palette. The architecture pulses with heat. Circuit patterns on the walls glow ember-orange.

## Pyraxis — The Node

### What It Is

Pyraxis is the fire zone's sector controller — the root process that runs every fire protocol in the Crucible. It's embedded in a throne-like architectural structure at the arena's center. It doesn't move. It doesn't need to. Everything in this room is an extension of it.

### Visual Design

- **Core shape**: Large hexagonal or octagonal geometric form (4-6x hunter size), constructed from angular dark geometry with bright orange-white fracture lines running through it — like cooling magma cracking through obsidian. The shape reads as a piece of the architecture that became sentient.
- **Eyes**: Two bright white-hot points set in the upper face of the node. Orange-yellow bloom halos. Always tracking the player. The brightest things in the arena. Rendered as small bright triangles + bloom FBO. The eyes are what make this feel alive — without them it's a glowing rock; with them it's watching you.
- **Throne structure**: The node sits embedded in a raised architectural housing — it's grown INTO the arena's infrastructure, connected by circuit-line conduits that run into the walls and floor. These conduits visually connect the node to its turrets and hazard plates, selling the idea that it controls the room.
- **Pulse**: The node pulses with a slow heartbeat rhythm (ember glow brightening and dimming). The pulse speed increases with phase transitions. In Phase 3, it's rapid — the node is straining.
- **Bloom**: Heavy bloom source. The node radiates orange light into the foreground bloom FBO. It dominates the visual space even when the player isn't looking directly at it.

### Eye Behavior

- **Phase 1**: Steady, calm — observing. Slow tracking. Contemplative.
- **Phase 2**: Eyes pulse brighter in sync with turret activations. Tracking becomes sharper — head-snap to player position changes.
- **Phase 3**: Eyes leave brief afterimage trails (ember streaks). Rapid pulsing. The calm is gone.
- **During turret sweeps**: Eyes narrow slightly (geometry compresses) — concentrating.
- **Death**: Eyes are the last thing to extinguish. The arena goes dark. The turrets die. The burn zones cool. The node's pulse stops. Then, finally, the eyes dim and go out.

### Damage Model

- **Always damageable** (no invuln gates outside phase transitions). The challenge isn't accessing the node — it's surviving long enough to shoot it while the arena tries to kill you.
- **No movement**: Stationary target. The player always knows where to aim. The difficulty is getting clear moments to fire.

## HP & Phase Thresholds

**Total HP: 10,000**

| Phase | HP Range | Damage to Clear | Approx Duration |
|-------|----------|-----------------|-----------------|
| Phase 1 — The Trial | 100% → 65% | 3,500 | 60-75 sec |
| Phase 2 — The Crucible | 65% → 25% | 4,000 | 75-90 sec |
| Phase 3 — The Transformation | 25% → 0% | 2,500 | 45-60 sec |

**Phase transition**: ~2 second invulnerability window. Node pulses intensely, arena transforms, voice line plays. Player gets a breath.

**Damage math basis**: Player at this point has sub_pea/mgun (~100 DPS sustained) + mines (~100 burst per mine). With dodging reducing uptime to ~40-50%, effective DPS is ~50-70. Total fight damage of 10,000 at ~60 effective DPS = ~165 seconds of dealing damage, total fight with downtime = ~240-300 seconds (4-5 min).

---

## Phase 1 — "The Trial"

**Theme**: The arena wakes up. Pyraxis observes. Pattern reading.
**Primary threats**: Bullet-hell projectile patterns from the node, periodic turret activation.
**Tests**: Dodging projectile patterns, reading turret telegraphs, finding damage windows.
**Duration**: 60-75 seconds

### Arena State

- **2 turrets active** (opposite sides of the arena). Each fires brief inferno sweeps on a staggered timer — one sweeps while the other is cooling. Short sweep arcs (~90°), slow rotation. Learnable pattern.
- **No burn zones yet** — floor is safe.
- **No enemy spawns** — pure arena mechanics.

### Node Behavior

- **Primary attack — Ember Volley**: Radial projectile bursts from the node in bullet-hell patterns
  - Pattern A: 12-projectile ring expanding outward (gaps to weave through)
  - Pattern B: Spiral arm pattern (3 arms, rotating) — requires circular movement to dodge
  - Pattern C: Targeted 5-shot burst aimed at player position (hunter-style but faster)
  - Cycles through patterns with 1.5-2s gaps between volleys
- **Secondary attack — Heat Pulse**: Every ~15s, expanding ring of fire from the node (must dash or outrun)

### Turret Behavior

- Turrets use `sub_inferno_core` — same beam, same damage, same sound
- Each active turret sweeps a ~90° arc, then pauses for ~6s cooldown
- Sweep direction alternates (clockwise/counter) to prevent simple circle-strafing
- Turret activation is telegraphed: the housing glows bright for ~1s before the beam fires

### Player Strategy

- Circle-strafe the arena, weave through projectile gaps
- Track turret cooldowns — attack the node during turret pauses
- Use movement subs (egress/boost) to dodge heat pulses
- Mines near the node for burst damage during lulls
- The 2 turrets are the tutorial — learn their telegraph and timing before Phase 2 adds more

### Phase 1 Voice Line (on enter)

Short line — Pyraxis acknowledges the intruder. Delivered during the initial 2-3 seconds before attacks begin.

### Phase 1 → 2 Transition

At 65% HP: All turrets deactivate. Node pulls all projectiles inward (visual implosion). Node pulses hard — conduit lines in the arena walls flare bright. Voice line plays. Then: more turrets come online, burn zones activate for the first time.

---

## Phase 2 — "The Crucible"

**Theme**: The arena fully activates. Inferno turrets and burn zones create a deadly spatial puzzle.
**Primary threats**: Multiple turret sweeps + rotating burn zones + fire enemy spawns.
**Tests**: Spatial awareness, movement under pressure, add management, multitasking.
**Duration**: 75-90 seconds

### Arena State

- **4-5 turrets active**: Multiple turrets online simultaneously with overlapping sweep patterns. Safe corridors exist but shift constantly.
- **Burn zone plates activate**: 3-4 floor plates ignite simultaneously, dealing DOT damage. Different plates cycle every ~8s — the safe floor is always moving.
- **Fire enemy spawns**: 2 fire seekers emerge from spawn alcoves every ~20s (low HP, ~30 each). Their dash trails leave brief fire corridors — they compound the area denial.

### Node Behavior

- **Ember Volley continues**: Reduced projectile count per pattern (fewer but faster). The turrets are now the primary projectile threat, not the node.
- **Heat Pulse**: Still fires every ~15s. Now harder to dodge because safe ground is limited.
- **Turret command**: The node's eye tracking telegraphs which turret activates next — the eye glances toward a turret housing ~1s before it fires. Observant players can read this tell.

### Turret Behavior

- More turrets, wider arcs (~120°), staggered timing
- Some turrets sweep faster than Phase 1 versions
- Overlapping sweep zones create "crossing beams" — areas that are only safe during brief windows
- Turret beams still leave brief fire trails on the ground (~2-3s duration)

### Burn Zone Behavior

- Floor plates telegraph before igniting (glow pattern brightens for ~1.5s)
- Active burn zones deal DOT damage (5 integrity/sec) while the player stands on them
- 3-4 zones active at any time, rotating on 8s cycles
- The player must track which zones are about to activate and reposition preemptively

### Player Strategy

- Watch for turret telegraphs (housing glow + node eye glance) — preposition to safe corridors
- Prioritize fire seeker adds quickly (they compound area denial with dash trails)
- Track burn zone rotation to maintain safe positioning
- Damage the node between turret sweeps — the brief windows when multiple turrets are cooling
- Sub_egress through turret beams if cornered (i-frames save you from a sweep)

### Phase 2 → 3 Transition

At 25% HP: Pyraxis absorbs all fire in the arena — turrets die, burn zones extinguish, seeker adds explode, brief moment of total darkness. Then everything ignites simultaneously. All turrets. All burn zones. Voice line about transformation/becoming.

---

## Phase 3 — "The Transformation"

**Theme**: Maximum intensity. The arena is fully hostile. The node mirrors the player's tools.
**Primary threats**: All turrets + constant burn + mirror abilities + enemy pressure.
**Tests**: Feedback management, ability timing, the full toolkit under maximum pressure.
**Duration**: 45-60 seconds (shortest but most intense)

### Arena State

- **All turrets active**: Every turret housing fires. Overlapping sweeps. Safe windows are brief.
- **Constant ambient burn**: The entire arena floor deals low tick damage (2 integrity/sec). Player MUST use sub_mend to sustain, or play perfectly.
- **Safe zones**: Small areas (2-3 cells) that cycle around the arena floor, glowing brighter — standing in these negates the floor burn. They orbit the arena edge on a ~6s rotation (semi-predictable pattern).
- **Continuous spawns**: Fire enemies emerge more frequently — hunters AND seekers now, keeping constant pressure.

### Node Behavior — The Mirror

Pyraxis adds "subroutine mirror" attacks, using corrupted versions of player abilities. These come FROM the node, in addition to the turret/burn pressure:

- **Mirror Aegis**: The node activates a fire shield (3-4 seconds duration, ~10s cooldown)
  - While shielded, projectiles are reflected back at the player
  - Player must STOP attacking during shield and focus on surviving
  - Tests patience and awareness (don't DPS mindlessly)

- **Mirror EMP**: Expanding feedback pulse from the node (400-unit radius)
  - Hits player with a large feedback spike (+30 feedback)
  - Forces the player to manage their feedback bar carefully
  - If already high feedback from damage taken + abilities used, this pushes into spillover
  - Telegraph: visible pulse charge for ~1s before detonation

- **Desperation (below 10% HP)**: Turret sweep speed increases, burn zone rotation accelerates. Final push of intensity.

### Player Strategy

- Constant movement to stay in safe zones (avoid floor burn)
- Sub_mend timing is critical — burn through heals managing floor DOT + Mirror EMP feedback
- Sub_aegis is the "save" for mistakes — if caught by a turret sweep or pushed to spillover
- Stop shooting during Mirror Aegis (reflected projectiles are punishing)
- Sub_egress through turret beams, mines for burst when turrets are cooling
- Kill adds fast — they compound the area denial and distract from safe zone tracking
- This phase is SHORT — aggressive DPS in safe windows, the node is almost dead

---

## Victory

1. All turrets deactivate — beams die, housings go dark
2. Burn zones extinguish — floor cools to neutral
3. Spawned enemies die instantly
4. Node death animation: fracture lines on the node flare white-hot, then go dark from the edges inward. The geometric shell cracks and fragments begin to flake off.
5. Node pulse stops
6. Eyes dim slowly — the last points of light in the arena
7. Brief silence. The arena cools.
8. Inferno fragment drops from the node's remains
9. Post-defeat voice line (CRU-09 recording)

---

## Damage Tuning Reference

### Arena Threats → Player

| Attack | Damage | Type | Notes |
|--------|--------|------|-------|
| Ember Volley (single projectile) | 10 | Integrity | From node, 12 per ring, dodge gaps |
| Heat Pulse (expanding ring) | 20 | Integrity | From node, ~15s cycle |
| Inferno turret beam (per tick) | 8 | Integrity + 4 Feedback | ~10 ticks/sec while in beam = 80 DPS + 40 FB/sec |
| Turret beam fire trail (ground DOT) | 5/sec | Integrity | Brief duration (2-3s) |
| Burn zone plates (floor DOT) | 5/sec | Integrity | Phase 2+, telegraphed |
| Fire Seeker dash | 30 | Integrity | Phase 2+ adds |
| Floor burn (Phase 3 ambient) | 2/sec | Integrity | Constant, safe zones negate |
| Mirror EMP | 5 | Integrity + 30 Feedback | The feedback is the real threat |
| Reflected projectile (Mirror Aegis) | Player's own damage | Integrity | Don't shoot the shield |

### Player Resources at This Point

- 100 Integrity, 5/sec regen (after 2s), 10/sec regen when feedback=0
- 100 Feedback, 15/sec decay after 500ms
- sub_mend: 50 HP instant + 3x regen for 5s (10s cooldown, 20 FB)
- sub_aegis: 10s invuln (30s cooldown, 30 FB)
- sub_egress: dash i-frames (2s cooldown, 25 FB)
- Each ability usage + damage taken pressures feedback — this is the resource management game

---

## Miniboss — Ember Sentinel

**Guarding the path to the Pyraxis arena via the procgen gate system.**

Not a complex encounter — a skill check to confirm the player is ready.

- **Concept**: A Fire Hunter with 2-3x normal stats (300 HP, faster fire rate, burn zones on projectile impact)
- **OR**: A Fire Seeker with buffed HP (200 HP) + longer dash trails + faster recovery
- **The miniboss is a gatekeeper, not a puzzle** — if you can beat this, you're ready for Pyraxis
- Could also be: a pair of fire enemies (Hunter + Seeker combo) rather than a single buffed one

> Decision: TBD — playtest which feels better as a gate check

---

## Voice Line Moments

| Moment | Trigger | Content Direction |
|--------|---------|-------------------|
| Arena Enter | Player enters boss chunk | Existing CRU-01 or new short line. Pyraxis acknowledges the intruder. Calm. Authoritative. |
| Phase 1 → 2 | HP hits 65% | "You can read patterns. Let us see if you can survive what patterns cannot describe." |
| Phase 2 → 3 | HP hits 25% | "Still here. Then let me show you what you could become." (All systems ignite) |
| Defeat | HP hits 0 | Existing CRU-09 recording — the full post-defeat speech |

> Note: These are content direction, not final scripts. Actual lines need to match Matthew Schmitz's delivery style and the established Pyraxis voice.

---

## Implementation Considerations

- Pyraxis is a unique entity — custom boss module (`boss_pyraxis.c/h`)
- State machine: `INTRO → PHASE_1 → TRANSITION_1 → PHASE_2 → TRANSITION_2 → PHASE_3 → DYING → DEFEATED`
- **Inferno turrets reuse `sub_inferno_core`** — same beam system the player will earn. Each turret embeds a core state struct with its own position, angle, and sweep config.
- **Mirror Aegis reuses `sub_shield_core`** visual adapted for boss scale
- **Mirror EMP reuses `sub_emp_core`** at larger radius
- Boss HP bar: dedicated UI element (wide bar, top of screen, phase notches)
- Burn zone plates: arena-specific cells with timed activation, managed by the boss module
- Node eye rendering: two points + bloom, player-tracking angle math
- Phase transitions: brief cinematic-like moments (player input still active, node invulnerable, arena transforms)
- Arena floor burn (Phase 3): shader effect or dense particle field
- Arena chunk: hand-authored ~40x40 geometry with turret housing positions, burn zone plate positions, spawn alcove positions, and node throne at center

---

## Open Questions

1. **Miniboss identity**: Single buffed enemy or a pair? Which enemy type? Playtest answer.
2. **Inferno turret visual**: Same particle blobs as player's inferno, or scaled up? Leaning toward same particles — **the skill is the skill**.
3. **Boss HP bar**: Always visible or only when damaged? Leaning toward always visible in boss arena.
4. **Phase 3 safe zones**: Semi-predictable orbit pattern (they circle the arena edge on a timer) so skilled players can track them.
5. **Sub_stealth vs Pyraxis**: A root-level system process shouldn't be fooled by cloak. Options: immune to stealth, or stealth drops aggro for 1-2s before the node "finds" the player again (reduced effectiveness, flavor win, doesn't break the fight).
6. **Turret destructibility**: Can turrets be destroyed? Leaning no — they're part of the architecture. But temporary disable (mine blast knocks a turret offline for ~5s?) could be an interesting skill expression layer.
