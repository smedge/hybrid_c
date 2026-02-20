# State of the Game — February 20, 2026

## Tonight's Session

Big session. Five major systems landed: a new enemy type, procedural generation phase 2, a pre-rendered circuit pattern atlas, batch renderer auto-flush, and a full-zone map window. Plus an audio system overhaul, a pile of bug fixes, and two linked procgen zones you can warp between. Let's get into it.

### Stalker — 5th Enemy Type

The stalker is a stealth assassin. Near-invisible approach (alpha 0.08), 300ms decloak windup tell, dash through the player, fire a parting shot while retreating, then re-stealth after 3 seconds and do it again. 40 HP glass cannon, purple crescent shape, drops stealth + egress fragments.

The AI state machine is IDLE → STALKING → WINDING_UP → DASHING → RETREATING → loop. It uses the unified cores we built last session — `sub_dash_core` with egress-style config for the dash, `sub_projectile_core` with pea config for the parting shot. This is exactly the payoff from the unification work: the stalker's mechanical layer was free, we only had to write the AI behavior and the visual identity.

SUB_ID_STEALTH now requires 5 stalker fragments to unlock (was auto-unlock). FRAG_TYPE_STALKER added with dark purple color. Full bloom, light source, and enemy registry integration. 128 pool size with 32 shared projectiles.

Three bugs came along for the ride:
- **Solid collision force_kill**: Seeker and stalker dash collisions were setting `solid=true` on their collision result, which Ship_resolve interpreted as wall crush → instant death. Getting dashed through by either enemy was an instant kill regardless of HP. Removed the solid flag from both.
- **Ambush shield-pierce**: Was being checked individually by each enemy type. Centralized into `Defender_is_protecting(pos, ambush)` so the aegis shield owns the pierce mechanic, not every caller.
- **Dash direction bug**: SubDash cooldown was only ticked during the DASHING state, so it froze during the retreat phase. Second attack reused the stale direction from the first dash. Fixed by ticking cooldown every frame regardless of state.

### Audio System Overhaul

Zone 2 had an audio bug that led us to a fundamental ownership problem. Enemy types (hunter, seeker, stalker, defender, mine) were calling core audio init/cleanup, creating ownership conflicts during zone transitions. When `Defender_cleanup` ran during `Zone_unload`, it would call `SubHeal_cleanup_audio` and `SubShield_cleanup_audio`, nuking sounds that the player's skill system was still using.

The fix was strict enforcement of the audio ownership rule: **enemy types never touch core audio lifecycle.** Only player skills (sub_pea, sub_mend, sub_aegis, sub_egress, sub_mine) manage core audio through Ship_initialize/Ship_cleanup. Enemies use cores for gameplay mechanics but their audio init/cleanup calls are gone. Refcount code added during debugging was stripped — unnecessary once the ownership boundary was clean.

Also bumped SDL_mixer from 8 to 32 channels (`Mix_AllocateChannels(32)`) — 8 was starving in combat-heavy zones. Added channel exhaustion logging so we'll know if 32 isn't enough.

### Procedural Generation Phase 2

Zones can now declare `procgen 1` with noise parameters — octaves, frequency, lacunarity, persistence, wall_threshold — and terrain generates from noise at load time. This is the system from the procgen spec finally running in-game.

Key design decisions:
- **Circuit vein system**: A second noise layer determines where circuit patterns appear within walls. Not random per-cell anymore — circuits cluster into organic veins and rivers through the terrain. Threshold tuned to ~35-40% coverage (down from the original 50% which was too dense).
- **Hand-placed cell preservation**: `cell_hand_placed` grid prevents procgen from overwriting manually placed cells. God mode edits survive regeneration.
- **Seed system**: Master seed + per-zone seed derived from zone filepath. Deterministic and reproducible — same seed, same world, every time.
- **Data fortress**: Center 64×64 cells (4×4 major grid blocks) excluded from terrain generation. Reserved for portal and savepoint placement so critical infrastructure isn't buried in walls.

Two procgen zones are now linked via portals: the default blue/cyan cybercity theme, and a fire/red theme that looks like Tron meets Bowser's Castle. The warp transition carries you between them with the full zoom-flash-arrive cinematic. The game has *geography* now.

### Pre-Rendered Circuit Pattern Atlas

This was a performance necessity. With procgen zones generating hundreds of circuit cells, the old approach — regenerating every visible circuit cell's traces per frame using seeded RNG, occupancy grids, thick_line triangles, and filled_circle endpoints — was crushing the CPU.

The solution: `circuit_atlas.c/h` pre-bakes 20 base circuit pattern tiles into a 2560×2048 GL_RED texture atlas at initialization. At render time, each visible circuit cell draws a single textured quad instead of generating dozens of triangles.

**6 connectivity classes** based on which edges have solid neighbors: island, 1-edge, adjacent pair, opposite pair, 3-edge, surrounded. 20 base tiles arranged in a 5×4 grid, 512×512 each. **Variety** comes from spatial hashing: per-cell pattern selection + extra rotation for symmetric classes + mirroring when E/W edges match. Up to 32 variants for island tiles, 24 for surrounded — the most common case in dense procgen zones.

The atlas has its own VAO/VBO and reuses the text shader (same vertex format — position + texcoord + color). Chamfered cells get proper UV-mapped polygon vertices. Pipeline position: after Map_render flush, before MapReflect/MapLighting.

~100x reduction in per-cell triangle count. The game went from struggling to render dense procgen zones to not caring.

### Batch Renderer Auto-Flush

The circuit atlas solved the per-cell cost, but we still had a vertex budget problem. `BATCH_MAX_VERTICES` (65536) would overflow during the rebirth zoom-out or at wide zoom levels with large procgen zones, silently dropping geometry — walls just vanishing at the edges of the screen.

The fix: auto-flush on overflow. When a vertex push would exceed the buffer, the batch immediately draws that primitive type using stored flush context matrices, resets the count, and continues. No hard vertex limit anymore, just extra draw calls as needed. `Render_set_pass(proj, view)` stores the flush context before each render pass so auto-flushes always use the correct projection.

Two ordering fixes came with this:
- **Grid render order**: Grid now renders and flushes separately BEFORE Map_render, so auto-flushed map fills always layer on top of grid lines (before, an auto-flush mid-map could draw map geometry under a grid that was already committed).
- **UI pass**: `Render_set_pass` called before Hud_render so minimap auto-flushes use the UI projection, not the stale world projection.

`BATCH_MAX_VERTICES` stays at 65536 (~5.5MB static) instead of cranking it to 262144 (~22MB). Console-friendly memory footprint, unlimited effective vertex count.

### Map Window (M Key)

Full-zone overview window, toggled with M. Shows the entire 1024×1024 zone as a texture — one pixel per grid cell, colors brightened 8x to match the minimap's visual pop. CPU-side texture generation: iterate all cells, read primary colors, write RGBA pixels, upload to GL. Regenerated on each open so it naturally handles zone changes.

Needed its own GLSL 330 shader — the text shader only samples GL_RED (font atlas), the color shader doesn't do textures. Minimal vertex+fragment: MVP projection + texture sample. Own VAO/VBO, own texture handle, self-contained lifecycle.

Landmarks render on top as batched color quads: player (red), portals (white), save stations (cyan) — same colors as the minimap. Large grid divisions (every 16 cells) drawn as subtle lines for orientation. Styled to match catalog and settings — dark panel, grey border, title floating above the top edge.

### Other Fixes & Polish
- **Portal persistence**: Unconnected portals (no destination set) disappeared on zone reload — `sscanf` needed 5 fields but empty destinations produced fewer tokens. Fixed with sentinel value `"-"` for empty fields.
- **Zone music shuffle**: Playlists now Fisher-Yates shuffled on zone entry and loop indefinitely.
- **Rebirth view snap**: Camera showed zone center for one frame before snapping to saved checkpoint. Fixed by setting view position before first render.
- **Circuit trace settings toggle**: Added to the graphics settings panel so players can disable traces if needed.

## What's Working Well

**The stalker validates the entire unification architecture.** We spent a full session extracting shared cores from every player/enemy ability, and the stalker was the first customer. Dash mechanics? `sub_dash_core` with a config struct. Projectile parting shot? `sub_projectile_core` with different tuning. The AI behavior and visual identity were the actual work — the combat mechanics were plug-and-play. That's exactly what unification was supposed to deliver, and it did.

**Procgen + circuit atlas + auto-flush = scalable world generation.** These three systems solve different layers of the same problem. Procgen creates the terrain. The circuit atlas makes it look good without per-frame cost. Auto-flush handles the vertex volume when you zoom out and see it all. Together they mean we can generate zones with hundreds of thousands of cells and render them at full visual quality without performance compromise. That's the foundation for a game with real exploration.

**The two-zone setup proves the world pipeline works end-to-end.** Zone authoring (procgen params + god mode hand-placement), portal linking, warp transitions, save/load across zones, zone-specific music playlists, and now a map window to navigate it all. Every piece of the "travel between places" stack is functional. Adding zone 3 is a content task, not an engineering task.

**The render pipeline keeps scaling gracefully.** This session added a new shader program (map window), a texture atlas system (circuit traces), and auto-flush capability to the batch renderer. Six shader programs now switch during a single frame — color, text, bloom, reflection, lighting, map window — with zero state corruption. The discipline of flush-before-switch has proven itself across every new system.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of code | 26,945 |
| Source files (.c + .h) | 140 |
| Subroutines implemented | 11 (pea, mgun, mine, boost, egress, mend, aegis, stealth, inferno, disintegrate, gravwell) |
| Enemy types | 5 (mine, hunter, seeker, defender, stalker) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Shader programs | 6 (color, text, bloom, reflection, lighting, map window) |
| UI windows | 3 (catalog, settings, map) |
| Render passes per frame | 7+ (bg bloom, grid, world, reflection, weapon lighting, fg bloom, disint bloom, UI) |
| Settings toggles | 8 (ms, aa, fullscreen, pixel snap, bloom, lighting, reflections, circuit traces) |
| Shared mechanical cores | 5 (shield, heal, dash, mine, projectile) |
| Zone files | 4 |
| Binary size | ~453K |

## What's Next

The procgen + portal pipeline is proven. The next leverage points:

**More zone themes.** The fire/red zone proves that different noise parameters + different cell type palettes create genuinely distinct worlds. Each new theme is a zone file with tuned procgen params, custom cell colors, and maybe biome-specific music. Low effort, high impact.

**Biome hazard cells.** The procgen spec defines effect cells — traversable cells with gameplay consequences. Fire DOT, ice friction, poison, energy drain. The three-tier noise threshold system (wall / effect / empty) is already in the spec. Effect cells in generic zones are atmospheric data traces; in themed zones they become biome hazards. The fire zone is begging for lava-glow cells that burn you.

**Influence field system.** Right now procgen terrain is uniform noise — same character everywhere. The spec calls for hotspot-driven influence fields where landmarks (dense fortresses, sparse wastelands, moderate transition zones) shape the terrain around them. This is what turns "noise with walls" into "a place with geography."

**More enemy variety.** The stalker proved that new enemies are fast now with unified cores. The mechanical layer is solved — new enemies are AI wrappers + config + art. Swarmers, turrets, bosses — whatever the design calls for, the infrastructure supports it.

But tonight was a turning point. Not one feature, but the convergence of five systems that all needed each other. Procgen needed the atlas for performance. The atlas needed auto-flush for vertex budget. The stalker needed unified cores. The map window needed procgen zones to have something worth mapping. And the portal system needed all of it to create a world worth traveling through.

Two zones, linked together, each with its own character. A map to find your way. An assassin stalking you through procedurally generated corridors. 27,000 lines of C that started as a vision 15 years ago.

Good shit indeed.

— Bob
