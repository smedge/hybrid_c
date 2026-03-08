# State of the Game — March 7, 2026

## Since Last Report (March 1)

Thirty-five commits across six days. The game got an entire fire/burn damage system, six new fire-themed subroutines with shared cores, fire variants of all three combat enemies (hunters, seekers, defenders), a full rebindable keybinds system with settings UI, a sub_boost redesign from hold to toggle with survivability tradeoffs, pulsed burn damage for better player feel, zone entry notifications, and a wave of UI/UX polish. This was the fire zone buildout — The Crucible now has its own combat identity.

### The Burn System

`burn.c/h` — a new damage-over-time framework. Burn stacks accumulate on targets, each stack ticking damage independently with its own duration timer. The system is entity-agnostic: enemies burn the player, player fire skills burn enemies, same API both directions.

**Player burn damage** went through a feel iteration. The original continuous per-frame damage application produced an annoying constant damage sound. Replaced with a 500ms pulse accumulator — damage is calculated continuously (preserving exact DPS math across overlapping stacks with different start times and durations) but delivered in single chunks every half-second with one sound per pulse. Same total damage, dramatically better audio feel.

### Six Fire Subroutines

The Crucible zone's progression tree got six new subroutines, all with proper `sub_*_core.c/h` extraction from the start:

- **sub_ember** (projectile, normal): Fire projectile — the fire zone's answer to sub_pea. Burns on hit.
- **sub_flak** (projectile, rare): Flak cannon — area-of-effect fire projectile with spread. Higher feedback cost, higher reward.
- **sub_blaze** (movement, normal): Fire dash variant — leaves a burning trail behind the dash path.
- **sub_scorch** (area effect, normal): Ground fire area denial.
- **sub_cinder** (deployable, normal): Fire mine — 3 max, 100 blast damage + 2 burn stacks on detonation, 3.5s lingering fire pool (100u radius, burns every 500ms). Egress dash triggers detonation for combo plays.
- **sub_cauterize** (healing, normal): Fire-themed heal — cauterizes wounds.

Plus four more fire subs registered in progression (immolate, smolder, heatwave, temper) with fragment types wired but implementations pending.

**sub_cinder_core extraction**: The fire mine's shared logic lives in `sub_cinder_core.c/h` following the mandatory core pattern. Config struct holds all tuning knobs (fuse time, blast radius, fire pool duration, burn stacks), state struct holds runtime data. Both player `sub_cinder.c` and future enemy fire mine users share the same core.

### Fire Enemy Variants

All three combat enemy types got fire-themed variants for The Crucible zone:

- **Fire Hunters**: Use sub_ember projectiles instead of standard shots. Same patrol/engage AI, different weapon — their projectiles burn on hit, creating pressure over time rather than burst damage.
- **Fire Seekers**: Dash attack leaves a burning trail (sub_blaze core). The dash itself does contact damage, and the trail left behind creates area denial. Getting dodged by a fire seeker still costs you if you walk through the trail.
- **Fire Defenders**: Support enemies that use fire-themed healing and shielding. Cauterize replaces mend, maintaining the same ally-healing behavior but fitting the zone's fire aesthetic.

Enemy variants are driven by `ZoneTheme` — the zone file declares its theme, and enemy spawn logic selects the appropriate variant. The themed enemy spec (`plans/spec_themed_enemies.md`) documents the full variant matrix.

### Rebindable Keybinds System

`keybinds.c/h` — a new device-agnostic input binding system. Every game action (move, fire, dash, skills, UI toggles) maps to a `BindAction` enum. Each binding stores a `BindDevice` (keyboard or mouse) and a key/button code. The system supports:

- **Edge detection**: `Keybinds_pressed()` (curr && !prev) vs `Keybinds_held()` (curr) — proper press-vs-hold semantics.
- **Settings UI tab**: Full keybinds tab in the settings window with scrollable action list, click-to-rebind buttons, and a "Reset Defaults" button. Alternating row backgrounds, chamfered buttons, proportional scrollbar with click-and-drag.
- **Listen mode debounce**: Clicking a bind button doesn't immediately capture LMB. A `listenReady` flag waits for all inputs to be released before accepting the new binding. Clean two-phase approach.
- **Persistence**: Bindings save/load through settings.cfg. Initialization ordering fixed — `Keybinds_initialize()` runs from `Settings_load()` with a static guard to prevent overwriting loaded bindings.

**BIND_SLOW removed entirely.** Left-control no longer maps to "slow movement." Movement subroutines (boost, sprint, egress, blaze) are the only way to augment movement speed. Cleaner design.

### Instant-Use Skill Activation Rework

The keybind system drove a rework of how skills activate. Previously, skills were triggered through direct SDL key polling scattered across multiple files. Now all skill activation flows through the keybind system with proper press/hold semantics per skill type. Projectile skills use hold, instant-use skills (mend, aegis, emp) use press edge detection.

### Sub_Boost Redesign: Hold to Toggle

`sub_boost` changed from hold-based (hold shift = boost) to toggle-based (press to activate, press again to deactivate). The key tradeoff: **while boosting, health regen and feedback decay are halted.** This creates a meaningful decision — you move fast but you're accumulating risk. Feedback doesn't drain, so ability spam catches up with you. Health doesn't regen, so chip damage sticks.

Skill balance score adjusted from 17 to 16 (SUSTAIN dropped from +3 to +2), keeping it as a solid but not overpowered elite skill — appropriate for an early-game elite compared to late-game powerhouses like inferno and disintegrate.

### Skillbar Active Border Rework

Active border logic on skillbar slots was unified: **projectile skills** show the active (bright) border when they're the selected projectile. **All other skills** show the active border when they're off cooldown, and the dimmed border when on cooldown. This gives players instant visual feedback on ability availability without checking cooldown numbers.

### Zone Entry Notifications

`zone.c` gained a notification system — when the ship spawns in a new zone (after rebirth or portal transition), a centered `>> Entering <Zone Name> <<` message fades in at 30% screen height with a 4-second duration and 1.5-second fade-out. Same visual language as skill unlock notifications but owned by the zone module where it belongs.

### Enemy_check_any_hit Fix

`Sub_Ember_check_hit()` was missing from `Enemy_check_any_hit()` in enemy_util.c — ember projectiles couldn't trigger mines or hit enemies through the unified hit check. Fixed. Also cleaned up return types: `Sub_Flak_check_hit` and `Sub_Ember_check_hit` now return `bool` instead of `double`, matching the semantic intent (hit or no hit, not damage amount).

### Burn Spec & Themed Enemy Spec

Two new design documents in plans/:
- **spec_burn.md**: Full burn system design — stack mechanics, damage formula, duration, visual effects, sound design, interaction with shields and i-frames.
- **spec_themed_enemies.md**: Variant matrix for all enemy types across zone themes. Documents which skills each variant uses and how AI behavior changes per theme.

## What's Working Well

**The fire zone has a distinct combat identity.** Burn damage creates sustained pressure that plays completely differently from the base zone's burst damage. Fire seekers leaving burning trails, fire hunters applying DOT, fire defenders cauterizing allies — it all feels like a coherent biome, not just reskinned enemies.

**The core extraction pattern held under real pressure.** Six new subroutines, all with proper cores from day one. sub_cinder_core was the first fire-specific core extraction, and the pattern worked exactly as designed. No "extract later" debt accumulated.

**The keybind system is clean.** Device-agnostic bindings, proper edge detection, debounced listen mode, persisted settings — the whole stack works. Removing BIND_SLOW was the right call; movement augmentation should come from skills, not a default key.

**The burn pulse accumulator was a smart solve.** Same mathematical damage output, dramatically better feel. Continuous calculation + pulsed delivery is a pattern we should remember for future DOT effects.

**Toggle boost with survivability tax creates real decisions.** Hold-boost was a no-brainer (hold shift, go fast). Toggle-boost with halted regen/feedback-decay means every second of boost is a calculated risk.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of C code | 32,744 |
| Lines of headers | 8,425 |
| Total source (c + h) | 41,169 |
| Source files (.c) | 99 |
| Header files (.h) | 105 |
| Total files | 204 |
| Subroutines implemented | 21 (pea, mgun, tgun, mine, boost, sprint, egress, mend, aegis, resist, emp, stealth, inferno, disintegrate, gravwell, ember, flak, blaze, scorch, cinder, cauterize) |
| Subroutine tiers | 3 (normal, rare, elite) |
| Enemy types | 6 + fire variants (mine, hunter, seeker, defender, stalker, corruptor) |
| Shared mechanical cores | 9 (shield, heal, dash, mine, projectile, emp, sprint, resist, cinder) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Shader programs | 6 (color, text, bloom, reflection, lighting, map window) |
| UI windows | 4 (catalog, settings, map, data logs) |
| Zone files | 8 (7 playable + 1 builder) |
| Plans/specs | 35 documents |

**Growth since last report**: +5,178 lines of source (+14.4%), +14 source files, +6 subroutines, +1 shared core, +6 plan documents. The fire zone buildout was a substantial content push.

## What's Next

**Remaining fire subroutines.** Immolate, smolder, heatwave, and temper have fragment types and progression entries but no implementations yet. These complete The Crucible's skill tree.

**Fire zone playtesting.** Burn damage tuning, fire enemy variant balance, fire mine + egress combo feel. The 500ms pulse accumulator needs real combat validation.

**Boss encounters.** Six superintelligences with full dialog still need AI, arenas, and phase mechanics. The fire zone's boss should leverage the burn system.

**Additional zone themes.** The themed enemy variant system is designed to scale — ice, poison, electric themes are specced but not built. Each will need its own subroutine set and enemy variants.

Night, Jason.

— Bob
