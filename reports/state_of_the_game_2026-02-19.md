# State of the Game — February 19, 2026

## Tonight's Session

Two massive efforts today, back to back.

### Subroutine Unification

This was the big one. We extracted shared mechanical cores from every duplicated player/enemy ability in the codebase. Before today, the player's aegis shield and the defender's aegis shield were two completely separate implementations doing the same thing with different numbers. Same story for heals, dashes, mines, and projectiles. That's five systems that existed in duplicate — player version and enemy version, each with their own state machines, their own rendering, their own timing logic.

Now there are five shared cores:

| Core | Shared Between | What Changed |
|------|---------------|-------------|
| `sub_shield_core` | sub_aegis + defender aegis | One state machine, config struct controls duration/cooldown/ring size |
| `sub_heal_core` | sub_mend + defender heal | One cooldown/beam system, caller applies the actual HP |
| `sub_dash_core` | sub_egress + seeker dash | One dash state machine, config controls speed/cooldown/damage |
| `sub_mine_core` | sub_mine + enemy mines | One fuse/explosion system, config controls timing/respawn |
| `sub_projectile_core` | sub_pea, sub_mgun + hunter shots | One projectile pool, config controls velocity/damage/color/cooldown |

Each core is a pure state machine — no knowledge of player input, skillbar, audio, or progression. The caller (player sub wrapper or enemy AI) owns all of that. Audio follows the strict ownership rule: skill sounds belong to skill cores, entity sounds are damage/death/respawn only.

This wasn't just cleanup. This is *architectural*. Every future enemy that shields, heals, dashes, mines, or shoots gets those mechanics for free. Write the AI wrapper, pass a config struct, done. When stalkers land and need a dash-strike, it's `sub_dash_core` with stalker config values. When swarmers need projectiles, it's `sub_projectile_core` with swarmer tuning. The mechanical layer is solved — forever.

Audio ownership got fixed as part of this too. Before unification, skill sounds were scattered — loaded and played by both entity files (hunter.c, defender.c, ship.c) AND player sub files. The same sound might be loaded in three places. Now the rule is strict: **skill sounds belong to skill cores**. Each core has `*_initialize_audio()` / `*_cleanup_audio()`, and the core owns its sounds completely. Entity files only play entity sounds (damage taken, death, respawn). No more shield activation sounds in defender.c, no more projectile fire sounds in hunter.c. Clean ownership, no duplication, no confusion about where a sound comes from.

We also centralized damage routing (`Enemy_check_player_damage()`) and fragment drops (`Enemy_drop_fragments()`) in `enemy_util.c`. Every enemy type now calls the same damage check that tests all player weapons and applies stealth ambush multipliers centrally. No more per-enemy-type weapon checking code.

### Settings Window & Render Pipeline Toggles

Then we built a full settings system. Seven toggles — multisampling, antialiasing, fullscreen, pixel snapping, bloom, lighting, reflections — in a modal window (I key) with the same visual style as the subroutine catalog. Pending-value pattern: edit freely, OK applies + saves to `settings.cfg`, Cancel/ESC discards. Settings persist across sessions.

The interesting engineering was in the render pipeline toggles. Bloom, lighting, and reflections each gate different parts of the 7-pass render pipeline. The stencil buffer situation was the real puzzle: reflections only draw on solid cells (stencil == 2), but lighting draws on ALL cells (stencil >= 1), and both share the same stencil write pass. Turn off reflections? The stencil write still has to happen for lighting. Turn off both? Skip everything. That conditional pipeline management is where bugs love to hide, and it's clean.

### Other Fixes
- **Click bleed-through**: Settings OK button was firing weapons underneath. Fixed with a `mouseConsumedUntilRelease` sticky flag that blocks mouse input until physical release after any UI panel closes. Cross-frame problem, cross-frame solution.
- **Fullscreen toggle broke instanced rendering**: ParticleInstance owns its own GL resources with lazy init, but nobody was calling cleanup when the GL context got destroyed. Inferno, disintegrate, and gravwell all rendered garbage after fullscreen toggle. One line fix — `ParticleInstance_cleanup()` in `Graphics_cleanup()`.
- **Pixel snapping is now optional**: The `floor(x + 0.5)` camera snapping we added early on to kill subpixel jitter is now a settings toggle. Some people might prefer smooth camera.
- **Render pipeline refactor**: Eliminated an extra entity iteration per frame — bloom source rendering now happens inline instead of requiring a second pass over all entities.

24,580 lines. The game runs, the game saves your preferences, the mechanical cores are unified, and you can turn off bloom if your machine is struggling. That's a real product.

## What's Working Well

**The subroutine unification we did today is the single most impactful refactor in the project's history.** Five duplicated systems collapsed into five shared cores. The codebase went from "player shield and enemy shield are two separate things" to "shield is a solved mechanical problem, just pass different config." That's not incremental improvement — that's a phase change in how fast we can add content. Every future enemy that uses any existing mechanic gets it for free. The mechanical layer is done. From here, new enemies are AI wrappers + config structs + art.

**The render pipeline is genuinely sophisticated for a 2D game.** Four bloom FBOs, stencil-based cloud reflections with parallax, weapon lighting that casts colored light from projectiles onto terrain, GPU instanced particles for three different subroutines. And it all composes — you can have inferno blobs casting orange light on walls while the background clouds reflect off solid blocks while disintegrate's purple bloom halo overlays everything. Seven render passes per frame, all correctly ordered, all with proper state management. That pipeline is doing work that most indie 2D games never attempt.

**The settings system's architecture is clean.** Pending-value pattern means you can toggle stuff freely without committing, OK applies and saves, Cancel discards. `Graphics_recreate()` only fires if multisampling/antialiasing/fullscreen actually changed — no unnecessary GL context teardown for a pixel snapping toggle. Load happens before engine init so the window opens in the right mode. Simple, correct, no edge cases.

## The Numbers

| Metric | Value |
|--------|-------|
| Lines of code | 24,580 |
| Source files (.c + .h) | ~110 |
| Subroutines implemented | 11 (pea, mgun, mine, boost, egress, mend, aegis, stealth, inferno, disintegrate, gravwell) |
| Enemy types | 4 (mine, hunter, seeker, defender) |
| Bloom FBO instances | 4 (foreground, background, disintegrate, weapon lighting) |
| Render passes per frame | 7 (bg bloom, world, reflection, weapon lighting, fg bloom, disint bloom, UI) |
| Settings toggles | 7 (ms, aa, fullscreen, pixel snap, bloom, lighting, reflections) |
| Shared mechanical cores | 5 (shield, heal, dash, mine, projectile) |
| Zone files | 2 (zone_001, zone_002) |
| Binary size | ~380K |

## What's Next

The foundation is solid. The combat is deep. The settings exist. The pipeline is mature. What's missing is *world*.

Procedural generation is still the highest-leverage remaining feature. One hand-authored test zone demonstrates the systems. Procgen makes them sing. The spec is written, Phase 1 (godmode tools) is done. The noise + influence field architecture is designed for exactly the kind of organic, unpredictable world that makes exploration genuine rather than memorizable.

But honestly? Tonight felt like a turning point. Not because settings windows are exciting, but because the *infrastructure concerns* are getting handled. When you're fixing click bleed-through edge cases and adding persistent user preferences, you're not in prototype mode anymore. You're in "this is going to be a thing people play" mode.

The game loop is there. The depth is there. The visual identity is there — neon geometry over ethereal purple clouds, reflected in polished walls, lit by your own weapons. Now it's about filling that world with content and letting the build-craft system do what it was designed to do: make the player think.

Sleep well, Jason. Tomorrow we build worlds.

— Bob
