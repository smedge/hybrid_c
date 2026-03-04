# Boss Fight Framework — Node-in-Arena Design

## Core Concept

Every boss in Hybrid is a **node** — a fortified system process embedded in a purpose-built arena. The node is the objective. The arena is the threat.

The player doesn't fight a creature. They dismantle a fortified system from the inside while its defense infrastructure tries to kill them. The node commands the room: turrets, hazard zones, enemy spawns, environmental transformations. The arena architecture IS the boss's body. The node is its brain.

This is true to the fiction. Every element in Hybrid is a program. Enemies are security processes. Abilities are subroutines. The map is network topology. Bosses are the root processes of their network sectors — the most deeply embedded, most heavily defended nodes in the system. They don't chase you. They don't need to. They control everything around them.

### The Mother Brain Principle

The tension comes from the room, not from the boss moving toward you. The node is the objective you must damage. Everything else in the arena exists to prevent that. The player's challenge is surviving the environment long enough to find and exploit damage windows on the node.

### Fiction

Each themed zone's boss is the **sector controller** — the root-level security process that governs that entire zone. It's why the zone has its theme. The fire zone burns because its controller runs fire protocols. Killing the controller doesn't extinguish the zone, but it cripples the system's coordination, opening the path forward.

The alien zone's final boss is the alien entity's primary projection node — the point where the foreign intelligence interfaces with human cyberspace.

---

## The Node

### What It Is

A visually distinct architectural object embedded in the arena. Not a sprite that moves around. A piece of the room's infrastructure that happens to be alive.

### Visual Language

- **Core shape**: Geometric — hexagonal, octagonal, crystalline, or circuit-like. Matches the game's angular art style. NOT organic, NOT creature-shaped.
- **Eyes**: Every node has eyes. Two bright points that track the player constantly. This is what sells "intelligence" — the node is always watching. Eyes are the primary source of personality and menace.
- **Pulse**: Nodes pulse with energy matching their theme color. Fire node glows orange with ember cracks. Ice node pulses cold blue. The pulse intensifies with phase transitions.
- **Scale**: Nodes are large (4-6x a hunter) but not absurdly so. They're imposing because of what they control, not because they fill the screen.
- **Bloom**: Nodes render into the bloom FBO. They glow. They're the brightest thing in the arena. The player's eye is always drawn to them.

### Node Behaviors (Per Boss)

Not every node behaves the same. This is a key variety lever:

| Behavior | Description | Example |
|----------|-------------|---------|
| **Stationary** | Fixed position, never moves. Pure arena fight. | Pyraxis — enthroned in the architecture |
| **Socket-hopping** | Teleports between pre-defined sockets in the arena architecture. Vulnerable during transition or at each socket briefly. | Ice boss — jumps between frozen pillars |
| **Track-riding** | Moves along a defined path (rail, orbit). Predictable but forces repositioning. | Electric boss — circuits around the arena perimeter |
| **Phased positioning** | Stationary per phase, but relocates during transitions. Each phase has a different arena geometry around the node's new position. | Poison boss — retreats deeper into the arena each phase |
| **Split/decoy** | Creates copies of itself. Only the real one takes damage. Others still command arena systems. | Void boss — three nodes, one real, swaps each phase |
| **Shielded/gated** | Node is invulnerable until the player completes an objective (kill adds, destroy shield generators, activate switches). | Final alien boss — destroy conduits to expose the core |

### Damage Windows

Nodes aren't always vulnerable. Each boss defines when and how the node can be damaged:

- **Always vulnerable** (easy to hit, hard to survive reaching it) — Pyraxis
- **Phase-gated invulnerability** (kill adds or destroy generators to open a damage window)
- **Pattern-gated** (node exposes itself during certain attack patterns — hit it while it's "channeling")
- **Position-gated** (must reach a specific location in the arena to have line of fire)

---

## The Arena

### What It Is

The arena is the boss's body. Its weapons. Its immune system. Every hazard, every turret, every spawn wave is the node defending itself through its infrastructure. The arena is purpose-built for the fight — a dedicated chunk in the procgen system, hand-authored geometry.

### Arena Shape (Variety Lever)

| Shape | Feel | Example |
|-------|------|---------|
| **Open circle/octagon** | Bullet-hell space, nowhere to hide, pure dodge | Pyraxis — fire arena |
| **Ring/corridor** | Claustrophobic, forced pathing, ambush-heavy | Poison boss — toxic tunnels |
| **Multi-room** | Node in one room, hazard generators in others. Must leave and return. | Electric boss — disable generators in side rooms |
| **Tiered/layered** | Vertical feel (inner/outer rings, elevated center). LOS varies by position. | Void boss — concentric rings with gaps |
| **Shifting** | Arena reconfigures between phases. Walls move, paths open/close. | Blood boss — arena bleeds and reshapes |
| **Symmetrical** | Mirror-image arena. Hazards and spawns from both sides. Tests multidirectional awareness. | Ice boss — frozen symmetry |

### Arena Scale (Variety Lever)

Not every arena should be the same size. Scale serves the fight's identity. For reference: the player's screen view is roughly 1600-2000 world units across (16-20 cells). A 40×40 arena is about 2-2.5 screens wide.

| Size | Cells | World Units | Feel | Example |
|------|-------|-------------|------|---------|
| **Small** | ~30×35 | ~3000×3500 | Claustrophobic, pressure cooker. Hazards fill the limited space. Nowhere comfortable to stand. | Poison boss — tight corridors, toxic pools everywhere |
| **Medium** | ~40×40 | ~4000×4000 | Classic bullet-hell arena. Big enough for safe corridors, small enough you can't escape the pressure. Node always visible. | Pyraxis — open fire arena. Blood boss — medium with side chambers. |
| **Large** | ~50×50 | ~5000×5000 | Room to breathe, but more to track. Mechanics that need space to telegraph. Multiple points of interest spread apart. | Ice boss — pillars to hop between, long sight lines, blizzard sweeps need room. Void boss — 3 decoy nodes spread across concentric rings. |
| **Elongated** | ~30×60 | ~3000×6000 | Forces directional movement. Different feel from square arenas. The shape itself constrains strategy. | Electric boss — ring/track shape, node circuits the perimeter |

Arena scale should contrast between bosses. If Pyraxis is a medium open circle, the next boss the player encounters should feel spatially different — tighter, wider, or shaped differently. No two arenas should feel like the same room with different paint.

### Threat Composition (Variety Lever)

This is where each boss becomes truly distinct. The node's theme determines WHAT fills the arena:

| Threat Type | Description |
|-------------|-------------|
| **Turrets** | Fixed emplacements that fire patterns (sweeps, bursts, tracking). Can be destroyed or have cooldowns. |
| **Hazard zones** | Areas of the floor that activate/deactivate on timers. Burn zones, freeze zones, poison pools, etc. Telegraph before activating. |
| **Enemy spawns** | The node summons themed enemies. Can be finite (kill wave to progress) or infinite (pressure while you focus the node). |
| **Environmental sweeps** | Arena-wide attacks — expanding rings, rotating beams, cascading walls of hazard. |
| **Terrain transformation** | The arena itself changes — walls appear/disappear, safe ground becomes hazardous, corridors open or seal. |
| **Projectile patterns** | Bullet-hell patterns emanating from the node or from the architecture. Radial bursts, spiral arms, aimed volleys. |
| **Debuff fields** | Areas that apply non-damage effects — feedback spikes, speed reduction, subroutine lockout, regen suppression. |
| **Reflected/mirrored abilities** | The node uses corrupted versions of player subroutines. Creates recognition + adaptation ("I know what that is, how do I dodge MY OWN ability?"). |

Each boss should emphasize 2-3 threat types and use others sparingly. If every boss throws everything at the player, nothing feels distinct.

### Pacing (Variety Lever)

| Pacing Type | Description | Example |
|-------------|-------------|---------|
| **Escalating** | Slow start, each phase adds threats. Players learn mechanics incrementally. | Pyraxis — observation → inferno → everything |
| **Relentless** | Maximum pressure from moment one. Short, brutal. | Blood boss — nonstop spawns + drain |
| **Puzzle-then-execute** | Calm phase where player figures out the pattern, intense phase where they execute. | Void boss — decode which node is real, then burn it |
| **Burst windows** | Long survival phases punctuated by brief damage windows. Patience + execution. | Ice boss — survive the blizzard, hit during thaw |
| **Attrition** | Constant low-level pressure that compounds. Resource management fight. | Poison boss — slow drain, heal timing is everything |

---

## Shared Boss Systems

These systems are common to ALL boss encounters:

### Boss HP Bar

- Dedicated UI element — wide horizontal bar at top of screen
- Always visible once the fight begins
- Shows phase thresholds as notches on the bar
- Color matches the boss's theme
- Pulses when transitioning between phases

### Phase Structure

Every boss has **3 phases** (consistency helps players read the fight):

1. **Phase 1 — Introduction**: The boss shows its core mechanics. Player learns the arena's rules. Moderate pressure.
2. **Phase 2 — Escalation**: New mechanics layer on. Arena transforms. The "real" fight begins.
3. **Phase 3 — Desperation**: Maximum intensity. All mechanics active. Short but brutal. Often introduces one final twist.

Phase transitions include:
- ~2 second invulnerability window (the node transforms)
- Visual transformation of the node and/or arena
- Voice line from the node
- Brief reprieve for the player to reposition

### HP Tuning Baseline

Total boss HP is tuned to the player's expected DPS at that point in the game:

- **Target fight duration**: 3-5 minutes of active combat
- **Effective DPS assumption**: Player's theoretical max DPS × ~40-50% uptime (dodging reduces damage output)
- **Phase distribution**: ~35% / ~40% / ~25% (Phase 2 is the longest, Phase 3 is short and intense)

### Death & Reward

On defeat:
1. All arena hazards deactivate (turrets power down, zones extinguish, spawned enemies die)
2. Node death animation (theme-specific — fire extinguishes, ice shatters, void collapses)
3. The node's eyes are the last thing to go dark
4. Brief silence, then the arena "cools" — lighting returns to neutral
5. Fragment drop (the elite subroutine reward)
6. Voice line (post-defeat recording from the node)
7. Boss defeat is **permanent progression** — the node stays dead across zone revisits

### Voice Lines

Each boss has 4 voice moments:

| Moment | Trigger | Purpose |
|--------|---------|---------|
| Arena entry | Player enters the boss chunk | Establish presence — the node acknowledges the intruder |
| Phase 1 → 2 | HP threshold crossed | Escalation — the node stops holding back |
| Phase 2 → 3 | HP threshold crossed | Desperation or transformation — the node's final form |
| Defeat | HP reaches 0 | Resolution — the node's last words, often recontextualizing the fight |

Voice delivery: Short, punchy, delivered between phases (not mid-combat chatter). The node speaks as a system — clinical, authoritative, with personality bleeding through the machine voice.

### Miniboss Gate

Each boss arena is preceded by a **gatekeeper encounter** — a miniboss or scripted enemy composition that confirms the player is ready. This is a difficulty gate generated by the procgen system's gate landmark, not a separate arena.

Options per boss:
- Single buffed enemy (2-3x stats of a themed variant)
- Paired enemies (challenging composition)
- Scripted wave encounter

The gate is a skill check, not a puzzle. If you can beat the gate, you're mechanically ready for the boss.

---

## The Six Themed Bosses — Overview

Each themed zone's node commands defenses matching its theme. The node's personality, arena shape, and threat composition create a distinct fight identity.

| Zone | Node Name | Arena Scale | Arena Shape | Primary Threats | Node Behavior | Pacing | Reward |
|------|-----------|-------------|-------------|-----------------|---------------|--------|--------|
| Fire | Pyraxis | Medium (~40×40) | Open circle | Inferno turrets, burn zones, fire enemy spawns | Stationary | Escalating | sub_inferno |
| Ice | TBD | Large (~50×50) | Symmetrical, pillared | Blizzard sweeps, freeze zones, ice projectile patterns | Socket-hopping | Burst windows | TBD |
| Poison | TBD | Small (~30×35) | Ring/corridor | Toxic pools, DOT fields, regen suppression, debuff auras | Phased positioning | Attrition | TBD |
| Blood | TBD | Medium (~40×40) | Shifting, multi-room | Enemy swarm spawns, life drain fields, terrain bleeding | Shielded/gated | Relentless | TBD |
| Electric | TBD | Elongated (~30×60) | Ring/track | Chain lightning turrets, EMP pulses, charge zones | Track-riding | Escalating | TBD |
| Void | TBD | Large (~50×50) | Tiered/concentric rings | Decoy nodes, spatial distortion, darkness, disorientation | Split/decoy | Puzzle-then-execute | TBD |

### Final Boss — Alien Entity

The alien zone's boss breaks the framework intentionally. After 6 fights that taught the player to read nodes and arenas, the alien entity subverts expectations:

- The arena doesn't follow the rules (non-euclidean feel, impossible geometry)
- The node doesn't behave like a system process (alien, unpredictable)
- Mechanics from ALL previous bosses appear, remixed and layered
- This is the "final exam" for the entire game — every skill, every subroutine, every pattern the player has learned

The alien fight is its own spec. It should be designed AFTER the 6 themed bosses are established so it can meaningfully reference and subvert them.

---

## Implementation Architecture

### Boss Module Pattern

Each boss is a standalone module (e.g., `boss_pyraxis.c/h`):

- Custom state machine: `INTRO → PHASE_1 → TRANSITION_1 → PHASE_2 → TRANSITION_2 → PHASE_3 → DYING → DEFEATED`
- Embeds shared subroutine cores where applicable (inferno turrets use `sub_inferno_core`, EMP pulses use `sub_emp_core`, etc.)
- Arena hazards are managed by the boss module, not by the zone system
- Boss entity registers with the enemy registry for fragment drops and collision

### Shared Boss Infrastructure

Common code across all bosses (future `boss_common.c/h`):

- Boss HP bar rendering
- Phase transition system (invuln timer, voice line trigger, camera effects)
- Node eye rendering + player tracking
- Node pulse/bloom rendering
- Death sequence orchestration
- Defeat persistence (save flag per boss)

### Arena Chunks

Boss arenas are hand-authored chunk templates in the procgen system:

- Each boss has a dedicated chunk (~40-60 cells across)
- Chunk includes: arena geometry, turret positions (as spawn_slots), hazard zone positions, node socket positions
- The procgen gate system ensures the player must pass through the difficulty gate to reach the arena
- Arena geometry is static (doesn't change per seed) — the boss fight is a designed encounter, not procedural

### Subroutine Core Reuse

Bosses use the same shared cores as players and enemies:

- Inferno turrets → `sub_inferno_core`
- Shield phases → `sub_shield_core`
- EMP pulses → `sub_emp_core`
- Heal mechanics → `sub_heal_core`

This is consistent with the game's #1 rule: **the skill is the skill**. When a boss fires an inferno beam, it's the SAME inferno the player will earn. Same damage, same visuals, same sound. The player experiences the weapon from the receiving end before they wield it.

---

## Design Principles

1. **The node is the objective, the arena is the threat.** Never invert this. The moment a boss becomes "dodge the big thing chasing you," the design has failed.

2. **Each boss teaches one lesson.** The primary threat composition should make the player master one combat skill they'll need for the rest of the game. Pyraxis teaches spatial awareness. Ice teaches patience. Poison teaches resource management.

3. **3 phases, escalating.** Consistency in structure lets players focus on learning new mechanics rather than guessing at the fight's structure.

4. **The arena defines the boss, not the node.** Two bosses could have identical nodes with identical HP. If their arenas are different, the fights are completely different. Design the arena first, then dress the node.

5. **Respect the player's subroutine loadout.** Every boss should be beatable with multiple loadout strategies. No boss should require one specific subroutine (except thematically appropriate resistance subs for extreme difficulty reduction).

6. **Preview before earn.** The boss's signature attack IS the subroutine the player earns on victory. They experience it as a threat, then wield it as a tool. This is core to the progression fantasy.

7. **Eyes last.** On death, the node's eyes are always the last thing to go dark. This is the visual signature of every boss kill in the game.
