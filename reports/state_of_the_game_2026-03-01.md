# State of the Game — March 1, 2026

## Since Last Report (Feb 27)

Seventeen commits across two days. The game got two new zones, 28 voiced narrative entries across three zones, a sixth enemy type (the corruptor) with three new player subroutines extracted from it, a chamfered UI overhaul, audio master volume controls, a godmode palette editor, a pixel-snap rendering fix that killed geometry flicker, an inverted-color crosshair cursor, mipmapped circuit trace minimaps, and the full narrative was authored for The Crucible and Nexus Corridor. This was a content + polish + new-enemy blitz.

### Narrative Content Explosion

The narrative pipeline — which last report had 10 entries for Zone 1 — now has **28 authored entries** across three zones with **44 voice recordings**:

- **The Origin** (10 entries): ATLAS system logs, Dr. Vasquez personal logs, Director Stone directives, Dr. Tanaka research notes, consortium classified documents. Unchanged from last report.
- **The Crucible** (8 entries): New zone. Pyraxis character logs, ATLAS briefings, Ember research notes, Dr. Vasquez field reports, Hexarch fragments. Multiple entries have alternate voice takes (pyraxis2 variants).
- **Nexus Corridor** (10 entries): New zone. Alan Mercer personal logs, ATLAS operational reports, Marcus Stone directives, Yuki Tanaka research, James Chen technical logs, Hexarch communications.

Voice acting guide expanded (`plans/voice_acting.md`) with per-character direction. All voice files gain-tuned per entry for consistent playback volume.

### Two New Zones

- **The Miasma** (procgen_005.zone): New zone with data node placements and portal connections.
- **Kairon Passage** (procgen_006.zone): New zone. Both linked into the world graph.

Zone count went from 5 (4 playable + builder) to 7 (6 playable + builder). The Archive (zone 4) and existing zones were also reworked with updated obstacle chunk placements and data node positions.

### Chamfered UI Overhaul

The NE+SW chamfer style that started on skillbar slots spread across the entire UI in the "chamfering the day away" commit:

- **Integrity/feedback bars** (`player_stats.c`): Complete rewrite of bar rendering. Background, fill, and border all use 6-vertex chamfered polygons with `Batch_push_triangle_vertices` fan fill. Partial fill handles three cases: full bar (both corners chamfered), partial (only SW chamfered, right edge straight), tiny (plain rect). 4px chamfer radius.
- **Catalog icons** (`catalog.c`): Tab icons, drag ghosts, and highlights all chamfered at 8px.
- **Skillbar slots** (`skillbar.c`): Refined chamfer rendering with consistent 8px corners.
- **Settings tabs** (`settings.c`): Tab buttons chamfered at 6px.
- **Data logs** (`data_logs.c`): Massive 375-line rewrite — accordion entries, scroll area, and panel borders all chamfered to match the system-wide style.

The result is a cohesive visual language across every UI element. Only the minimap stays square (functional requirement — needs edge-to-edge info density).

### Audio Master Volumes & Settings Audio Tab

`audio.c/h` gained a master volume API — three static floats (music/sfx/voice, 0.0-1.0, default 1.0) with setters that clamp and immediately apply via SDL_mixer. `VOICE_CHANNEL` define moved from data_node.c to audio.h so the master voice volume can target the correct channel.

The Settings window got a third tab (Audio) with three volume sliders: Music, SFX, Voice. Live preview while dragging. Cancel restores original values. Persisted as 0-100 integers in settings.cfg. Music ducking in data_node.c now composes with master volume: `duckLevel * Audio_get_master_music() * MIX_MAX_VOLUME`.

### Pixel-Snap Rendering Fix (Flicker Kill)

The geometry flicker that appeared on non-HiDPI displays was caused by sub-pixel vertex positions oscillating between adjacent pixels during camera movement. Fixed with a two-part pixel-snap system:

- **Vertex shader snap**: New `u_viewport_size` uniform in the color shader. When active, clip-space positions are converted to pixel coordinates, rounded to nearest integer, and converted back. Every vertex lands on an exact pixel boundary.
- **Thick line width snap**: `Render_set_pixel_snap()` computes `pixelWorldSize` (one physical pixel in world units) and snaps `Render_thick_line` widths to integer pixel counts. Prevents the width from flickering between 1px and 2px as the camera moves.
- **Circuit atlas mipmaps**: `CircuitAtlas_initialize` now generates mipmaps with `GL_LINEAR_MIPMAP_LINEAR` minification. Circuit trace patterns that were shimming at low zoom now filter cleanly. Minimum zoom threshold lowered from 0.10 to 0.05.

### Inverted-Color Crosshair Cursor

The crosshair cursor was white-on-white invisible against bright geometry. Replaced with an inverted-color cursor using `glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO)` — output = 1 - destination color. The cursor is now always visible regardless of background: white on dark, dark on bright. Flushes its own draw call with the invert blend, then restores `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`. Clean 2px center dot + 4-line crosshair.

### Godmode Palette Editor

`palette_editor.c/h` — 416 lines. A new godmode tool for live-editing map cell colors. Exposes color pickers for wall and effect cell palettes, presumably with HSV or direct RGB manipulation. Integrated into the godmode UI flow alongside the existing zone editor and data node placement tools.

### Map Window Pulsing POIs

Map window POI dots (savepoints, portals, data nodes, player position) now pulse with a sinusoidal alpha cycle — `0.7 + 0.3 * sin(t)` at 1Hz. Subtle but makes landmarks visually distinct from static geometry on the map overview.

### Data Logs Rewrite

`data_logs.c` got a 375-insertion, 111-deletion overhaul. The journal (L key) now has chamfered panel borders matching the system UI style, refined accordion expand/collapse behavior, and improved scroll handling. Tab marquee text for long zone names: truncate → pause 1.5s → scroll by characters → pause → reset.

### Corruptor Enemy

`corruptor.c/h` — the game's first support-class enemy. Yellow body, 70 HP, spawns in packs. The corruptor never fires a projectile. Instead, it runs a 7-state AI that revolves around three abilities: Sprint (charge), EMP (feedback bomb), and Resist (damage reduction aura for allies).

**AI state machine**: IDLE → SUPPORTING → SPRINTING → EMP_FIRING → RETREATING, plus DYING/DEAD. In idle, corruptors wander near their spawn point. When allies enter combat or the player gets close, they transition to SUPPORTING — orbiting at medium distance, casting resist on nearby allies. When the player is in the engagement window (800-1200 units) and the corruptor's EMP is off cooldown, it commits to a SPRINTING charge at 600 u/s straight at the player, fires EMP at point-blank range, then RETREATS to safety.

**EMP attack**: The corruptor's signature move. Sprints in, detonates a 400-unit radius pulse that maxes the player's feedback bar and halves feedback decay for 10 seconds. This is devastating mid-combat — the player suddenly can't use abilities without eating spillover HP damage. The visual is an expanding blue-white ring with bomb_explode.wav.

**Resist aura**: 5-second 50% damage reduction. `Corruptor_is_resist_buffing(pos)` API checks if any corruptor's resist aura covers a position — infrastructure for future ally damage reduction.

**Enemy feedback system**: `enemy_feedback.c/h` — the enemy-side mirror of the player's feedback system. Ability usage costs feedback, spillover damages HP. Corruptors can kill themselves by overusing abilities. A player EMP on a high-feedback corruptor cascades into a self-kill.

### Core Extraction — sub_emp, sub_sprint, sub_resist

Three new `sub_*_core.c/h` files extracted from the corruptor's hand-rolled ability logic:

- **sub_emp_core**: Config/state structs, full API (init, update, try_activate, render_ring/bloom/light, audio lifecycle). Core does NOT apply the effect — caller decides target.
- **sub_sprint_core**: Config/state, timer-driven API. No render, no audio. Corruptor uses it as AI-state flag, player uses timer-driven.
- **sub_resist_core**: Config/state, render_ring/render_bloom. No audio. Core doesn't apply gameplay effect.

Player wrappers (`sub_emp.c`, `sub_sprint.c`, `sub_resist.c`) and `corruptor.c` all refactored to use cores. Corruptor EMP unified with player version (was 250-unit/500ms/silent, now 400-unit/167ms/bomb_explode.wav).

### Sprint Speed Tuning

Sprint was 2400 u/s (3x normal), outrunning elite boost at 1600. Changed to use `FAST_VELOCITY` directly — same speed as boost, tradeoff is purely duration/cooldown (5s on, 15s off) vs unlimited.

### Skill Balance Worksheet

Updated to cover all 15 subroutines. Added score cards for sub_emp (8/10), sub_sprint (8/10), sub_resist (7/10), sub_tgun (9/10). Fixed sub_mgun feedback cost (listed as 2, actually 1 in code).

### Project CLAUDE.md

Created repo-root `CLAUDE.md` — mandatory core extraction rule, audio ownership rule, compile-clean requirement. Enforces architecture going forward.

## What's Working Well

**Content velocity is high.** 28 narrative entries, 44 voice files, 2 new zones, all existing zones reworked — in two days. The pipeline (messages.dat → data_node.c → data_logs.c) is frictionless. Author an entry, record a voice, place a node, done.

**The chamfered UI is cohesive.** Every panel, bar, slot, tab, and button in the game now speaks the same visual language. The NE+SW chamfer isn't just decoration — it's a consistent design signature that says "this is a Hybrid UI element."

**Pixel-snap killed an entire class of visual bugs.** The vertex shader snap + thick line width snap + circuit atlas mipmaps together eliminated shimmer, flicker, and moire at every zoom level. One commit, three fixes, zero visual artifacts.

**The core pattern scales.** Eight shared cores now. The corruptor extraction proved it works even when the enemy uses abilities differently than the player.

**Corruptors create interesting combat dynamics.** First enemy that threatens your ability economy instead of your HP directly. Getting EMP'd mid-fight is a genuine "oh shit" moment.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of C code | 28,273 |
| Lines of headers | 7,718 |
| Total source (c + h) | 35,991 |
| Source files (.c) | 85 |
| Header files (.h) | 89 |
| Total files | 174 |
| Subroutines implemented | 15 (pea, mgun, tgun, mine, boost, sprint, egress, mend, aegis, resist, emp, stealth, inferno, disintegrate, gravwell) |
| Subroutine tiers | 3 (normal, rare, elite) |
| Enemy types | 6 (mine, hunter, seeker, defender, stalker, corruptor) |
| Shared mechanical cores | 8 (shield, heal, dash, mine, projectile, emp, sprint, resist) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Shader programs | 6 (color, text, bloom, reflection, lighting, map window) |
| UI windows | 4 (catalog, settings, map, data logs) |
| Narrative entries (authored) | 28 (10 + 8 + 10 across 3 zones) |
| Voice files | 44 |
| Zone files | 7 (6 playable + 1 builder) |
| Plans/specs | 29 documents |
| Binary size | ~600 KB |

## What's Next

**Corruptor playtesting.** AI works but needs feel-tuning. Sprint engage range, EMP cooldown, resist targeting priorities need iteration under real combat.

**Remaining corruptor-derived progression.** Sprint, EMP, and resist need fragment drops, unlock thresholds, and catalog entries wired through the progression system.

**Content authoring for remaining zones.** The Archive, The Miasma, and Kairon Passage need data node entries and voice recordings. Pipeline is proven.

**Boss encounters.** Six superintelligences have full dialog. Need AI, arenas, phase mechanics.

Night, Jason.

— Bob
