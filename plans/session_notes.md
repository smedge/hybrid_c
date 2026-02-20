# Session Notes for State of the Game Report

## Stalker Enemy (5th enemy type) — Implemented
- Stealth assassin: approaches near-invisible, dashes through player, fires parting shot while retreating, re-stealths
- 40 HP glass cannon, purple crescent shape
- AI state machine: IDLE → STALKING → WINDING_UP (300ms decloak tell) → DASHING → RETREATING (parting shot 500ms in) → re-stealth after 3s
- Uses unified cores: sub_dash_core (egress config) for dash, sub_projectile_core (pea config) for parting shot
- Stealth alpha: 0.08 idle, ramps to 1.0 during windup, fades back during retreat
- Drops stealth + egress fragments (50/50)
- SUB_ID_STEALTH now requires 5 stalker fragments (was auto-unlock)
- FRAG_TYPE_STALKER added (dark purple 0.5, 0.0, 0.8)
- Full bloom, light source, and enemy registry integration
- 128 pool size, 32 shared projectile pool

## Bug Fixes
- **Solid collision force_kill**: Seeker and stalker dash collisions set `solid=true` which Ship_resolve treated as wall crush → instant death. Removed solid flag from both enemy collide functions.
- **Ambush shield-pierce centralized**: Moved from individual enemy damage checks to `Defender_is_protecting(pos, ambush)` — aegis owns the shield-pierce mechanic, not individual callers.
- **Dash direction bug**: SubDash cooldown was only ticked during DASHING state, freezing during retreat. Second attack used stale direction. Fixed by ticking cooldown every frame.

## Audio System Overhaul
- **Root cause of zone 2 audio bug**: Enemy types (hunter, seeker, stalker, defender, mine) were calling core audio init/cleanup, creating ownership conflicts during zone transitions. Defender_cleanup would run SubHeal/SubShield cleanup_audio during Zone_unload, interfering with player skill audio.
- **Fix — enforced audio ownership rule**: Removed all core audio init/cleanup calls from enemy types. Only player skills (sub_pea, sub_mend, sub_aegis, sub_egress, sub_mine) manage core audio lifecycle through Ship_initialize/Ship_cleanup. Enemies use cores for gameplay but never touch audio lifecycle.
- **Removed dead refcounting code**: Refcount pattern added during debugging was unnecessary once enemy types stopped managing core audio. Stripped from all four cores (sub_heal_core, sub_shield_core, sub_projectile_core, sub_dash_core).
- **Increased SDL_mixer channels**: Added `Mix_AllocateChannels(32)` in Audio_initialize (was default 8 — too low for combat-heavy zones).
- **Channel exhaustion logging**: Audio_play_sample now logs a warning if Mix_PlayChannel returns -1 (no free channel).

## Procedural Generation Phase 2 — Implemented
- Zone-defined procgen: zones can now declare `procgen 1` with noise parameters (octaves, frequency, lacunarity, persistence, wall_threshold) to generate terrain from noise at load time.
- Circuit vein system: second noise layer determines where circuit patterns appear within walls, creating organic veins/clusters.
- Hand-placed cells preserved: `cell_hand_placed` grid prevents procgen from overwriting manually placed cells.
- Seed system: master seed + per-zone seed derivation from zone filepath for deterministic, reproducible generation.

## Pre-Rendered Circuit Pattern Atlas — Implemented
- **Problem**: `render_circuit_pattern()` regenerated every visible circuit cell's traces per frame — seeded RNG, occupancy grid, thick_line triangles, filled_circle endpoints. Massive CPU cost with hundreds of circuit cells in procgen zones.
- **Solution**: `circuit_atlas.c/h` — pre-bake 20 base circuit pattern tiles into a 2560×2048 GL_RED texture atlas at init, draw textured quads at render time using the text shader.
- **6 connectivity classes** (which edges have solid neighbors): island, 1-edge, adjacent pair, opposite pair, 3-edge, surrounded. 20 base tiles in 5×4 grid, 512×512 each.
- **Variety**: spatial hash per cell selects pattern + extra rotation (for symmetric classes) + mirror (when E/W edges match). Up to 32 variants for island, 24 for surrounded — the most common case in dense zones.
- **Rendering**: own VAO/VBO, reuses text shader (same vertex format), single draw call for all visible circuit cells. Chamfered cells get proper UV-mapped polygon vertices.
- **Pipeline position**: after Map_render flush, before MapReflect/MapLighting.
- ~100x reduction in per-cell triangle count for circuit rendering.

## Batch Renderer Auto-Flush — Implemented
- **Problem**: `BATCH_MAX_VERTICES` (65536) overflowed during rebirth/wide zoom with large procgen zones, silently dropping geometry.
- **Solution**: Auto-flush on overflow — when a push would exceed the buffer, the batch draws that primitive type using stored flush context matrices, resets count, and continues. No vertex limit anymore, just extra draw calls as needed.
- `Render_set_pass(proj, view)` stores flush context before each render pass. `Batch_flush` also stores context on every explicit flush.
- **Grid render order fix**: Grid now renders and flushes separately BEFORE Map_render, so auto-flushed map fills always layer on top of grid lines.
- **UI pass fix**: `Render_set_pass` called before Hud_render so minimap auto-flushes use the correct UI projection (not stale world projection).
- `BATCH_MAX_VERTICES` stays at 65536 (~5.5MB static) instead of 262144 (~22MB). Console-friendly.

## Procgen Tuning
- **Circuit vein threshold**: Lowered from 0.5 to 0.30 — more circuit tiles in procgen zones (~35-40% vs 15-20%).
- **Data fortress**: Center 64×64 cells (4×4 major grid blocks) of procgen zones excluded from terrain generation. Reserved for portal and savepoint placement.

## Portal Persistence Fix
- **Bug**: Unconnected portals (no destination set) disappeared on zone reload. `sscanf` required 5 fields but empty destination strings produced fewer tokens.
- **Fix**: Sentinel value `"-"` written for empty dest_zone/dest_portal_id. Load converts `"-"` back to empty string. Unfinished portals now survive zone transitions.

## Zone Music Shuffle
- Zone music playlists now shuffled (Fisher-Yates) on zone entry and loop indefinitely.

## Rebirth View Snap Fix
- **Bug**: On entering a zone for rebirth from save, the camera showed zone center for one frame before snapping to the saved checkpoint position.
- **Fix**: Set `View_set_position(loadSpawnPos)` in `Mode_Gameplay_initialize_from_save()` before first render, so the view starts at the correct position immediately.

## Map Window (M Key) — Implemented
- Full-zone overview window toggled by M key, styled like settings/catalog (dark panel + border, title floating above top edge).
- **CPU-side texture generation**: iterates all cells via `Map_get_cell(x,y)`, writes brightened pixel colors (8x multiplier matching minimap) into an RGBA8 texture. One pixel per grid cell, GL_NEAREST filtering for crisp pixels.
- **Dedicated GLSL 330 shader**: minimal vertex+fragment for full-color textured quad rendering (text shader only samples GL_RED, can't do RGBA).
- **Landmark dots**: player (red), portals (white), savepoints (cyan) — matching minimap colors and rendered as batched quads on top of the texture.
- **Large grid divisions**: every 16 cells (matching in-game big grid), subtle lines at low alpha.
- **Window sizing**: 70% of shortest viewport dimension, square, centered. Texture regenerated on each open so it handles zone changes naturally.
- **Mutual exclusion**: opening map closes catalog/settings and vice versa. ESC or M to close.
- New files: `map_window.c/h`. Modified: `input.h/c`, `sdlapp.c`, `graphics.c`, `mode_gameplay.c`.
- Two linked procgen zones now explorable: default blue/cyan theme and a fire/red Tron theme.
