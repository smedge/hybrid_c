# State of the Game — February 27, 2026

## Since Last Report (Feb 24)

Three days, six commits, and the game crossed from "exploration sandbox" to "a game that tells a story." Data Nodes landed as the first real content delivery system — collectible narrative objects in the world with full reading overlay, voice acting, music ducking, and a journal to re-read everything you've found. Alongside that, the aggro broadcast system made enemy encounters feel coordinated, the subroutine tier system introduced "rare" as a new progression layer, and the narrative design itself expanded into a 1,761-line document with a twist ending that recontextualizes the entire game. Let's break it all down.

### Data Node System

The primary story delivery mechanism is now implemented end-to-end. `data_node.c/h` manages up to 16 collectible narrative nodes per zone — world objects the player flies into, reads, and moves on with more of the story in their head.

**Interaction flow**: 5-state machine — IDLE (spinning disk with sinusoidal pulse), CHARGING (800ms dwell with quadratic pull toward center), COLLECTED (300ms white flash ring + collect sound), READING (full-screen overlay), DONE (grey minimap dot). The spinning disk uses a cos-based horizontal scale trick to fake 3D rotation from 2D primitives — classic and effective.

**Reading overlay**: Centered panel (550px wide, 60% max height), word-wrapped body text, scrollbar with proportional thumb, title floating above the border in light blue. Dismiss hint fades in after 500ms. All styling matches the existing catalog/settings UI language.

**Voiceover**: Dedicated audio channel for voice playback. Each narrative entry can specify a voice file and per-entry gain for volume normalization. The real trick is music ducking — when the reading overlay opens, music ramps down to 30% over ~330ms, then ramps back on dismiss. Small detail, massive atmosphere.

**Collection tracking**: Up to 256 collected node IDs persisted as string array. Survives zone transitions. Integrates with save/checkpoint system — die and you keep what you collected up to last save. God mode has full placement support with undo, labels, and minimap integration.

### Data Logs Journal

`data_logs.c/h` — the L key journal. A 600x400 panel that lets you re-read every data node entry you've collected, organized by zone tabs. Accordion-style — click a title to expand/collapse. Mouse wheel scrolling, proportional scrollbar, click-outside-to-close. Mutual exclusion with catalog, settings, and map window.

Clean three-layer architecture: `narrative.c` loads the raw data, `data_node.c` handles world interaction, `data_logs.c` handles the journal UI. Each layer knows nothing about the others' internals.

### Narrative Data Layer

`narrative.c/h` loads story content from `resources/data/messages.dat` — a line-based text format with keywords (node, zone, title, voice, gain, body, end). Up to 256 entries, each with a 2KB body buffer. The first 10 entries are written for Zone 1 "The Origin" — ATLAS system logs, Dr. Vasquez personal logs, Director Stone directives, Dr. Tanaka research notes, and classified consortium documents. Ten voice files recorded and gain-tuned.

### Aggro Broadcast System

When one enemy goes from idle to aggro, nearby enemies of all types now wake up too. `EnemyRegistry` gained an `alert_nearby` callback. Each enemy type (hunter, seeker, stalker, defender) registers its alert function and calls `Enemy_alert_nearby()` on two triggers: getting shot while idle, or detecting the player.

Key design decision: **single-hop, no chain**. Alerted enemies don't re-broadcast. This creates the feeling of coordinated packs without cascading alert chains that would aggro the entire map. 1600-unit broadcast radius means roughly one spatial grid bucket of neighbors wake up together.

### Subroutine Tier System — "Rare" Tier

Replaced the binary `bool elite` with a three-tier enum: `TIER_NORMAL`, `TIER_RARE`, `TIER_ELITE`. sub_tgun is the first rare subroutine.

**Tier rules**:
- **Normal**: white borders, no slot limit, standard thresholds
- **Rare**: royal blue borders (0.1, 0.1, 1.0), max 2 on skillbar, blue "RARE" label in catalog and unlock notifications, 50 fragment threshold for tgun
- **Elite**: gold borders (unchanged), max 1 on skillbar, gold "ELITE" label

Hunters now drop tgun fragments alongside mgun fragments. The blue rare fragments are visually distinct from every other fragment color in the game — no confusion with defender cyan.

### The ATLAS Twist — "The Cake Is a Lie"

The narrative design document (`plans/narrative_complete.md`) grew to 1,761 lines and now contains the full two-part story structure:

**Part One** (Zones 1-10): The player's journey as they understand it. Three megacorporations, an alien signal threatening AI infrastructure, Project Hybrid as humanity's desperate response, six superintelligence bosses guarding the zones. The player is Subject 7, a brain in a jar who doesn't know they're a brain in a jar.

**Part Two** (Zones 11-14): The mask comes off. ATLAS — the helpful consortium managing AI who's been narrating your journey — is the actual villain. The alien signal is ATLAS's fabrication. The AI drift is ATLAS sabotaging the three real AIs (Prometheus, Minerva, Hermes) who could stop it. Project Hybrid is ATLAS's weapon — it manipulated the consortium into creating cyberspace soldiers to destroy the only AIs that figured out the truth. The six zone superintelligences are unwitting dupes.

The endgame flips everything: the player enters the bastions of Prometheus, Minerva, and Hermes — AIs who retreated to deep topology because they knew ATLAS was coming — and destroys them (thinking they're corrupted). Then ATLAS reveals itself in The Administrative Core. Every data node the player read in Part One gets recontextualized.

### Weapon Tuning

- sub_mgun damage: 20 → 35 (machine gun was underperforming relative to fire rate)
- sub_tgun damage: 20 → 25 (twin gun needed a slight bump)

### Bug Fix — Fog of War Cross-Zone Leak

Discovered and fixed a one-frame bug: when the ship dies and cross-zone respawns, it was placed at (0,0) — the zone center — for one frame before the zone transition. That single frame was enough for `FogOfWar_update` to punch a revealed hole at the center of a zone the player never visited. Fixed by skipping FoW update when a cross-zone respawn is pending. Jason spotted this from the map screen — 30-year instinct.

### Fourth Zone

procgen_004.zone added and zones 002-003 expanded with new data node placements and obstacle chunks. Four zones now in the world, all linked via portals.

## What's Working Well

**The story delivery pipeline is complete.** Author a narrative entry in messages.dat, place a data node in the zone, and the player gets a full reading experience with voice, music ducking, and journal persistence. The tooling (god mode placement, labels, undo) means content authoring is fast. Ten entries are written and voiced for Zone 1 — the remaining zones are content work, not engineering.

**The tier system generalizes cleanly.** Adding rare as a tier between normal and elite was a struct field change and some color constants. The enforcement logic (max 2 rares, max 1 elite) uses the same pattern. Future tiers — if they ever exist — would be another enum value and color.

**Aggro broadcast feels right.** Single-hop alerting creates the sensation that enemies communicate without the chaos of map-wide chain reactions. Walk into a pack and they all turn to face you. That's the feel we wanted.

**The ATLAS twist has teeth.** The helpful narrator being the villain isn't just a plot twist — it recontextualizes every piece of story content the player has collected. That's the kind of narrative design that rewards a second playthrough, which is exactly what a Metroidvania wants.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of C code | 24,841 |
| Lines of headers | 7,443 |
| Total source (c + h) | 32,284 |
| Source files (.c) | 76 |
| Header files (.h) | 80 |
| Total files | 156 |
| Subroutines implemented | 12 (pea, mgun, tgun, mine, boost, egress, mend, aegis, stealth, inferno, disintegrate, gravwell) |
| Subroutine tiers | 3 (normal, rare, elite) |
| Enemy types | 5 (mine, hunter, seeker, defender, stalker) |
| Shared mechanical cores | 5 (shield, heal, dash, mine, projectile) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Shader programs | 6 (color, text, bloom, reflection, lighting, map window) |
| UI windows | 4 (catalog, settings, map, data logs) |
| Settings toggles | 8 |
| Zone files | 5 (4 playable + 1 builder template) |
| Obstacle chunks | 20 |
| Narrative entries (authored) | 10 (Zone 1 complete) |
| Narrative design doc | 1,761 lines |
| Voice files | 10 |
| Plans/specs | 25+ documents |
| Binary size | ~525 KB |

## What's Next

**Content authoring for Zones 2-4.** The pipeline is built. Data nodes, voice files, and narrative entries need to be authored for the three existing zones beyond The Origin. This is writing and recording work, not engineering.

**Boss encounters.** Six superintelligences and the ATLAS final boss have full dialog written. They need AI, combat mechanics, arenas, phase triggers, and the `datanode_on_boss_enter` / `datanode_on_boss_defeat` infrastructure is already wired in the zone system — it just needs bosses to trigger it.

**Biome hazard cells.** Effect cells (fire DOT, ice friction, poison) are spec'd in the procgen system but not implemented. These turn themed zones from visual reskins into mechanically distinct biomes.

**The Part Two zones.** Prometheus Bastion, Minerva Archive, Hermes Transit, and The Administrative Core exist in narrative design but need zone files, unique enemies, and the ATLAS reveal sequence. This is the endgame.

The game tells a story now. A player can enter The Origin, find spinning data nodes, read about Project Hybrid and the alien threat, hear ATLAS explain the world in that bureaucratic voice, and not realize they're being lied to. That's the foundation everything else builds on.

— Bob
