# Pyraxis Boss Fight Spec

> **Framework**: Node-in-Arena (see `plans/spec_boss_framework.md`)

## Overview

**Zone**: The Crucible — Fire Themed Zone
**Arena Access**: Landmark portal in the Crucible zone leads to Pyraxis's arena
**Arena**: Dedicated boss chunk (96x96 inner arena, thick solid border for camera containment)
**Node Behavior**: Stationary — Pyraxis dominates the arena center
**Pacing**: Escalating — observation → inferno → everything
**Fight Duration**: 3-5 minutes total, full restart on death
**Reward**: sub_inferno (elite channeled fire beam)

## Design Philosophy

**"The room is trying to kill you."** Pyraxis doesn't chase the player. It doesn't need to. It commands every surface of the arena. Inferno turrets sweep patterns across the floor. Burn zones ignite and shift. Fire enemies pour in from spawn points in the walls. The node sits in the center of it all, watching, pulsing, waiting.

**"Preview Before Earn"** — Pyraxis's primary weapon system is inferno turrets — the same sub_inferno_core the player earns upon victory. The player experiences the weapon from the receiving end first, learning its sweep speed, its range, its sound. When they finally wield it, they already respect it.

**"Final Exam"** — Each phase tests a cluster of skills the player developed surviving the Crucible zone. The fight escalates from pattern reading to spatial management to everything-at-once.

## The Arena

96x96 inner playable arena enclosed by a thick solid border ring (prevents camera from seeing past the edge at max zoom). Total chunk footprint is 162x162.

### Arena Layout

- **Shape**: Open arena with architectural infrastructure around and through it — pillars, raised geometry, embedded turret housings. NOT empty. The architecture IS the threat delivery system.
- **Walls**: Ring of solid blocks forming the arena boundary — nowhere to flee
- **Turret housings**: 6-8 architectural structures around the arena perimeter, each containing an inferno turret emplacement. Turrets are visible as part of the geometry (nozzle shapes, glowing vents). They activate in different combinations per phase.
- **Burn zone plates**: 8-12 floor areas marked with faint grating patterns. These are the hazard zones — they telegraph briefly (glow brighter) before igniting. Different plates activate on rotating timers.
- **Spawn alcoves**: 4 recessed areas in the arena walls where fire enemies emerge during later phases.
- **No cover from node**: The center node has clear LOS to the entire arena. The player can use pillar architecture to break LOS from turrets, but never from the node's gaze.

### Reactor Core Background

The arena background is NOT the standard open cloudscape. The player has descended into infrastructure — a reactor core.

- **Grid of dark squares**: A regularly spaced grid of dark gray metallic squares fills the background behind the arena, with noticeable uniform gaps between them. The squares are very dark — nearly black — barely visible against the void.
- **Metallic sheen via stencil reflection**: The squares use the same stencil-based cloud reflection as solid map cells (`stencil value 2`). The bg_bloom cloudscape slides across their surfaces with parallax, giving them a subtle metallic glint that only reveals their presence when the camera moves. At rest they're almost invisible; in motion, the reflection betrays the grid.
- **Parallax**: The reactor grid scrolls at a different parallax rate than the cloudscape behind it, creating visible depth layering. The clouds are visible through the gaps between squares — you're looking through a grate into the reactor below.
- **Contrast with Pyraxis**: Cold, dark, dead steel grid vs. the churning black-red entity at the center. When Pyraxis attacks, fire abilities light up the reactor squares through the weapon lighting system, briefly revealing the full grid in orange light before it fades back to near-darkness.
- **Implementation**: Custom background renderer for the boss arena — renders the square grid as quads with stencil writes, sampled by the existing `MapReflect` pipeline. Replaces the standard cloudscape-only background for this chunk.

## Pyraxis — The Node

### What It Is

Pyraxis is the fire zone's sector controller — the root process that runs every fire protocol in the Crucible. It dominates the arena's center — a massive swirling vortex of dark energy with burning eyes. It doesn't move. It doesn't need to. Everything in this room is an extension of it.

### Visual Design

- **Core shape**: A large swirling cloud of mostly black with deep red-orange bleeding through — like a dark star collapsing. Diameter of ~1600 units (16 cells across, ~800u radius — dramatically larger than any normal enemy). No hard geometric edges — it's a churning mass rendered via the `particle_instance` system. Hundreds of dark blobs orbit the center at varying speeds and distances, with scattered bright ember particles mixed in. The shape is alive, constantly shifting, never quite the same frame to frame.
- **Color palette**: Predominantly black/near-black particles with dark crimson and burnt orange swirled through. NOT a bright fire — this is something that consumes light. The darkness is the threat. The occasional ember flare is what makes it feel volcanic rather than void.
- **Eyes**: Two fiery points set in the upper mass of the swirl. Bright orange-white cores with heavy bloom halos. Always tracking the player. The brightest things in the arena — the only consistent points of light in the churning dark. Rendered as small bright triangles + dedicated bloom. The eyes are what make this feel alive and intelligent — without them it's a storm; with them it's watching you.
- **Scale and presence**: Pyraxis should dominate the arena visually. Against the cold dark reactor grid background, this burning dark mass is unmistakable. The player should feel small. The swirl should extend far enough that navigating near the center means weaving through the outer edges of the cloud.
- **Conduits**: Circuit-line conduits run from the base of the swirl into the arena walls and floor, connecting the entity to its turrets and hazard plates. These sell the idea that Pyraxis isn't just inhabiting the room — it IS the room.
- **Pulse**: The swirl's rotation speed and ember intensity pulse with a slow heartbeat rhythm. Pulse speed increases with phase transitions. In Phase 3 the rotation is rapid and the red-orange bleeds through more intensely — the black is losing containment.
- **Bloom**: Heavy bloom source. The ember particles and eyes radiate orange light into the foreground bloom FBO, and the weapon lighting system casts Pyraxis's glow onto surrounding map cells and the reactor grid behind it. It dominates the visual space even from across the arena.

### Eye Behavior

- **Phase 1**: Steady, calm — observing. Slow tracking. The swirl rotates lazily. Contemplative.
- **Phase 2**: Eyes pulse brighter in sync with turret activations. Tracking becomes sharper — snap to player position changes. The swirl tightens and accelerates.
- **Phase 3**: Eyes leave brief afterimage trails (ember streaks) as the swirl whips past them. Rapid pulsing. Red-orange dominates the palette. The calm is gone — this is a firestorm with intent.
- **During turret sweeps**: Eyes narrow slightly (geometry compresses) — concentrating. The swirl briefly slows as if focusing energy outward.
- **Death**: The swirl flies apart — particles scatter outward in a final burst, leaving only the eyes hanging in empty space. The arena goes dark. The turrets die. The burn zones cool. Then, finally, the eyes dim and go out.

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

## Arena Geometry Reference

```
Arena: 96x96 inner (grid 463-560), center at (511.5, 511.5)
Cell size: 100 world units → arena = 9600x9600u

8 turret pillars (2x2 circuit blocks) in a ring ~32 cells from center:

    Cardinals (~32 cells from center, 3200u):
      N: (511-512, 543-544)
      S: (511-512, 479-480)
      E: (543-544, 511-512)
      W: (479-480, 511-512)

    Diagonals (~45 cells from center, 4525u):
      NE: (543-544, 543-544)
      NW: (479-480, 543-544)
      SE: (543-544, 479-480)
      SW: (479-480, 479-480)

Player: 800 u/s normal, 1600 u/s boosted, 4000 u/s egress dash (150ms)
Boss turret beam range: 1600u (16 cells) — blob speed 2500 u/s × 640ms TTL
Arena wall to nearest cardinal pillar: ~16 cells
Cardinal pillar to node: ~32 cells
Diagonal pillar to node: ~45 cells
```

The 8 pillars divide the arena into **lanes** — the spaces between adjacent pillars. Cardinal pillars are closer to center, diagonal pillars are pushed outward, creating wider lanes at the diagonals and tighter lanes at the cardinals.

**Turret mounting**: Each turret is mounted at the center of its 2×2 circuit pillar, firing outward. The beam rotates 360° continuously — the pillar does NOT block its own turret's beam. Pillars only provide cover from OTHER turrets and from node attacks (ember volleys, heat pulse). A turret beam at 16-cell range covers pillar-to-wall and halfway to the node from a cardinal.

**Mine disable**: A mine detonation hitting a turret pillar knocks that turret offline for 5s. The beam dies, pillar circuits go dark. After 5s the turret reignites with a 1s spin-up (beam fades in, rotation resumes from where it stopped). Only affects the single turret hit — not adjacent pillars. This is a deliberate tradeoff: mines used on turrets are mines NOT used for burst DPS on the node. Skilled players can strategically disable a turret to create a safe corridor during a burn zone swap or a Phase 3 add wave.

**Cover**: A player can duck behind any pillar to break LOS from a turret beam, but all 8 pillars have clear LOS to the node. There is no safe spot from everything simultaneously — the player must constantly choose which threat to dodge.

### Burn Zone Geometry

The arena is divided into a permanent center burn and 4 directional burn zones that activate per phase:

```
Center burn (permanent): 32x32 cells (grid 496-527 both axes, 3200x3200u)
  Pyraxis lives here. Approaching the node always costs integrity.

Four directional zones (each 1/3 of the arena):
  Top:    full width × 32 tall  (grid x=463-560, y=528-560)
  Bottom: full width × 32 tall  (grid x=463-560, y=463-495)
  Left:   32 wide × full height (grid x=463-495, y=463-560)
  Right:  32 wide × full height (grid x=528-560, y=463-560)

Corner overlaps (belong to 2 zones each):
  TL: Top AND Left     TR: Top AND Right
  BL: Bottom AND Left  BR: Bottom AND Right

Pillar positions within zones:
  N cardinal → Top zone       S cardinal → Bottom zone
  E cardinal → Right zone     W cardinal → Left zone
  NE diagonal → Top+Right     NW diagonal → Top+Left
  SE diagonal → Bottom+Right  SW diagonal → Bottom+Left

Phase 2 safe pockets (when Top+Bottom active):
  Left-mid:  32x32 (grid 463-495, 496-527) — W cardinal pillar inside
  Right-mid: 32x32 (grid 528-560, 496-527) — E cardinal pillar inside

Phase 2 safe pockets (when Left+Right active):
  Top-mid:    32x32 (grid 496-527, 528-560) — N cardinal pillar inside
  Bottom-mid: 32x32 (grid 496-527, 463-495) — S cardinal pillar inside
```

**Phase escalation**:
- Phase 1: One zone at a time — 3/4 of the outer arena safe
- Phase 2: Two opposite zones — two 32×32 safe pockets remain (each with a turret in it)
- Phase 3: All four zones — entire arena is burning, no safe ground

---

## Phase 1 — "The Trial"

**Theme**: The arena wakes up. Pyraxis observes. The player learns the turret language.
**Primary threats**: Node projectile patterns + 2 turrets rotating continuously + single burn zone rotation.
**Tests**: Reading turret rotation, movement timing, finding DPS angles on the node, basic repositioning.
**Duration**: 60-75 seconds

### Turrets

**2 active** — one pair of opposing cardinal turrets (N + S, or E + W).

| Parameter | Value |
|-----------|-------|
| Rotation | 360° continuous — lighthouse style |
| Rotation speed | 30°/sec (12 seconds per full rotation) |
| Direction | Opposite — one CW, one CCW |
| Beam range | 1600u (16 cells) from pillar center |
| Beam origin | Center of 2×2 pillar — fires outward over the pillar edges in all directions |

The turrets are mounted at the center of each 2×2 circuit pillar. The beam shoots outward from the pillar center, rotating continuously. The pillar geometry does NOT block its own turret's beam — the beam fires over/past the pillar edges as it sweeps. Pillars only provide cover from OTHER turrets' beams and from the node's attacks.

**No downtime**. The beam is always somewhere, always moving. The player must track where the beam is pointing and stay behind it — moving with the rotation, not against it. At 30°/sec the player has time to read and react. Two turrets spinning in opposite directions means their beams cross paths periodically in the lanes between them — the player has to time those crossings.

This is the learning phase. 30°/sec is slow enough to be readable. The player learns: watch the beam, move with it, shoot the node when the beam isn't between you and center.

### Burn Zones — Single Zone Rotation

The center 32×32 burn zone is always active — approaching Pyraxis costs integrity. In Phase 1, the four directional zones activate **one at a time** in rotation.

| Parameter | Value |
|-----------|-------|
| Center burn | Always active, 5 integrity/sec |
| Zone rotation | One directional zone active at a time (Top → Right → Bottom → Left → ...) |
| Zone duration | 6s active per zone |
| Transition gap | 2s dark between zones (brief full-arena safety) |
| Zone DOT | 5 integrity/sec |
| Telegraph | 1.5s — zone floor glows orange, building to full brightness before damage starts |

With only one zone active, 3/4 of the outer arena is always safe. The player just needs to not be standing in the active zone when it lights up. This is the easy version — teaching the player to read the zone telegraphs and reposition. The 2s gap between zones is generous, giving time to move.

### Node Attacks

**Ember Volley** — radial projectile bursts from the node. These are boss-specific projectiles (NOT `sub_projectile_core` — dedicated ember pool, fire-colored, slower than player peas).

| Pattern | Projectiles | Speed | Interval | Dodge Method |
|---------|-------------|-------|----------|--------------|
| **Ring** | 16 evenly spaced | 600 u/s | Every 4s | Weave through gaps (gaps are ~370u at 10-cell range, wider than the ship) |
| **Spiral** | 3 arms × 6 shots, rotating | 500 u/s | Every 6s | Move perpendicular to arm rotation — circle-strafe with it |
| **Aimed Burst** | 5-shot spread at player | 900 u/s | Every 5s | Sidestep or egress — fast but narrow cone |

- Patterns cycle: Ring → Spiral → Aimed → Ring → ...
- 1.5s gap between patterns (the node's eyes narrow, swirl slows — visual "inhale" before the next volley)
- Projectiles despawn at ~40 cells from center (4000u) — they don't litter the whole arena

**Heat Pulse** — expanding ring of fire from the node.

| Parameter | Value |
|-----------|-------|
| Frequency | Every 20s |
| Expansion speed | 1200 u/s |
| Ring width | ~200u (2 cells) |
| Damage | 20 integrity |
| Reach | Full arena (hits wall and dissipates) |
| Telegraph | 2s — node swirl contracts inward, embers pulled to center, distinct audio cue |

The ring reaches the nearest cardinal pillar (~32 cells) in ~2.7s. The player can outrun it with boost (1600 u/s > 1200 u/s), egress through it with i-frames, or hide behind a pillar (pillars block the ring — creates a shadow behind them). This teaches pillar usage as cover.

### Fire Adds — Introduction

Light add pressure introduces the enemy types the player will face in later phases.

| Parameter | Value |
|-----------|-------|
| Spawn rate | 2 seekers every 25s |
| HP | 30 each |
| Behavior | Standard seeker AI — orbit → windup → dash |
| Max alive | 2 |
| Spawn location | Random alcove (N/S/E/W wall midpoints) |

Seekers are manageable in Phase 1 — the arena is mostly open, only one burn zone active at a time. They teach the player to split attention between arena mechanics and adds before Phase 2 compounds the pressure.

### Player Strategy

- Orbit the node at mid-range (~20-30 cells), weaving through ember volleys
- Track the 2 rotating turret beams — move with the rotation, not against it. Shoot the node when the beam isn't between you and center.
- Watch which burn zone is about to activate — reposition early, don't get caught flat-footed
- Use pillars as cover from heat pulses and OTHER turrets' beams (not your own — it fires over its own pillar)
- Kill seekers quickly — they're easy now but learning the reflex pays off in Phase 2
- Mines near the node when you have a clear angle
- **Learning objective**: read turret rotation and beam crossings, read ember patterns, read burn zone telegraphs. These skills are mandatory for Phase 2.

### Phase 1 Voice Line (on enter)

Short line — Pyraxis acknowledges the intruder. Delivered during the initial 2-3 seconds before attacks begin. The arena is still, the turrets dark. Then the first two pillar circuits begin to glow.

### Phase 1 → 2 Transition (at 65% HP)

1. Active turrets deactivate — beams die
2. Active burn zone extinguishes
3. Node pulls all ember projectiles inward (visual implosion — projectiles reverse velocity toward center)
4. Swirl contracts tight, pulses hard — conduit lines running from node to ALL 8 pillars flare bright sequentially (a "wake up" ripple)
5. Voice line plays
6. 2s invulnerability window — the breath before the storm
7. 4 new turret pillars ignite. Two opposing burn zones ignite simultaneously. Phase 2 begins.

---

## Phase 2 — "The Crucible"

**Theme**: The arena is a machine and it's running. Turrets, burn zones, and adds create a spatial puzzle where the safe ground keeps shrinking.
**Primary threats**: Overlapping turret sweeps + two opposing burn zones + fire seeker adds.
**Tests**: Spatial planning, movement under compound pressure, add prioritization, multitasking.
**Duration**: 75-90 seconds

### Turrets

**6 active** — all 4 cardinals + 2 diagonal turrets (rotating which 2 diagonals are active).

| Parameter | Value | Change from P1 |
|-----------|-------|----------------|
| Rotation | 360° continuous | Same |
| Rotation speed | 45°/sec (8s per rotation) | +50% faster |
| Direction | Mixed CW/CCW | More crossing patterns |
| Active count | 6 | 3× Phase 1 |

**6 lighthouse beams** rotating simultaneously at different phases. The beams create a constantly shifting maze — safe corridors exist at any moment but they move. Adjacent turrets spinning in opposite directions cross beams regularly, creating deadly intersections the player must time.

**Diagonal rotation**: Every ~15s, the 2 active diagonal turrets swap with the 2 inactive ones. The outgoing diagonals' beams slow to a stop and dim over 1s while the incoming ones spin up and ignite. This prevents the player from memorizing a fixed pattern of safe corridors.

**Beam crossings**: When two adjacent turrets' beams converge on the same lane, the intersection is lethal — double damage tick rate. With 6 turrets at varying speeds and directions, crossings happen frequently but predictably. Reading these crossings is the Phase 2 skill check.

### Burn Zones — Opposing Pairs

Two opposing directional zones are active simultaneously, alternating between Top+Bottom and Left+Right.

| Parameter | Value | Change from P1 |
|-----------|-------|----------------|
| Center burn | Always active, 5 integrity/sec | Same |
| Active zones | 2 opposing (Top+Bottom OR Left+Right) | Was 1 |
| Pair duration | 10s active | Longer — player must commit to a safe pocket |
| Swap telegraph | 2s — active zones dim while incoming zones glow | New — both pairs visible during transition |
| Swap gap | 1s full dark between pairs | Shorter gap than P1 |
| Zone DOT | 5 integrity/sec | Same |

**The squeeze**: With Top+Bottom active, the player is funneled into two 32×32 safe pockets (left-mid and right-mid). With Left+Right active, they're funneled into top-mid and bottom-mid. Every 10s the safe ground shifts 90° — the player must cross through the center danger zone (Pyraxis's burn aura) or sprint along the edge to reach the new safe pocket.

**Turret interaction**: Each safe pocket contains a cardinal turret pillar (W and E for Top+Bottom, N and S for Left+Right) with a beam continuously rotating through the pocket. The player is forced into close quarters with an active lighthouse beam in the only safe ground. The burn zones compress you into the pocket; the turret beam sweeps through it every 8 seconds. No rest.

**Corner danger**: The 4 corner areas (where diagonal pillars sit) are in TWO zones each. When Top+Bottom is active, the corners are burning. When Left+Right, different corners burn. The diagonal turrets are always in hostile territory during their zone's activation — the player must dash through burn damage to use corner pillars as cover.

### Fire Adds — Seekers + Corruptors

Corruptors enter the fight. Their scorch trails and EMP feedback spikes are the new pressure layer.

| Parameter | Value |
|-----------|-------|
| Spawn rate | 1 seeker + 1 corruptor every 18s |
| Seeker HP | 30 (glass cannon) |
| Corruptor HP | 70 |
| Max alive | 4 total |
| Spawn location | Random alcoves (N/S/E/W wall midpoints) |

**Seekers** dash through safe pockets — their trails cut the limited safe ground in half. Burst chaos.

**Corruptors** are the star. They snake through the arena on scorch sprints, leaving 4s burning footprints everywhere they go. In a 32×32 safe pocket, a single corruptor sprint contaminates a huge percentage of the player's safe ground. Their EMP feedback spike (+30 FB) compounds the feedback pressure from turret hits and ability usage — a well-timed corruptor EMP after the player egresses through a turret beam can push straight into spillover.

**Priority**: Corruptors are the #1 kill target. Every second they live, the safe pocket shrinks and feedback pressure mounts. Seekers are secondary — dangerous but brief. A corruptor left alive for a full sprint cycle can make a safe pocket uninhabitable.

### Node Attacks

- **Ember Volley**: Same 3 patterns but projectile count reduced per pattern (12 ring, 2×5 spiral, 4 aimed). The turrets and burn zones are now the primary threats — the node's volleys are supplementary pressure.
- **Heat Pulse**: Still every 20s. Significantly harder — the player is already confined to a safe pocket, and a heat pulse forces them to egress or eat 20 damage. Can't casually reposition when 2/3 of the arena is on fire.

### Player Strategy

- **Pocket awareness**: Always know which safe pocket you're heading to next. When the burn zone swap telegraph starts (2s warning), start moving toward the new safe ground immediately.
- **Pocket transitions**: The fastest route between safe pockets is through the center — but that's Pyraxis's burn aura. Boost speed (1600 u/s) crosses the 32-cell center in 2s, taking ~10 integrity from the center burn. Alternatively, sprint along the arena edge — longer path but avoids center burn, though you cross through the briefly-dark transitioning zone.
- **Turret windows**: Deal damage to the node from within your safe pocket. The cardinal turret in your pocket is the main threat — track its sweep cycle and fire on the node between sweeps.
- **Kill corruptors FIRST**. Their scorch trails contaminate safe pockets and their EMP feedback spike can push you into spillover. A corruptor alive in your safe pocket is an emergency. Seekers are secondary.
- **Feedback budget**: Corruptor EMPs (+30 FB) + turret hits (+4 FB each) + egress dashes (25 FB) + mend heals (20 FB) = the feedback bar is under constant pressure. Don't panic-dash through a turret beam right after eating a corruptor EMP — that's spillover territory.
- **Egress as escape**: Sub_egress i-frames through turret beams or heat pulses. Budget carefully — you'll need egress for pocket transitions too.
- **Beam tracking**: The cardinal turret in your safe pocket rotates continuously — know where the beam is at all times. Use other pillars as cover from heat pulses and ember volleys, but remember they can't block the turret in your own pocket.

### Phase 2 → 3 Transition (at 25% HP)

1. Pyraxis roars — audio spike, screen shake
2. All active fire is absorbed: turret beams retract into pillars, burn zones snuff out, all adds explode in place (fire sucked toward the node)
3. The arena goes dark. For ~1.5s, the only light is Pyraxis's eyes — brighter than ever, burning white-hot
4. Voice line: "Still here. Then let me show you what you could become."
5. All four burn zones ignite simultaneously — the entire arena floor glows. All 8 turrets fire.
6. 2s invulnerability window ends. Phase 3 begins.

---

## Phase 3 — "The Transformation"

**Theme**: Pyraxis stops holding back. The arena is fully hostile. The node deploys Mirror Aegis while corruptors, seekers, and hunters swarm the burning arena. This is the final exam — every skill the player has must be used, and feedback management becomes the real fight.
**Primary threats**: All 8 turrets + total floor burn + Mirror Aegis + seeker/hunter/corruptor adds.
**Tests**: Feedback management, ability timing, add prioritization, DPS discipline under maximum pressure.
**Duration**: 45-60 seconds (shortest phase, highest intensity)

### Turrets — Full Activation

All 8 turrets online. Every pillar is a lighthouse.

| Parameter | Value | Change from P2 |
|-----------|-------|----------------|
| Rotation | 360° continuous | Same |
| Rotation speed | 60°/sec (6s per rotation) | +33% faster |
| Direction | Mixed CW/CCW | Same |
| Active count | 8 | All turrets |

**8 beams** sweeping the arena simultaneously. At 60°/sec the beams move fast enough that the player must be in constant motion — standing still means getting hit within seconds. The overlapping beams create a dense web of hazards. Safe corridors still exist but they're narrow and fleeting. Beam crossings happen frequently.

### Floor Burn — The Clock

All four directional burn zones are active simultaneously. Combined with the permanent center burn, the **entire arena is on fire**. No safe pockets. No rotation. Nowhere to stand without taking damage.

| Parameter | Value |
|-----------|-------|
| DOT | 2 integrity/sec (everywhere) |
| Coverage | Entire arena — center burn + all 4 directional zones |
| Duration | Permanent for the phase |

**2 integrity/sec** doesn't sound like much, but it's relentless. Over 60 seconds that's 120 damage — more than the player's full integrity bar. Without sub_mend, the player dies to the floor alone. This is the timer. Phase 3 can't be played passively. The burn zones aren't a puzzle anymore — they're a countdown.

### Mirror Aegis

Pyraxis's signature Phase 3 ability — a fire shield that punishes mindless DPS.

| Parameter | Value |
|-----------|-------|
| Duration | 3s |
| Cooldown | 12s |
| Telegraph | 1s — hexagonal fire ring expands from node, distinct audio chime |
| Visual | Orange-red hexagon ring (same geometry as player aegis, fire palette) |
| Effect | All projectiles that hit the shield are reflected back toward the player at 1.5× speed |
| Reflected damage | Player's own projectile damage value |

The test: **stop shooting**. Player pea projectiles reflected at 1.5× speed are fast and hard to dodge. Mines detonating against the shield also reflect their blast. The player must recognize the telegraph, stop DPS, and focus on survival for 3 seconds. Muscle memory says "keep shooting" — discipline says stop.

The 3s downtime is also a natural window to kill adds or reposition — smart players use it productively rather than just waiting.

### Fire Adds — All Three Types

All enemy types are in play. The full fire zone roster.

| Parameter | Value | Change from P2 |
|-----------|-------|----------------|
| Spawn rate | 1 seeker + 1 hunter + 1 corruptor every 15s | All three types, faster |
| Seeker HP | 30 | Same |
| Hunter HP | 60 (reduced from normal 100) | Die faster, still dangerous |
| Corruptor HP | 70 | Same |
| Max alive | 6 | +2 cap |
| Floor burn on adds | Yes — 2 integrity/sec | Adds are on a death timer too |

**Seekers** create burst chaos with dashes. **Hunters** add ranged projectile pressure from distance — another thing to dodge while managing turret sweeps. **Corruptors** are still the priority kill — their EMP feedback spike (+30 FB) is devastating when the player is already juggling ability costs, turret hits, and floor burn. A corruptor EMP after an egress dash (25 FB) + mend (20 FB) = 75 feedback in seconds → spillover → death.

The floor burn means adds die on their own eventually (seeker: 15s, hunter: 30s, corruptor: 35s), but they do plenty of damage before that. Ignoring them is not an option.

**Feedback pressure is now enemy-driven**: Corruptor EMPs are the feedback threat, not the node. Pyraxis focuses on Mirror Aegis (DPS discipline) while its minions handle the feedback game. This means feedback spikes come from unpredictable positions — wherever the corruptors are — not from the fixed center node. The player can't just outrange it.

### Desperation (below 10% HP / 1000 HP remaining)

| Parameter | Change |
|-----------|--------|
| Turret rotation speed | 75°/sec (+25%) |
| Floor burn DOT | 3 integrity/sec (+50%) |
| Node swirl | Rotation accelerates, red-orange dominates, black containment breaking apart |
| Ember volley | Ring pattern fires every 3s instead of 4s |

The arena is coming apart. The node is desperate. This lasts ~15-20 seconds at most — the player is close to victory and both sides know it. The visual intensity peaks here: bloom cranked, particle count maxed, the reactor grid behind fully lit by turret and node glow.

### Player Strategy

- **Feedback is the fight**. Corruptor EMPs (+30 FB), turret hits (+4 FB each), egress dashes (25 FB), mend heals (20 FB), aegis (30 FB). The feedback bar is under siege from every direction. Spillover = integrity damage = death spiral. The player must constantly track their feedback and make hard choices about which abilities to use and when.
- **Kill corruptors above all else**. Their EMP is unpredictable (comes from wherever they are, not the fixed center node) and their scorch trails stack on top of the floor burn for extra DOT. A corruptor alive for a full sprint cycle is a run-ender.
- **Sub_mend cadence**: With 2/sec floor burn + intermittent turret/projectile hits, the player needs to mend roughly every 10s (its cooldown). Missing a mend cycle means integrity spirals. This is the healing check. Sub_mend's instant 50 HP heal buys ~25s against floor burn alone — that's the rhythm.
- **Stop shooting during Mirror Aegis**. 3 seconds of patience. Use the window to kill adds — corruptors and hunters are high-value targets during aegis downtime.
- **No safe ground — accept the burn**. Unlike Phase 2, there's nowhere to hide from the floor. The player takes 2/sec no matter what. The question is whether they can kill Pyraxis before the burn + turrets + feedback overwhelm their healing. Every second spent not dealing damage to the node is a second closer to death.
- **This phase is SHORT**. 2500 HP at ~50-70 effective DPS = 35-50 seconds. Aggression wins. Play scared and the floor burn + feedback pressure grinds you down. The player who attacks during every safe window finishes this phase before the desperation ramp gets out of hand.

---

## Victory

1. All turrets deactivate — beams die, housings go dark
2. Burn zones extinguish — floor cools to neutral
3. Spawned enemies die instantly
4. The swirl destabilizes — rotation reverses, particles scatter outward in a violent burst, ember particles flare white-hot then fade. The massive dark cloud disperses like smoke, revealing only the two burning eyes suspended in empty space.
5. Conduit lines in the arena walls flicker and go dark
6. Eyes dim slowly — the last points of light in the arena. The last thing to die.
7. Brief silence. The arena cools. The reactor grid is visible now — cold steel, no reflection, no light.
8. Inferno fragment drops from where the eyes were — **this fragment never times out**. It persists until collected. The player earned this.
9. Post-defeat voice line (CRU-09 recording)
10. On fragment collection, the player is transported back to the Pyraxis portal landmark within the Crucible zone.

---

## Damage Tuning Reference

### Arena Threats → Player

| Attack | Damage | Type | Notes |
|--------|--------|------|-------|
| Ember Volley (single projectile) | 10 | Integrity | From node, 16 per ring, dodge gaps |
| Heat Pulse (expanding ring) | 20 | Integrity | From node, ~20s cycle |
| Inferno turret beam (per tick) | 8 | Integrity + 4 Feedback | ~10 ticks/sec while in beam = 80 DPS + 40 FB/sec |
| Center burn (permanent) | 5/sec | Integrity | Always active — approaching Pyraxis costs HP |
| Directional burn zone | 5/sec | Integrity | P1: 1 zone, P2: 2 opposing, P3: all 4 |
| Floor burn (Phase 3 total) | 2/sec | Integrity | Entire arena, no safe ground |
| Fire Seeker dash | 30 | Integrity | P1+ adds |
| Corruptor scorch trail | 5/sec per footprint | Integrity | P2+ adds — contaminates safe pockets |
| Corruptor EMP | 30 Feedback + minor integrity | Feedback | P2+ — unpredictable position, can't outrange |
| Hunter projectile | ~10 | Integrity | P3 adds — ranged pressure from distance |
| Reflected projectile (Mirror Aegis) | Player's own damage | Integrity | P3 — don't shoot the shield |

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

## Arena Zone Prerequisites

The arena zone file must be hand-authored before the Pyraxis fight can be implemented. The following spawn/marker types are read by the boss module to wire up encounter mechanics:

| Spawn Type | Count | Purpose |
|------------|-------|---------|
| `boss_pyraxis` | 1 | Pyraxis center position (grid 511,511) |
| `turret` | 8 | Inferno turret positions — one per 2×2 circuit pillar |
| `fire_spawner` | 4 | Spawn alcove positions (N/S/E/W wall midpoints) for seeker/hunter/corruptor adds |
| Portal/savepoint | 1 | Player entry point from the Crucible portal |

**Arena geometry**: 96x96 inner arena (grid 463-560) with thick solid border ring (grid 431-592, 162x162 total) for camera containment. Turret housing structures at each `turret` position, alcove recesses at each `fire_spawner` position, and open center area for Pyraxis.

**Zone file**: `resources/zones/pyraxis.zone` — `size 1024`, arena geometry centered at grid 511,511. 2 cell types: solid (dark gray) and circuit (dark red, yellow circuit pattern). Music and color palette set to fire theme.

---

## Implementation Considerations

- Pyraxis is a unique entity — custom boss module (`boss_pyraxis.c/h`)
- State machine: `INTRO → PHASE_1 → TRANSITION_1 → PHASE_2 → TRANSITION_2 → PHASE_3 → DYING → DEFEATED`
- **Swirl cloud rendering**: `particle_instance` system drives the core visual — hundreds of instanced quads orbiting center with varying radius, speed, and color (mostly black/dark red, scattered ember). Phase transitions adjust rotation speed, color balance, and particle count.
- **Reactor grid background**: Custom background renderer for the boss arena. Renders dark gray squares as quads with stencil value 2, sampled by `MapReflect` pipeline for metallic cloud reflection. Parallax rate distinct from cloudscape. Replaces standard background for this chunk.
- **Inferno turrets reuse `sub_inferno_core`** — same beam system the player will earn. Each turret embeds a core state struct with its own position and current angle. Boss module advances the angle each tick based on rotation speed (°/sec) and direction (CW/CCW). Turret beam fires from pillar center outward — pillar geometry does not block its own beam. Extended TTL (640ms vs 500ms) for 16-cell range.
- **Mirror Aegis reuses `sub_shield_core`** visual adapted for boss scale
- Boss HP bar: dedicated UI element (wide bar, top of screen, phase notches)
- Burn zones: 4 directional zones (top/bottom/left/right thirds) + permanent center 32×32, managed by boss module state machine
- Eye rendering: two bright points + bloom FBO, player-tracking angle math. Eyes are positioned within the swirl but rendered on top (higher render pass) so they're always visible through the churning particles.
- Phase transitions: brief cinematic-like moments (player input still active, node invulnerable, arena transforms)
- Arena floor burn (Phase 3): shader effect or dense particle field
- Arena chunk: hand-authored 96x96 inner arena (162x162 total with border) with turret housing positions, burn zone plate positions, spawn alcove positions, and Pyraxis center position
- **Portal access**: Landmark in the Crucible procgen zone contains a portal to the boss arena chunk

---

## Implementation Phases

Each phase is implemented, tested, and tuned before moving to the next. This lets us nail the feel of each layer in isolation before stacking them.

### Phase A — The Node

Build Pyraxis as a shootable entity in the arena. The player can enter, see it, fight it, kill it.

- **boss_pyraxis.c/h** — core module, state machine (simplified: `ACTIVE → DYING → DEFEATED` initially, expand to full phase machine later)
- **Swirl rendering** — particle_instance system: hundreds of dark blobs orbiting center, scattered embers. 1600u diameter. Get the visual presence right first.
- **Eyes** — two bright points tracking the player, rendered on top of the swirl via higher render pass. Bloom source.
- **Damage model** — 10,000 HP, always damageable, collision hitbox sized to the swirl
- **Boss HP bar** — wide bar at top of screen with phase threshold notches (65%, 25%)
- **Death sequence** — swirl disperses, eyes linger and dim, fragment drops
- **Elite fragment drop** — sub_inferno fragment, persists until collected, triggers zone transport on pickup
- **Ember volleys** — node projectile patterns (ring, spiral, aimed burst). Boss-specific projectile pool.
- **Heat pulse** — expanding ring attack
- **Zone wiring** — portal entry from Crucible, spawn point, player start position
- **Test/tune**: Does the node feel massive and alive? Is the swirl visually dominant? Do the ember patterns feel fair? Is the HP bar readable? Does death feel earned?

### Phase B — Reactor Grid Midground

The arena background. This is a **midground** layer — rendered between the bg_bloom cloudscape (far background) and the main world geometry (foreground). The dark metal grid sits behind the arena floor but in front of the clouds.

- **Reactor grid renderer** — regularly spaced grid of dark gray/near-black metallic squares with uniform gaps between them. Rendered as quads.
- **Stencil integration** — grid squares write stencil value 2, sampled by existing `MapReflect` pipeline for metallic cloud reflection. The bg_bloom cloudscape slides across their surfaces with parallax, giving subtle metallic glint on camera movement.
- **Parallax layering** — reactor grid scrolls at a different parallax rate than the cloudscape, creating visible depth. Clouds visible through the gaps between squares.
- **Render order** — after bg_bloom composite, before world flush. The arena walls and circuit pillars render on top of the grid.
- **Weapon lighting interaction** — turret beams and Pyraxis glow light up the reactor grid via the light_bloom FBO, briefly revealing the full grid in orange light before it fades back to near-darkness.
- **Test/tune**: Does the grid feel like infrastructure — cold steel behind the arena? Does the parallax create convincing depth? Does weapon lighting reveal the grid dramatically?

### Phase C — Turrets

Layer the inferno turrets onto the working node fight.

- **Turret state** — 8 turret structs, each with: pillar position, current angle, rotation speed (°/sec), direction (CW/CCW), active flag, disabled timer
- **Inferno beam** — each turret embeds a `sub_inferno_core` state struct. Boss module advances angle per tick, fires beam from pillar center outward. Extended TTL (640ms) for 16-cell range.
- **360° rotation** — continuous lighthouse sweep. Pillar does not block its own beam.
- **Phase escalation** — P1: 2 turrets at 30°/sec, P2: 6 at 45°/sec, P3: 8 at 60°/sec
- **Mine disable** — mine detonation on pillar knocks turret offline for 5s, 1s spin-up to reignite
- **Diagonal rotation (P2)** — swap which 2 diagonal turrets are active every ~15s
- **Full phase state machine** — expand to `INTRO → PHASE_1 → TRANSITION_1 → PHASE_2 → TRANSITION_2 → PHASE_3 → DYING → DEFEATED`
- **Phase transitions** — invulnerability windows, visual spectacle (beams die, conduits flare, etc.)
- **Mirror Aegis (P3)** — fire shield on node, reflects projectiles, reuses `sub_shield_core` visual
- **Test/tune**: Are rotation speeds readable in each phase? Do beam crossings feel dangerous but fair? Does mine disable feel rewarding? Is the DPS-vs-turret-control mine tradeoff meaningful? Do phase transitions feel dramatic?

### Phase D — Burn Zones

Layer the floor damage system onto the turret fight.

- **Center burn** — permanent 32×32 zone around Pyraxis, 5 integrity/sec
- **Directional zones** — 4 zones (top/bottom/left/right thirds of arena), managed by boss phase state
- **P1 rotation** — one zone at a time, 6s duration, 2s gap, 1.5s telegraph
- **P2 opposing pairs** — Top+Bottom or Left+Right, 10s duration, 2s swap telegraph, 1s gap
- **P3 total burn** — all four zones active, 2 integrity/sec everywhere
- **Visual telegraph** — zone floor glows orange building to full brightness before damage starts
- **Desperation** — P3 below 10% HP bumps floor burn to 3 integrity/sec
- **Test/tune**: Is the P1 single-zone rotation teaching effectively? Are P2 safe pockets large enough to fight in but small enough to feel compressed? Is P3 total burn survivable with sub_mend cadence? Does the desperation ramp feel like a final push, not a cheap death?

### Phase E — Enemy Spawns

Layer adds onto the complete arena.

- **Spawn alcoves** — 4 positions at N/S/E/W wall midpoints
- **P1 seekers** — 2 every 25s, max 2 alive. Training wheels.
- **P2 seekers + corruptors** — 1+1 every 18s, max 4 alive. Corruptor scorch trails contaminate safe pockets, EMP feedback spikes compound pressure.
- **P3 all three types** — 1 seeker + 1 hunter + 1 corruptor every 15s, max 6 alive. Adds take floor burn damage (death timer). Hunters add ranged pressure.
- **Floor burn on adds (P3)** — adds take the 2/sec ambient burn, giving them a natural lifespan
- **Test/tune**: Is add pressure enough to demand attention without overwhelming the turret/burn mechanics? Are corruptor EMPs too punishing in Phase 2 safe pockets? Is Phase 3 add cap (6) too chaotic? Do adds feel like part of the fight or a distraction from it?

### Implementation Order Rationale

Each phase builds on the last and can be playtested in isolation:
1. **Node first** — you need something to shoot at. Nail the visual presence and core combat loop.
2. **Reactor grid** — establishes the arena atmosphere. Once this is in, the arena feels like a real place.
3. **Turrets** — the primary mechanical threat layer. This is where the fight's identity emerges. Turrets + node = the core Pyraxis experience.
4. **Burn zones** — spatial pressure on top of turret patterns. Forces movement decisions. This is where phase escalation kicks in.
5. **Enemy spawns** — chaos multiplier on top of everything. The final layer that pushes the fight from "hard pattern game" to "full combat management." Added last because add tuning depends on how much pressure turrets + burn already create.

---

## Open Questions

1. **Miniboss identity**: Single buffed enemy or a pair? Which enemy type? Playtest answer.
2. **Inferno turret visual**: Same particle blobs as player's inferno, or scaled up? Leaning toward same particles — **the skill is the skill**.
3. **Boss HP bar**: Always visible or only when damaged? Leaning toward always visible in boss arena.
4. **Sub_stealth vs Pyraxis**: A root-level system process shouldn't be fooled by cloak. Options: immune to stealth, or stealth drops aggro for 1-2s before the node "finds" the player again (reduced effectiveness, flavor win, doesn't break the fight).
5. **Turret rotation tuning**: Rotation speeds (30/45/60°/sec across phases) and beam range (16 cells) need playtesting. Too fast = unfair, too slow = trivial.
