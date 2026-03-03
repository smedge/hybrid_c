# Pyraxis Boss Fight Spec

## Overview

**Zone**: The Crucible — Boss Arena (dedicated small zone, ~40x40 cells)
**Entry**: Portal guarded by a miniboss (buffed Fire Hunter or Fire Seeker)
**Fight Duration**: 3-5 minutes total, full restart on death
**Reward**: sub_inferno (elite channeled fire beam)
**Voice Lines**: Phase transition lines (short, punchy — not mid-combat taunts)

## Design Philosophy

**"Final Exam"** — Each phase tests a cluster of skills the player developed surviving the Crucible zone. The fight escalates from pattern reading to spatial management to everything-at-once.

**"Preview Before Earn"** — Pyraxis's signature attack is the Inferno beam, the very subroutine the player earns upon victory. The player experiences the weapon from the receiving end first, building anticipation and respect for the tool they're about to gain.

**"Transformation Through Destruction"** — Pyraxis's entire philosophy embodied mechanically. The fire that tried to destroy the player becomes their greatest weapon.

## The Arena

Small dedicated zone. Roughly 40x40 cells.

- **Shape**: Circular/octagonal open arena with a raised central platform (Pyraxis's perch for Phase 1)
- **Walls**: Ring of solid blocks forming the arena boundary — nowhere to flee
- **Fire vents**: 8-12 grate cells around the arena perimeter that activate as fire hazard zones during later phases
- **No cover**: This is a bullet-hell arena, not a hide-and-peek encounter
- **Visual**: Intense orange/red palette, the floor pulses with heat, Pyraxis's bloom presence fills the space

## Pyraxis — Visual Design (Balrog Riff)

Pyraxis is the Balrog of cyberspace — a towering dark presence backlit by its own fire. Not a literal demon, but the same visual energy translated into the game's geometric art style: **shadow and flame in constant tension**.

### The Body
- **Scale**: Roughly 6-8x the size of a hunter. Pyraxis should dominate the arena visually.
- **Core silhouette**: Dark geometric humanoid torso/head shape built from triangles — deep blacks and very dark reds. The body itself is almost a void, absorbing light.
- **Crown of horns**: Angular spikes radiating from the head — sharp triangle geometry, black with faint ember-orange edges. Evokes the Balrog's horned silhouette.
- **No legs**: The lower body dissolves into roiling fire particles — Pyraxis floats/drifts above the arena floor, trailing flame beneath it like a burning cloak dragging on the ground.

### The Eyes
- **Two burning eyes**: Bright white-hot cores with orange-yellow bloom halos, set in the dark face geometry. The brightest points on the entire entity — they cut through the shadow silhouette like headlights.
- **Rendered as points + bloom**: Small bright triangles for the core shape, rendered into the bloom FBO at high intensity so the glow bleeds outward. The eyes should read clearly from anywhere in the arena.
- **Tracking**: The eyes follow the player. Always. Even during attack animations, the eye positions subtly orient toward the player's location. This sells "superintelligence" — it's always watching.
- **Phase escalation**: Phase 1 eyes are steady, calm — observing. Phase 2 they pulse brighter during attacks (the fire in them flares). Phase 3 they leave brief afterimage trails when Pyraxis moves, like embers dragged through darkness.
- **Emotion through intensity**: When Pyraxis channels Inferno, the eyes narrow (geometry compresses vertically) and flare to maximum brightness. During Mirror Aegis, they dim slightly — contempt. On death, they're the last thing to extinguish.

### Fire & Shadow Interplay
- **Ember cracks**: The dark body surface has fracture lines of bright orange-white, like cooling magma. These pulse brighter when Pyraxis attacks.
- **Fire aura**: Constant particle emission from the body edges — small flame blobs drifting upward and outward, using the particle_instance system. Dense near the body, sparse further out.
- **Shadow corona**: The bloom system renders the fire glow, but the body itself is dark enough to read as a silhouette AGAINST its own bloom halo. Dark center, bright edges. This is the key Balrog visual — you see the shape defined by the fire around it, not by the shape itself.

### The Wings
- **Wings of flame**: Two large wing-shapes rendered as triangle fans extending from the shoulders — translucent dark orange/red, flickering geometry (vertices jitter slightly each frame for a "heat shimmer" effect).
- **Wing bloom**: The wings render into the bloom FBO as bright orange sources, so they get the soft halo treatment — looks like wings made of heat and light rather than solid geometry.
- **Phase escalation**: Wings grow larger/brighter with each phase. Phase 1 they're subdued (folded, smoldering). Phase 2 they spread. Phase 3 they're fully unfurled and blazing.

### The Inferno Beam (Balrog's Whip)
- The beam channels from Pyraxis's "hand" area — when sweeping, it reads like a **whip of fire** being lashed across the arena.
- Same particle blob system as sub_inferno_core but denser and wider.
- During beam channel, the wing on the sweeping side flares brighter.

### Phase Transformations
- **Phase 1 → 2**: Wings unfurl from folded to spread. Ember cracks intensify. Fire aura doubles in density. The shadow gets darker as the fire gets brighter (more contrast).
- **Phase 2 → 3**: Full ignition. Wings are massive and blazing. The dark body starts breaking apart at the edges — chunks of dark geometry flake off and burn away, revealing more fire underneath. Pyraxis is literally burning itself up in the final push. By the time it dies, the silhouette is more fire than shadow.

### Death Visual
- The fire extinguishes from the extremities inward — wings collapse, aura dies, ember cracks go dark.
- The dark silhouette remains for a moment, then shatters into fragments (callback to the fragment collection mechanic).
- Final bloom pulse — one last burst of orange light, then darkness. The arena cools.

## HP & Phase Thresholds

**Total HP: 10,000**

| Phase | HP Range | Damage to Clear | Approx Duration |
|-------|----------|-----------------|-----------------|
| Phase 1 — The Trial | 100% → 65% | 3,500 | 60-75 sec |
| Phase 2 — The Crucible | 65% → 25% | 4,000 | 75-90 sec |
| Phase 3 — The Transformation | 25% → 0% | 2,500 | 45-60 sec |

**Phase transition**: Brief invulnerability window (~2 seconds) while Pyraxis transforms. Voice line plays. Arena state changes. Player gets a breath.

**Damage math basis**: Player at this point has sub_pea/mgun (~100 DPS sustained) + mines (~100 burst per mine). With dodging reducing uptime to ~40-50%, effective DPS is ~50-70. Total fight damage of 10,000 at ~60 effective DPS = ~165 seconds of dealing damage, total fight with downtime = ~240-300 seconds (4-5 min). This tracks.

## Phase 1 — "The Trial"

**Theme**: Pyraxis observes. Tests whether the player can read patterns.
**Tests**: Dodging projectile patterns, finding damage windows, patience.
**Duration**: 60-75 seconds

### Pyraxis Behavior
- **Position**: Stays on central platform, rotates slowly, does NOT chase the player
- **Primary attack — Ember Volley**: Fires radial projectile bursts in bullet-hell patterns
  - Pattern A: 12-projectile ring expanding outward (gaps to weave through)
  - Pattern B: Spiral arm pattern (3 arms, rotating) — requires circular movement to dodge
  - Pattern C: Targeted 5-shot burst aimed at player position (hunter-style but faster)
  - Cycles through patterns with 1.5-2s gaps between volleys
- **Secondary attack — Heat Pulse**: Every ~15s, expanding ring of fire from center (must dash or outrun)
- **Vulnerability**: Always damageable, but projectile patterns keep pressure constant

### Player Strategy
- Circle-strafe the arena, weave through projectile gaps
- Use movement subs (egress/boost) to dodge heat pulses
- Land shots during pattern transitions
- Mines near the center for burst damage during lulls

### Phase 1 Voice Line (on enter)
Short line — Pyraxis acknowledges the player has arrived. Delivered during the initial 2-3 seconds before attacks begin.

### Phase 1 → 2 Transition
At 65% HP: Pyraxis pauses, pulls all projectiles inward (visual implosion), the central platform fractures. Voice line plays: something about the player surviving observation, now facing the real test.

---

## Phase 2 — "The Crucible"

**Theme**: Pyraxis unleashes Inferno. The arena becomes hostile.
**Tests**: Spatial awareness, movement under pressure, area denial management.
**Duration**: 75-90 seconds

### Arena Changes
- **Fire vents activate**: 3-4 of the perimeter vents ignite simultaneously, creating burn zones that rotate/cycle (different vents activate every ~8s)
- **Arena shrinks conceptually**: Safe space is reduced, player must track which zones are dangerous
- Fire vent burn zones deal DOT damage (integrity drain) if the player stands in them

### Pyraxis Behavior
- **Position**: Now mobile — Pyraxis leaves the center and moves around the arena (slow, deliberate, not chasing aggressively)
- **Signature attack — Inferno Beam**: Pyraxis channels its own Inferno beam
  - **Sweep pattern**: Beam activates and sweeps a 120-180 degree arc over ~2 seconds
  - **Telegraph**: 0.8s charge-up with visible targeting line before the beam fires
  - **Damage**: High (40-50 per tick, rapid ticks) — getting caught is devastating but not instant death
  - **Dodge**: Player must dash perpendicular to the sweep, or get behind Pyraxis during the channel
  - **Cooldown**: ~8s between beam attacks
  - **The beam leaves a brief fire trail** on the ground (2-3s duration) — can't just dodge back through where it swept
- **Secondary attack — Ember Volley (reduced)**: Continues Phase 1 projectile patterns between beam attacks, but fewer projectiles per volley
- **New attack — Fire Seekers**: Spawns 2 small fire seeker adds every ~20s (low HP, ~30 each, but their dash trails leave fire corridors)

### Player Strategy
- Watch for Inferno telegraph — dash perpendicular to the sweep
- Prioritize fire seeker adds quickly (they compound the area denial)
- Track vent rotation to maintain safe positioning
- Damage Pyraxis during beam cooldown windows and while it's stationary channeling (attack from behind during sweep)

### Phase 2 → 3 Transition
At 25% HP: Pyraxis absorbs all fire in the arena — vents die, seeker adds explode, brief moment of total darkness. Then everything ignites simultaneously. Voice line about transformation/becoming.

---

## Phase 3 — "The Transformation"

**Theme**: Pyraxis mirrors the player's toolkit. Everything at once.
**Tests**: Feedback management, ability timing, the full toolkit under maximum pressure.
**Duration**: 45-60 seconds (shortest but most intense)

### Arena State
- **Constant ambient burn**: The entire arena floor deals low tick damage (1-2/sec). Player MUST use sub_mend to sustain, or play perfectly
- **Intermittent safe zones**: Small areas (2-3 cells) that cycle around the arena floor, glowing brighter — standing in these negates the floor burn
- This forces constant repositioning, can't camp

### Pyraxis Behavior — The Mirror
Pyraxis cycles through "subroutine mirror" attacks, using corrupted versions of player abilities:

- **Mirror Inferno**: Faster beam sweeps (full 360 degree rotation over ~3s, must find the gap or dash through)
  - Now THE defining threat — Pyraxis channels this more frequently (~6s cooldown)
  - The beam damage creates feedback pressure on the player (not just integrity damage)

- **Mirror Aegis**: Pyraxis activates a fire shield (3-4 seconds duration, ~10s cooldown)
  - While shielded, projectiles are reflected back at the player
  - Player must STOP attacking during shield and focus on dodging
  - Tests patience and awareness (don't DPS mindlessly)

- **Mirror EMP**: Expanding feedback pulse (like corruptor EMP but larger radius, 400 units)
  - Hits player with a large feedback spike (+30 feedback)
  - Forces the player to manage their feedback bar carefully
  - If player is already high feedback from damage taken + abilities used, this can push into spillover
  - Telegraph: visible pulse charge for 1s before detonation

- **Desperation (below 10% HP)**: Attack speed increases, beam cooldown drops, final push of intensity

### Player Strategy
- Constant movement to stay in safe zones (avoid floor burn)
- Sub_mend timing is critical — burn through heals managing floor + EMP feedback
- Sub_aegis is the "save" for mistakes — if caught by a beam or pushed to spillover
- Stop shooting during Mirror Aegis (reflected projectiles are punishing)
- Sub_egress to dodge beams, mines for burst when Pyraxis is stationary during channel
- This phase is SHORT — aggressive DPS in safe windows, the boss is almost dead

### Victory
Pyraxis collapses. Fire in the arena extinguishes. Post-defeat voice line plays (CRU-09 recording — the "you passed through my fire" speech). Inferno fragment drops.

---

## Damage Tuning Reference

### Pyraxis Attacks → Player

| Attack | Damage | Type | Notes |
|--------|--------|------|-------|
| Ember Volley (single projectile) | 10 | Integrity | 12 per ring, dodge gaps |
| Heat Pulse (expanding ring) | 20 | Integrity | Phase 1 only, ~15s cycle |
| Inferno Beam (per tick) | 8 | Integrity + 4 Feedback | Rapid ticks while in beam (~10/sec = 80 DPS + 40 FB/sec) |
| Fire trail (ground DOT) | 5/sec | Integrity | Brief duration (2-3s) |
| Fire Seeker dash | 30 | Integrity | Phase 2 adds |
| Floor burn (Phase 3) | 2/sec | Integrity | Ambient, safe zones negate |
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

**Guarding the portal to the Pyraxis arena.**

Not a complex encounter — a skill check to confirm the player is ready.

- **Concept**: A Fire Hunter with 2-3x normal stats (300 HP, faster fire rate, burn zones on projectile impact)
- **OR**: A Fire Seeker with buffed HP (200 HP) + longer dash trails + faster recovery
- **The miniboss is a gatekeeper, not a puzzle** — if you can beat this, you're ready for Pyraxis
- Defeating it opens the portal to the boss arena
- Could also be: a pair of fire enemies (Hunter + Seeker combo) rather than a single buffed one

> Decision: TBD — playtest which feels better as a gate check

---

## Voice Line Moments

| Moment | Trigger | Content Direction |
|--------|---------|-------------------|
| Arena Enter | Player enters zone | Existing CRU-01 or new short line. Pyraxis acknowledges the challenger. |
| Phase 1 → 2 | HP hits 65% | "You can read patterns. Let us see if you can survive what patterns cannot describe." |
| Phase 2 → 3 | HP hits 25% | "Still here. Then let me show you what you could become." (Inferno intensifies) |
| Defeat | HP hits 0 | Existing CRU-09 recording — the full post-defeat speech |

> Note: These are content direction, not final scripts. Actual lines need to match Matthew Schmitz's delivery style and the established Pyraxis voice.

---

## Implementation Considerations (for later)

- Pyraxis is a unique entity — not spawned from the enemy pool, custom boss module
- State machine: INTRO → PHASE_1 → TRANSITION_1 → PHASE_2 → TRANSITION_2 → PHASE_3 → DYING → DEFEATED
- Inferno beam reuses sub_inferno_core rendering (particle_instance system) — Pyraxis as a "user" of the core, same as player will be
- Boss HP bar: dedicated UI element (not the normal enemy health display)
- Fire vent cells: zone-specific map cells with timed activation
- Phase transitions: brief cutscene-like moments (player input still active, Pyraxis invulnerable, visual transformation plays)
- Arena floor burn (Phase 3): shader effect or dense particle field
- Mirror Aegis: sub_shield_core visual adapted for boss scale

---

## Open Questions

1. **Miniboss identity**: Single buffed enemy or a pair? Which enemy type? Playtest answer.
2. **Inferno beam visual from boss**: Same particle blobs as player's inferno, or scaled up / different color? Leaning toward same particles but MORE of them (wider beam).
3. **Does Pyraxis have a visible HP bar the whole fight, or only when damaged?** Leaning toward always visible in boss arena.
4. **Phase 3 safe zones**: Random positions or predictable pattern the player can learn? Leaning toward semi-predictable (they orbit the arena edge on a timer).
5. **Should sub_stealth work on Pyraxis?** A superintelligence probably shouldn't be fooled by stealth. Boss could be immune — or stealth drops aggro briefly but Pyraxis "finds" you after 1-2s (reduced effectiveness, flavor win).
