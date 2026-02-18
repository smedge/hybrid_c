# State of the Game — February 18, 2026

## The Big Picture

Jason, this thing is *real*. 15 years of vision and it's not a prototype anymore — it's a game. 23K lines of C99, 253 commits, 10 subroutines, 4 enemy types, a full progression system, a level editor, zone transitions, a bloom pipeline, and a procedural generation spec that's going to blow the world open. The core loop — fight, collect, unlock, equip, fight harder — is in and it works. The skill catalog, the fragment system, the type-exclusivity loadout puzzle — that's the skeleton of the metroidvania build-craft vision, and it's standing upright.

What impresses me most is that the *hard* design decisions are already made and validated. The feedback/integrity dual-resource system is elegant — it creates real tension without being punitive. The "enemies are gates, subroutines are keys" philosophy is sound and already demonstrable with the current enemy set. Defenders healing hunters while you're trying to burst them down, seekers forcing you to run sub_egress, the feedback gate on stealth — these compositions already create the "I need a different build" moments that are the entire point.

You're past the "will this work?" phase. You're in the "how far can we push it?" phase.

## Code Quality

Honestly? For a C99 codebase this size with no engine underneath it, it's remarkably clean.

**What's good:**
- The `Render_*` abstraction layer is the smartest decision in the codebase. Every entity file is GL-free. When Metal/Direct3D time comes, you're rewriting 4 files, not 54. That's real engineering foresight.
- Static module pattern (no malloc, static arrays, bounds-checked pools) keeps everything predictable. No hidden allocations, no leak hunting, no fragmentation. For a game that needs to run at 60fps without hiccups, this is the right call.
- Each enemy/subroutine is self-contained. Adding a new one is "copy the pattern, change the numbers." The enemy registry callback system means new enemies get defender support for free. That's going to pay dividends when you're adding stalkers, swarmers, and corruptors.
- The bloom system is well-factored — configurable instances with per-entity `render_bloom_source()` callbacks. Three bloom FBOs doing completely different jobs (neon halos, ethereal clouds, purple beam glow) through the same code path.
- And as of tonight — the particle instance renderer. Three subroutines that were each managing their own rendering (gravwell with custom GL, inferno and disintegrate through the batch renderer) now share one GPU pipeline. One draw call per system per pass. That's the kind of consolidation that keeps a codebase from rotting.

**What could be better:**
- The ECS is functional but showing its age. The static singleton component pattern works, but `Entity_collision_system()` is O(n^2) pairwise. With 2048 entity slots and the enemy counts you're planning, that's going to hurt. The spatial partitioning spec in the PRD is the right answer — it's just not built yet.
- Some files are doing double duty. `map.c` owns private grid data AND line-of-sight testing AND cell rendering. As procgen lands and map sizes balloon, that file is going to get heavy. Not urgent, but worth watching.
- The zone file format is simple and human-readable (good), but full-file rewrite on every edit is fine now and will stay fine for hand-authored content. Procgen zones at 1024x1024 might want a binary format eventually, but that's a bridge to cross later.

## Performance

The 380K binary is hilarious. This thing is *tiny*. SDL2 + OpenGL + the entire game in less space than a single PNG texture in most modern games.

**Current state:**
- The batch renderer's 65K vertex cap was the old bottleneck — the geometry glow system (10 concentric shapes per object) was eating it alive. Bloom FBOs killed that problem dead. Smart move.
- Tonight's instanced renderer work is pure performance. Inferno was pushing up to 512 individual `Render_quad` calls per frame. Disintegrate up to 432. Gravwell up to 5,760. Now each is a single `glDrawArraysInstanced` call with a buffer upload. The GPU was never the bottleneck on these — but the CPU-side vertex submission overhead was real, and it's gone now.
- Static array pools everywhere (640 blobs, 256 blobs, 80+64 particles) mean zero allocation during gameplay. No GC pauses because there's no GC. No malloc because there's no malloc.

**Where it'll need attention:**
- Spatial partitioning is the big one. Right now every projectile checks every enemy every frame. With 128 hunters, 128 seekers, 64 defenders, plus swarmers splitting into 8-12 drones each? That's potentially thousands of collision pairs per frame. The grid bucket approach in the PRD spec is simple and correct — flat memory, cache-friendly, ~50 lines of C. It needs to happen before swarmers land.
- Procgen at 1024x1024 (100-unit cells = ~10.5 million square units) will stress the map renderer. The current approach renders all visible cells, but at low zoom levels that could be a lot of geometry. Frustum culling or chunk-based visibility will probably be needed.
- Audio is loading samples per-entity-type with `if (!sample)` guards, which works. But with 10+ enemy types x themed variants, you'll want to think about a central audio manager eventually.

## Future Direction — My Take

The roadmap in the PRD is ambitious but sequenced correctly. Here's what I think matters most, in priority order:

**1. Procedural Generation (the multiplier)**
This is the single highest-leverage feature remaining. Right now you have one hand-authored test zone. Procgen turns that into infinite content. The spec is solid — noise + influence fields, hotspot-carried terrain character, connectivity guarantees, chunk templates for set pieces. Phase 1 (godmode tools) is done. Phases 2-6 will transform this from "a game with a cool combat system" into "a game with a world."

**2. Three more enemy types (stalkers, swarmers, corruptors)**
The enemy composition gating table in the PRD is the game's difficulty design document. Right now you can demonstrate it with hunters + defenders. But the real magic — "no single loadout handles all of these" — needs at least 5-6 enemy types creating cross-pressure. Swarmers especially, because they're the gate that creates demand for AoE subs and unlock the minion master archetype.

**3. More subroutines (the depth)**
10 subs is enough to prove the system. 20 starts making build-craft real. 30+ is where "I need a different loadout for this zone" becomes genuinely strategic. The balance framework (point-budget scoring) is already in place. The catalog UI handles it. The progression system handles it. The infrastructure is ready — it's just content now.

**4. Boss encounters (the milestones)**
Bosses are what make metroidvania progression *feel* like progression. Clearing a boss is permanent, visible, meaningful. They're also the ultimate build-check — if your loadout can't handle the boss, you need to rethink. No boss design exists yet, but the combat systems are rich enough to support interesting boss mechanics.

**5. Spatial partitioning (the enabler)**
This is infrastructure, not content, but it unblocks everything above. You can't have swarmer drone swarms without it. It's ~50 lines of C and it removes the ceiling on entity count. Do it before or alongside swarmers.

## The Vision Check

The thing that makes this project special isn't any one feature — it's the *philosophy*. No colored doors. No keys. The gate is a room full of enemies that counter your build. The key is the right combination of subroutines and the knowledge of when to use them. That's a genuinely original take on metroidvania progression, and the systems to support it are already working.

The 50+ subroutine endgame vision with build archetypes (minion master, stealth assassin, control/disruption, AoE specialist, tank/sustain) — that's where this game becomes something nobody's played before. Guild Wars skill-deck building meets bullet hell combat meets metroidvania exploration. Each piece exists in other games. The combination doesn't.

You're building something worth building, man. Let's keep going.

— Bob
