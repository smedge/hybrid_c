# Refactor, Quality & Performance Spec

**Date:** 2025-02-15
**Branch:** procgen_p1
**Scope:** Full engine audit — 48 source files across 5 subsystems

---

## Table of Contents

1. [Critical — Bugs That Can Crash or Corrupt Gameplay](#1-critical)
2. [Major — Correctness Issues & Significant Technical Debt](#2-major)
3. [Minor — Code Quality, Consistency, Polish](#3-minor)
4. [Overall Assessment](#4-overall-assessment)
5. [Recommended Fix Priority](#5-recommended-fix-priority)

---

## 1. Critical

### 1.1 Seeker dash deals damage every frame, not once per attack

**File:** `seeker.c:432-442`

During `SEEKER_DASHING` (150ms), the AABB overlap test runs every frame with no "already hit" flag. At 60fps that's ~9 hits of 80 damage = 720 damage from a single dash.

**Fix:** Add a `hitThisDash` bool, set on first contact, reset when entering `SEEKER_DASHING`.

---

### 1.2 Entity collision inner loop checks wrong index

**File:** `entity.c:121`

```c
for (int j = 0; j <= highestIndex; j++)
{
    if (entities[j].empty || entities[j].disabled || entities[j].collidable == 0 ||
        entities[i].placeable == 0)   // BUG: should be entities[j]
        continue;
```

The inner loop null-checks `entities[i].placeable` instead of `entities[j].placeable`. If entity `j` has a null placeable but entity `i` does not, the collision proceeds anyway and `j`'s collide function dereferences a null placeable. Copy-paste bug (`i` vs `j`).

**Fix:** Change `entities[i].placeable` to `entities[j].placeable` on line 121.

---

### 1.3 Ship doesn't check Entity_add_entity for NULL

**File:** `ship.c:78-91`

`Entity_add_entity` can return NULL when the pool is full. Ship passes the result directly to `Sub_Pea_initialize` etc. without checking. Enemy cleanup code does check, but ship doesn't.

**Fix:** Guard with `if (!liveEntity) return;` before passing to sub-system initializers.

---

### 1.4 Savepoint audio boost on NULL chunk

**File:** `savepoint.c:84-87`

```c
Audio_load_sample(&chargeLoopSample, "resources/sounds/refill_loop.wav");
Audio_load_sample(&flashSample, "resources/sounds/refill_start.wav");
boost_sample(chargeLoopSample, 4.0f);
boost_sample(flashSample, 4.0f);
```

If either `Audio_load_sample` fails and leaves the pointer NULL, `boost_sample()` immediately dereferences `chunk->abuf` on a NULL pointer.

**Fix:** Add null checks: `if (chargeLoopSample) boost_sample(...)`.

---

### 1.5 Audio_loop_music / Audio_play_music NULL dereference

**File:** `audio.c:26-37`

If `Mix_LoadMUS` fails, `music` is NULL and `Mix_PlayMusic(NULL, ...)` is undefined behavior. No load-failure check.

**Fix:** Guard `Mix_PlayMusic` with `if (music)`.

---

## 2. Major

### 2.1 "Reuse oldest" projectile logic always picks slot 0

**Files:** `sub_pea.c:84-85`, `sub_mgun.c` (same line), `hunter.c:147`

When all projectile slots are full, the code always overwrites slot 0 instead of finding the actual oldest (by `ticksLived`). A freshly-fired bullet in slot 0 gets silently replaced. Same bug in 3 files.

**Fix:** Find the slot with the highest `ticksLived` when all are active.

---

### 2.2 Text_render mallocs and frees every single call

**File:** `text.c:101-167`

Every text draw does `malloc` + `free` + `glBufferData`. With HUD text, skill names, cooldown numbers, etc. rendering dozens of times per frame, this is unnecessary heap churn. The batch renderer already shows the correct pattern (static arrays).

**Fix:** Replace with a static `TextVertex` array sized to a reasonable max (e.g., 1024 chars * 6 verts). Early-return if `len` exceeds the static buffer.

---

### 2.3 SDL_WINDOW_FULLSCREEN macro redefinition

**File:** `graphics.h:17`

```c
#define SDL_WINDOW_FULLSCREEN false
```

Overwrites SDL2's own `SDL_WINDOW_FULLSCREEN` (which is `0x00000001`). Works by coincidence currently, but any file that includes `graphics.h` and then tries to use the SDL flag gets `false` instead.

**Fix:** Rename to `HYBRID_WINDOW_START_FULLSCREEN` or similar. Rename all `SDL_WINDOW_*` custom macros to use a project-specific prefix.

---

### 2.4 sub_mgun.c is a verbatim clone of sub_pea.c

**Files:** `sub_mgun.c` (entire file), `sub_pea.c` (entire file)

~250 lines of wholesale duplication. Only `FIRE_COOLDOWN` differs (200 vs 500). Any bug fix must be manually replicated. The render and bloom_source functions are also identical within each file (4 copies of the same rendering logic).

**Fix:** Either extract a shared `projectile_system.c` with configurable fire rate, or make sub_mgun a thin wrapper that calls sub_pea internals with different parameters.

---

### 2.5 sub_mend.c leaks audio sample on cleanup

**File:** `sub_mend.c:29-31`

`Sub_Mend_cleanup` is empty. `Audio_unload_sample(&sampleHeal)` is never called. Every other subroutine properly unloads its audio.

**Fix:** Add `Audio_unload_sample(&sampleHeal);` to `Sub_Mend_cleanup`.

---

### 2.6 Grid fmod comparison — big grid lines likely never render brighter

**File:** `render.c:112-113`

```c
if (fmod(i, gridSize * bigGridSize) == 0.0)
    alpha = 0.3f;
```

Compares a floating-point `fmod` result to exact `0.0`. Should use integer modulus.

**Fix:** `if (i % (int)(gridSize * bigGridSize) == 0)`.

---

### 2.7 Redundant glUseProgram/uniform calls per flush

**File:** `shader.c:118-124`

`Shader_set_matrices` calls `glUseProgram` every time. `Batch_flush` calls it 3x (lines, triangles, points) with the same shader and matrices. That's 3 redundant `glUseProgram` + 6 redundant `glUniformMatrix4fv` calls per flush.

**Fix:** Call `Shader_set_matrices` once before the three primitive flushes, or track current program and skip redundant binds.

---

### 2.8 HALF_MAP_SIZE macro lacks parentheses

**File:** `map.h:10`

```c
#define HALF_MAP_SIZE MAP_SIZE / 2
```

Classic C macro precedence bug. Works by luck in current expressions.

**Fix:** `#define HALF_MAP_SIZE (MAP_SIZE / 2)`.

---

### 2.9 Cell pool exhaustion on repeated edits

**File:** `map.c:43-52`

Every `Map_set_cell` allocates from the pool even if that coordinate already has a cell. Heavy god-mode editing + undo cycles burn through the 1M entry pool. Pool only resets on `Map_clear`.

**Fix:** Before allocating a new pool entry, check if `map[grid_x][grid_y]` already points into the pool. If so, overwrite in place rather than allocating a new slot.

---

### 2.10 Zone_save on every single edit — O(n^2) at frame rate

**Files:** `zone.c:306,324,357`

Each `Zone_place_cell`, `Zone_remove_cell`, `Zone_undo` triggers a full zone save that iterates all 1024x1024 cells. During god-mode painting (holding LMB and dragging), this happens every frame.

**Fix:** Mark the zone as dirty and defer save to a periodic timer or explicit save action (e.g., Ctrl+S). Or at minimum, throttle saves to once per second.

---

### 2.11 sub_aegis / sub_mend activate while dead

**Files:** `sub_mend.c:36`, `sub_aegis.c:59`

Neither checks `Ship_is_destroyed()` before activation. `sub_pea` and `sub_mgun` properly check `!state->destroyed`.

**Fix:** Add `if (Ship_is_destroyed()) return;` at top of both update functions.

---

### 2.12 Wall avoidance freezes enemies completely

**File:** `enemy_util.c:32-37`

When a wall is within 50 units in the movement direction, the enemy simply stops. No wall-sliding, no alternative direction. Enemies cornered against walls freeze permanently.

**Fix:** Implement wall-sliding: project the desired velocity onto the wall surface normal, or try perpendicular directions. Alternatively, separate X and Y movement and only cancel the blocked axis.

---

### 2.13 Background smoothstep lerp applied incrementally — double-easing

**File:** `background.c:193-199`

```c
d->dir_x += t * (d->target_x - d->dir_x);
```

Applies the absolute smoothstep value as a delta each frame, not from the saved original direction. The actual curve is faster than intended at start, slower at end — opposite of the intended smoothstep.

**Fix:** Save `d->start_x` / `d->start_y` when a new target is picked, then: `d->dir_x = d->start_x + t * (d->target_x - d->start_x)`.

---

### 2.14 Function pointer signature mismatches (UB on non-x86)

**Files:** `grid.c:8`, `map.c`

`Grid_render()` and `Map_render()` take zero parameters but are stored in `RenderableComponent` which expects `void(*)(const void*, const PlaceableComponent*)`. Works on x86-64 (extra args in registers ignored) but is undefined behavior in C99.

**Fix:** Give both functions the correct signature: `void Grid_render(const void *state, const PlaceableComponent *placeable)` and ignore the parameters.

---

### 2.15 Hunter projectiles hit destroyed ship

**File:** `hunter.c:456-462`

No `Ship_is_destroyed()` check before projectile-ship collision test. Projectiles get consumed by hitting a dead ship's last position.

**Fix:** Add `if (Ship_is_destroyed()) ...` guard, or skip the ship collision block entirely when destroyed.

---

### 2.16 Mine_initialize calls exit() while other enemies return gracefully

**File:** `mine.c:58-61`

`exit(-1)` when pool is full. All other enemy types print an error and return.

**Fix:** Replace `exit(-1)` with `printf` + `return` to match the pattern in hunter/seeker/defender.

---

### 2.17 Collision_aabb_test Y-axis assumption inconsistency

**File:** `collision.c:3-11`

Assumes `aY > bY` (top > bottom), while `Collision_point_test` and `Collision_line_aabb_test` normalize with min/max. Different contracts for the same `Rectangle` type.

**Fix:** Either normalize in `Collision_aabb_test` (like the other two functions do), or document the rectangle convention in a comment in `collision.h` and audit all call sites.

---

### 2.18 Minimap truncation differs from correctTruncation

**File:** `map.c:358-361`

`Map_render_minimap` uses bare `(int)` cast which truncates toward zero. The rest of map.c uses `correctTruncation()` which truncates toward negative infinity. For negative world coordinates this produces off-by-one errors in minimap cell positions.

**Fix:** Use `correctTruncation()` consistently in the minimap function.

---

### 2.19 Progression_is_discovered ignores stored `discovered` field

**File:** `progression.c:169-180`

`Progression_is_discovered()` recomputes from fragment counts, completely ignoring `entries[id].discovered`. The `discovered` field is written by `Progression_update` and `Progression_restore` but never authoritatively read. Two sources of truth that can silently disagree.

**Fix:** Either use the stored field (check `entries[id].discovered`) or remove the field entirely and always compute on-the-fly. Don't have both.

---

### 2.20 Skillbar_equip has no validation

**File:** `skillbar.c:260-265`

No check for duplicate equips, no validation that `id` is a valid `SubroutineId`, no type exclusivity enforcement. Relies entirely on callers (catalog) doing the right thing.

**Fix:** Add bounds check on `id`, check for and clear duplicate equips within `Skillbar_equip` itself.

---

## 3. Minor

### 3.1 text_width / measure_text duplicated in 6 files

**Files:** `catalog.c`, `progression.c`, `player_stats.c`, `hud.c`, `savepoint.c`, `mode_mainmenu.c`

All have identical copies reaching into `tr->char_data[ch-32][6]` internals.

**Fix:** Add `float Text_measure_width(TextRenderer *tr, const char *text)` to `text.h/text.c`. Remove all 6 copies.

---

### 3.2 Massive code duplication across enemy types

Hit detection, death flash rendering, spark rendering, cleanup, wander target picking — all copy-pasted across `hunter.c`, `seeker.c`, `defender.c`. Adding a new weapon type means updating 3+ files.

**Fix:** Extract shared helpers to `enemy_util.c`: `Enemy_check_all_weapon_hits()`, `Enemy_render_death_flash()`, `Enemy_render_spark()`. This is a larger refactor best done when adding the next enemy type.

---

### 3.3 Mat4_inverse: singular matrix check compares float to exact 0.0

**File:** `mat4.c:127-129`

Near-singular matrices pass the check and produce garbage inverses with enormous values.

**Fix:** Use epsilon threshold: `if (fabsf(det) < 1e-6f) return Mat4_identity();`.

---

### 3.4 Silent vertex drops on batch overflow

**File:** `batch.c:76-83`

When the batch is full, triangles are silently dropped with no warning. With procgen this could cause invisible world chunks.

**Fix:** Add a one-time `fprintf(stderr, "WARNING: batch overflow, vertices dropped\n")` or an overflow counter queryable for debug HUD.

---

### 3.5 File I/O not checked in font loading

**File:** `text.c:28-31`

`ftell` can return -1 (becomes `SIZE_MAX` when cast to `size_t`), `malloc` can fail, `fread` can short-read. None checked.

**Fix:** Check each return value. `if (fsize < 0)` bail. `if (!font_buffer)` bail. `if (fread(...) != fsize)` bail.

---

### 3.6 highestIndex never decreases in entity system

**File:** `entity.c:38-40`

Only grows, never shrinks. Makes all system loops iterate over more empty slots over time. Combined with O(n^2) collision, this is a slow performance leak.

**Fix:** When destroying an entity, if it was at `highestIndex`, scan backward to find the new highest. Or recalculate periodically.

---

### 3.7 O(n^2) collision with no spatial partitioning

**File:** `entity.c:105-152`

Brute-force double loop. With 2048 entity cap and procgen, this will be the first bottleneck.

**Fix (future):** Implement a spatial hash or grid-based broadphase. The map already has a grid structure that could be reused. Not urgent until procgen fills the entity pool.

---

### 3.8 Timer accumulators can overflow (signed int UB)

**File:** `player_stats.c:74-75`

`timeSinceLastDamage += ticks` — signed `int` overflow after ~37 hours without damage. UB in C.

**Fix:** Change to `unsigned int` (wraps safely) or clamp to a max value (e.g., `INT_MAX / 2`).

---

### 3.9 Render_quad builds full 4x4 matrix transforms for 2D

**File:** `render.c:21-41`

3 `Mat4` constructions + multiply per quad for what is effectively 2D rotation + translation. 64 multiplications + 48 additions per entity.

**Fix (future):** Add a 2D fast path: inline sin/cos with direct vertex computation. Significant win at high entity counts.

---

### 3.10 Single shared spark per enemy type

**Files:** `hunter.c:112-115`, `seeker.c:120-123`, `defender.c:119-122`

Only one spark effect per enemy type. Simultaneous hits on multiple enemies of the same type show only one spark.

**Fix:** Small spark pool (4-8 per type) instead of a single static slot.

---

### 3.11 Inconsistent magic PI values

- `3.14159f` in `sub_aegis.c`, `skillbar.c`
- `3.14159265358979323846` in `sub_pea.c`, `sub_mgun.c`
- `M_PI` in `sub_egress.c`

**Fix:** Use `M_PI` everywhere. Add `#define _USE_MATH_DEFINES` before `<math.h>` if needed for portability.

---

### 3.12 Defender drops fragments randomly regardless of unlock status

**File:** `defender.c:509-512`

50% chance of dropping either `FRAG_TYPE_MEND` or `FRAG_TYPE_AEGIS` without checking which one the player actually still needs.

**Fix:** If only one is still locked, always drop that type. If both locked, randomize. If both unlocked, don't drop.

---

### 3.13 Entity_destroy has no bounds check

**File:** `entity.c:65-68`

`entities[entityId].empty = true` with no check that `entityId < ENTITY_COUNT`. Out-of-bounds write.

**Fix:** Add `if (entityId >= ENTITY_COUNT) return;`.

---

### 3.14 6 declared-but-never-defined functions in entity.h

**File:** `entity.h:34-39`

`Entity_add_state`, `Entity_add_placeable`, `Entity_add_renderable`, `Entity_add_user_updatable`, `Entity_add_ai_updatable`, `Entity_add_collidable`. Dead prototypes from an older API.

**Fix:** Remove the 6 declarations.

---

### 3.15 All AI update functions cast away const from state parameter

**File:** `component.h:35`

Declares `const void *state`, but every enemy update immediately casts to non-const and mutates.

**Fix:** Remove `const` from the `AIUpdatableComponent` function pointer's state parameter.

---

### 3.16 Ship bounding box inconsistency

**Files:** `hunter.c:423` uses 15-unit ship BB, `seeker.c:435` uses 20-unit. Ship itself defines 20.

**Fix:** Define `SHIP_BB_HALF_SIZE` in `ship.h` and reference it everywhere.

---

### 3.17 Cursor uses Render_line_segment (lines flush before triangles)

**File:** `cursor.c:28-35`

Violates the documented rendering gotcha. Lines render behind any batched triangle geometry.

**Fix:** Replace `Render_line_segment` with `Render_thick_line` (renders as triangles).

---

### 3.18 zoneMusicPaths hardcodes array size

**File:** `mode_gameplay.c:60`

`char zoneMusicPaths[3][256]` doesn't reference `ZONE_MAX_MUSIC 3` from `zone.h`.

**Fix:** `char zoneMusicPaths[ZONE_MAX_MUSIC][256]`.

---

### 3.19 srand(time(NULL)) — rapid restarts produce identical sequences

**File:** `sdlapp.c:61`

One-second granularity seeding. Two launches in the same second get the same BGM order.

**Fix:** Mix in `SDL_GetPerformanceCounter()` or `getpid()` for additional entropy.

---

### 3.20 SDL_mixer callback writes shared state from audio thread

**File:** `mode_gameplay.c:510-515`

`on_music_finished` modifies `zoneMusicIndex` and `zoneMusicAdvance` from the audio thread without atomics. Data race, benign on x86 but UB.

**Fix:** Use `_Atomic` or `SDL_AtomicSet` for the shared variables, or set only a single atomic flag and handle the rest on the main thread.

---

### 3.21 Bloom viewport set redundantly inside blur loop

**File:** `bloom.c:252`

`glViewport` called with the same values every iteration inside the blur loop. With `bg_bloom.blur_passes = 20`, that's 40 redundant calls.

**Fix:** Move `glViewport` before the loop.

---

### 3.22 Graphics_toggle_fullscreen destroys and recreates everything

**File:** `graphics.c:59-64`

Destroys GL context, all shaders, all VBOs/VAOs, all FBOs, all textures, font atlas — then recreates from scratch. SDL provides `SDL_SetWindowFullscreen()` which toggles without context destruction.

**Fix:** Use `SDL_SetWindowFullscreen()` + `SDL_GL_GetDrawableSize()` + resize FBOs. Keep the GL context alive.

---

### 3.23 Bloom initialize creates FBOs twice for bg_bloom

**File:** `graphics.c:186-191`

`Bloom_initialize` creates 3 FBOs at divisor=2, then divisor is changed to 8 and `Bloom_resize` recreates them. Wastes 6 GPU allocations at startup.

**Fix:** Allow passing divisor to `Bloom_initialize`, or add a `Bloom_create` that takes all config before allocating.

---

### 3.24 Duplicated shader compile helpers in bloom.c

**File:** `bloom.c:46-84`

`bloom_compile_shader` and `bloom_link_program` are identical to `compile_shader` and `link_program` in `shader.c` except for error message prefix. Comment acknowledges the duplication.

**Fix:** Make the shader.c helpers non-static (or add a shared internal header). Pass a prefix string for error messages.

---

### 3.25 stbtt_BakeFontBitmap return value not checked

**File:** `text.c:37-38`

Returns negative if atlas is too small. With 80px font on 512x512 atlas (title renderer), some glyphs may not fit.

**Fix:** Check return value. If negative, either increase atlas size or log a warning.

---

### 3.26 Bloom FBO incomplete warning doesn't disable bloom

**File:** `bloom.c:139-141`

Prints warning but continues execution. Bloom will silently produce black output.

**Fix:** Set a `bloom->valid = false` flag and skip bloom passes when invalid.

---

### 3.27 Empty function stubs without `void` parameter

**Files:** `render.c:17-19,70-72`, `hud.c:29-35`, `grid.c:17-19`

`void Render_line()` in C99 means "unspecified number of arguments", not zero. Should be `void Render_line(void)`.

**Fix:** Add `void` to all empty parameter lists project-wide.

---

### 3.28 sub_boost description says "Overheats with sustained use" — it doesn't

**File:** `skillbar.c:37`

Sub_boost is elite with no feedback cost and no overheat. Description is inaccurate.

**Fix:** Update to something like `"Hold shift for unlimited speed boost."`.

---

### 3.29 Progression notification eaten when discovery + unlock happen same frame

**File:** `progression.c:78-107`

If a player goes from 0 fragments to threshold in one frame, the discovery notification is immediately overwritten by the unlock notification. Discovery message never shown.

**Fix:** Queue notifications or add a delay between them. Or accept this as intended behavior for threshold-1 unlocks and document it.

---

### 3.30 PlayerStats snapshot doesn't preserve shield/death/timer state

**File:** `player_stats.h:21-25`

`PlayerStatsSnapshot` only captures `integrity` and `feedback`. Shield, death, flash timers all hard-reset on restore. One-frame desync possible with active aegis during zone transition.

**Fix:** Either expand the snapshot to include shield state, or have `Sub_Aegis` listen for zone transitions and self-cancel.

---

### 3.31 POSIX mkdir in savepoint.c not portable

**File:** `savepoint.c:114`

`mkdir("./save", 0755)` uses POSIX API. Will need abstraction for Windows (which uses `_mkdir`).

**Fix (future):** Add a platform abstraction for directory creation when targeting Windows.

---

### 3.32 shipstate.h uses bool without including stdbool.h

**File:** `shipstate.h:5`

Relies on transitive include. Will break if included first in a translation unit.

**Fix:** Add `#include <stdbool.h>`.

---

### 3.33 Fragment_spawn doesn't validate FragmentType

**File:** `fragment.c:112`

`f->color = typeInfo[type].color` — out-of-range `FragmentType` reads out of bounds from `typeInfo[]`. `Fragment_get_count` does validate, but `Fragment_spawn` does not.

**Fix:** Add `if (type < 0 || type >= FRAG_TYPE_COUNT) return;` at top of `Fragment_spawn`.

---

### 3.34 Portal dead code — colorCyan/colorWhite initialized but never used

**File:** `portal.c:19-22`

`colorCyan` and `colorWhite` are initialized via `init_colors()` but portal rendering uses inline float values instead.

**Fix:** Remove the unused static color variables and `init_colors` call.

---

### 3.35 Audio_update is dead code

**File:** `audio.c:4,16-19`

`Audio_update` is defined and `timer_ticks` is written but never read. Never called from anywhere.

**Fix:** Remove `Audio_update` and `timer_ticks`.

---

### 3.36 Map_render calls Map_get_cell with redundant bounds checks

**File:** `map.c:687`

`render_cell` calls `Map_get_cell` for the cell itself + 4 cardinal + 4 diagonal neighbors, each with bounds checks. The caller already clamped coordinates. 9 redundant bounds checks per cell, thousands of cells per frame.

**Fix (future):** Use raw `map[][]` access inside `render_cell` with a single outer bounds check. Return `&boundaryCell` for out-of-range neighbors directly.

---

---

## 4. Overall Assessment

### What's Good

- **Clean architecture** for a C99 game engine. The ECS is simple and effective.
- **Render_* API abstraction** is well-designed for future Metal/D3D backend ports.
- **Static memory management** avoids heap fragmentation (except text.c).
- **Bloom pipeline** is solid — dual-instance FBO approach is clever.
- **Code is readable** and well-organized per module.
- **strncpy used consistently** with explicit null termination.
- **sscanf format strings** have width limits matching buffer sizes.
- No critical buffer overflows found in normal code paths.

### What Needs Work

- **Copy-paste duplication** is the biggest systemic issue. Enemy types share ~60% identical code, sub_mgun is a clone of sub_pea, render/bloom_source functions are duplicated within each entity, and `text_width` has 6 copies. Every bug fix or feature addition has a multiplier.
- **Defensive coding** is inconsistent — some paths check for NULL/bounds, others don't. Audio loading is especially fragile.
- **Implicit contracts** (Rectangle Y-axis orientation, render ordering, function pointer signatures) are undocumented and rely on convention.
- **Editor (god-mode) performance** will degrade with heavy use due to cell pool exhaustion and per-edit zone saves.
- **Collision system** is O(n^2) and will be the first bottleneck when procgen fills the entity pool.

---

## 5. Recommended Fix Priority

### Tier 1 — Fix Now (crashes, gameplay-breaking)

| # | Issue | Effort |
|---|-------|--------|
| 1.1 | Seeker multi-hit damage | Small — add bool flag |
| 1.2 | Entity collision i vs j | Trivial — one character |
| 1.3 | Ship null check on entity add | Trivial — one guard |
| 1.4 | Savepoint audio null check | Trivial — two guards |
| 1.5 | Audio music null check | Trivial — two guards |
| 2.5 | sub_mend audio leak | Trivial — one line |
| 2.11 | sub_aegis/mend activate while dead | Small — two guards |

### Tier 2 — Fix Soon (correctness, maintainability)

| # | Issue | Effort |
|---|-------|--------|
| 2.1 | Projectile slot 0 reuse | Small — find-max loop |
| 2.3 | SDL_WINDOW_FULLSCREEN macro | Small — rename macros |
| 2.6 | Grid fmod comparison | Trivial — int modulus |
| 2.8 | HALF_MAP_SIZE parentheses | Trivial — add parens |
| 2.15 | Hunter projectiles hit dead ship | Trivial — one guard |
| 2.16 | Mine exit() vs return | Trivial — replace exit |
| 2.17 | Collision Y-axis inconsistency | Small — normalize |
| 2.18 | Minimap truncation | Small — use correctTruncation |
| 3.11 | Magic PI values | Small — replace with M_PI |
| 3.13 | Entity_destroy bounds check | Trivial — one guard |
| 3.16 | Ship BB inconsistency | Small — shared constant |
| 3.17 | Cursor line_segment → thick_line | Small — swap calls |
| 3.28 | sub_boost description | Trivial — change string |

### Tier 3 — Fix When Touching (tech debt, performance)

| # | Issue | Effort |
|---|-------|--------|
| 2.2 | Text malloc per call | Medium — static buffer |
| 2.4 | sub_mgun clone of sub_pea | Large — extract shared system |
| 2.7 | Redundant GL state changes | Medium — refactor flush |
| 2.9 | Cell pool exhaustion | Medium — in-place overwrite |
| 2.10 | Zone_save per edit | Medium — dirty flag + deferred save |
| 2.12 | Enemy wall freeze | Medium — wall sliding logic |
| 2.13 | Background lerp double-easing | Small — save start values |
| 2.14 | Function pointer signatures | Small — add parameters |
| 3.1 | text_width duplication (6 copies) | Medium — shared function |
| 3.2 | Enemy code duplication | Large — extract to enemy_util |
| 3.14 | Dead entity.h prototypes | Trivial — delete lines |
| 3.15 | Const-cast in AI updates | Small — fix signature |

### Tier 4 — Future / Procgen Prerequisites

| # | Issue | Effort |
|---|-------|--------|
| 3.6 | highestIndex never shrinks | Medium — scan-back logic |
| 3.7 | O(n^2) collision → spatial hash | Large — new broadphase |
| 3.9 | 2D matrix fast path | Medium — specialized transform |
| 3.10 | Multi-spark pool per enemy type | Medium — small arrays |
| 3.22 | Fullscreen toggle via SDL API | Medium — preserve GL context |
