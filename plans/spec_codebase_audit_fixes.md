# Codebase Audit Fixes Spec

Generated from full codebase audit on 2026-02-21. Issues organized by priority.

---

## Critical Fixes

### 1. Entity `highestIndex` never shrinks on enemy cleanup
**Files:** entity.c, hunter.c, seeker.c, defender.c, mine.c
**Problem:** All enemy cleanup functions set `entityRefs[i]->empty = true` directly, bypassing `Entity_destroy()` which maintains the `highestIndex` watermark. Over many death/respawn cycles, all four system loops iterate over more and more dead slots. Performance degrades monotonically.
**Fix:** Either have cleanup functions call `Entity_destroy()` with the entity's ID, or add an `Entity_recalculate_highest_index()` function and call it after bulk cleanup in the respawn flow.

### 2. Batch `auto_flush` breaks render ordering
**Files:** batch.c
**Problem:** When the triangle batch overflows mid-frame, only triangles are flushed — lines and points are left pending. This violates the lines->triangles->points ordering contract, causing visual artifacts in complex scenes approaching 65k vertices.
**Fix:** When any single batch overflows, flush all 3 batches in the correct order (lines->triangles->points), not just the overflowing one.

### 3. Projectile/mine skills NOT deactivated on ship death/respawn
**Files:** ship.c, sub_pea.c, sub_mgun.c, sub_mine.c, sub_inferno.c, sub_disintegrate.c
**Problem:** Respawn reinitializes boost/egress/mend/aegis/stealth but not sub_pea, sub_mgun, sub_mine, sub_inferno, or sub_disintegrate. Active projectiles, blobs, and mines persist across death — continuing to fly, deal damage, and render as ghosts after respawn.
**Fix:** Add deactivate/reset calls for all projectile and deployable skills in the ship respawn flow (ship.c ~line 214).

### 4. Triple-delete of shared ParticleInstance GL resources
**Files:** sub_inferno.c, sub_disintegrate.c, sub_gravwell.c, particle_instance.c
**Problem:** All three cleanup functions call `ParticleInstance_cleanup()`, which deletes the shared shader/VAO/VBOs. The `piInitialized` guard makes 2nd/3rd calls no-ops, but ownership is ambiguous and fragile.
**Fix:** Remove `ParticleInstance_cleanup()` calls from individual sub cleanup functions. Add a single `ParticleInstance_cleanup()` call at the top level (Ship_cleanup or mode_gameplay cleanup) after all subs are cleaned up.

### 5. Bloom double-initialization wastes GPU resources
**Files:** graphics.c, bloom.c
**Problem:** `bg_bloom`, `disint_bloom`, and `light_bloom` are each initialized at divisor=2 (creating 3 FBOs), then immediately resized to the correct divisor (destroying and recreating). 9 FBOs created and immediately thrown away. Also, all 4 bloom instances compile their own identical shader programs — 8 programs where 2 would suffice.
**Fix:** Add a `Bloom_initialize_with_divisor()` that takes the divisor upfront, or pass divisor to `Bloom_initialize()`. For shaders, compile once and share program IDs across instances.

### 6. `Enemy_pick_wander_target` crashes if radius < 1.0
**Files:** enemy_util.c
**Problem:** `(rand() % (int)radius)` — if radius is ever < 1.0, `(int)radius` is 0, causing division by zero.
**Fix:** Guard with `if (radius < 1.0) radius = 1.0;` or return early with zero offset.

### 7. `input_initialize` doesn't initialize `keyH`/`keyI`
**Files:** input.c
**Problem:** These fields are uninitialized (garbage) on the first frame since Input is stack-allocated. Technically UB.
**Fix:** Add `input->keyH = false; input->keyI = false;` to `input_initialize`.

### 8. Procgen `erode_landmark_edges` — 3 mallocs with no NULL checks
**Files:** procgen.c
**Problem:** The only 3 `malloc` calls in the entire codebase (allocating ~9 MB total) have no NULL checks. If any fail, segfault.
**Fix:** Add NULL checks after each malloc, free any already-allocated buffers on failure, and return early.

---

## Moderate Fixes

### 9. Subroutine updates run while ship is destroyed
**Files:** ship.c
**Problem:** After the `if (shipState.destroyed)` block, all subroutine updates execute unconditionally — ghost weapon fire possible during death timer.
**Fix:** Gate subroutine updates on `!shipState.destroyed`, or ensure each sub's update function checks `Ship_is_destroyed()` internally (and verify all do).

### 10. Shared projectile pool update gated on `idx == 0`
**Files:** hunter.c, seeker.c, defender.c
**Problem:** Shared resource updates (projectile pool, spark decay) only run when the index-0 entity of that type processes its update. If entity 0 is removed while others remain, shared resources freeze.
**Fix:** Move shared pool updates out of per-entity update and into a dedicated per-type system update called once per frame (e.g., `Hunter_update_projectiles(ticks)` called from mode_gameplay).

### 11. Seeker recovery deceleration is frame-rate dependent
**Files:** seeker.c
**Problem:** `dampen = 1.0 - 3.0 * dt` — multiplicative damping with linear dt produces different results at different frame rates. At very low framerates (dt > 0.33), velocity instantly zeroes.
**Fix:** Use `pow(base, dt)` for frame-rate independent exponential decay.

### 12. O(n^2) collision system with no spatial partitioning
**Files:** entity.c
**Problem:** Checks every entity pair — up to 4.2M iterations at ENTITY_COUNT 2048. Layer/mask pruning helps, but iteration cost is quadratic.
**Fix:** For now, acceptable at current entity counts. Long-term: add spatial grid/hash for broad-phase. Flag for procgen scaling.

### 13. Per-enemy weapon pool iteration
**Files:** enemy_util.c
**Problem:** Each alive enemy calls `Enemy_check_player_damage()` which iterates all weapon pools (~122k collision checks/frame at full capacity).
**Fix:** Invert the check — iterate weapon pools once, check against enemies. Or add spatial partitioning. Flag for procgen scaling.

### 14. Sub_Mine has no destroyed check before placement
**Files:** sub_mine.c
**Problem:** No `Ship_is_destroyed()` check — can deploy mines while dead.
**Fix:** Add `if (Ship_is_destroyed()) return;` at the top of `Sub_Mine_update`.

### 15. Catalog drag-to-slot loses active state for elite skills
**Files:** catalog.c, skillbar.c
**Problem:** When moving an elite skill between slots, the old slot is cleared first (deactivating the type), then the new slot is equipped without inheriting active state.
**Fix:** Capture the active state before clearing the source slot, then restore it after equipping to the target slot.

### 16. `Graphics_get_drawable_size()` called 5 times per render
**Files:** mode_gameplay.c
**Problem:** Redundant `SDL_GL_GetDrawableSize()` calls each frame.
**Fix:** Hoist to a single call at the top of `Mode_Gameplay_render()`.

### 17. `Batch_flush` uploads shader matrices even when all batches empty
**Files:** batch.c
**Problem:** `Shader_set_matrices()` called unconditionally (glUseProgram + 2x glUniformMatrix4fv) even with zero vertices across all 3 batches. With ~15 flushes per frame, many may be empty.
**Fix:** Early-return from `Batch_flush` if all three batch counts are 0.

### 18. `flush_batch_keep` calls `Shader_set_matrices` up to 3x
**Files:** batch.c
**Problem:** Each of lines/triangles/points in `Batch_flush_keep` redundantly binds the same shader and uploads the same matrices.
**Fix:** Call `Shader_set_matrices` once before the three `flush_batch_keep` calls.

### 19. Defender has no LOS check for de-aggro in SUPPORTING state
**Files:** defender.c
**Problem:** Inconsistent with hunter/seeker which check LOS for de-aggro. Defender stays aggro through walls.
**Fix:** Add `!Enemy_has_line_of_sight()` to the de-aggro condition, or document as intentional.

### 20. `Audio_load_sample` takes `char *` instead of `const char *`
**Files:** audio.h, audio.c
**Problem:** Passing string literals to `char *` is technically UB in C99.
**Fix:** Change signature to `const char *path`.

### 21. Savepoint `frag_names[]` duplicated in two places
**Files:** savepoint.c
**Problem:** Same array defined as local static in `do_save()` (line 154) and `Savepoint_load_from_disk()` (line 579). Must stay in sync manually with FragmentType enum.
**Fix:** Move to a single file-scoped `static const char *frag_names[]` used by both functions.

### 22. Binary FoW save format has no version header
**Files:** fog_of_war.c
**Problem:** No magic number or version. If MAP_SIZE changes, old saves silently corrupt.
**Fix:** Add a small header: `uint32_t magic; uint32_t version; uint32_t map_size;`

### 23. Music callback volatile bool — not guaranteed atomic in C99
**Files:** mode_gameplay.c
**Problem:** `on_music_finished` is called from SDL's audio thread, sets `volatile bool`. Not guaranteed atomic by C standard.
**Fix:** Use `SDL_AtomicInt` or `SDL_AtomicSet`/`SDL_AtomicGet`.

---

## Minor / Code Quality

### 24. Shared shader compilation helpers
**Files:** shader.c, bloom.c, map_reflect.c, map_lighting.c, particle_instance.c
**Problem:** `compile_shader`/`link_program` duplicated in 5 files.
**Fix:** Make public in shader.h, remove duplicates.

### 25. Shared fullscreen quad
**Files:** bloom.c, map_reflect.c, map_lighting.c
**Problem:** Same quad vertex data + VAO/VBO duplicated 6 times (4 bloom + reflect + lighting).
**Fix:** Create a shared fullscreen quad utility in shader.c or a new quad.c.

### 26. Dead code cleanup
**Files:** render.c
**Problem:** `Render_flush_additive` (never called), `Render_line()` and `Render_convex_poly()` (empty stubs).
**Fix:** Remove all three.

### 27. `func()` vs `func(void)` in C99 headers
**Files:** view.h, sub_pea.h, sub_mgun.h, others
**Problem:** `func()` means unspecified parameters in C99, not zero parameters.
**Fix:** Change to `func(void)` throughout.

### 28. `(int)sizeof()` casts in GL buffer calls
**Files:** particle_instance.c, circuit_atlas.c
**Problem:** Unnecessary `(int)` cast on `sizeof` in `glBufferData`. Should use `GLsizeiptr`.
**Fix:** Remove `(int)` casts, use proper GL types.

### 29. `ParticleInstance_draw` does unnecessary `glUseProgram(0)`
**Files:** particle_instance.c
**Fix:** Remove the call — next draw always binds its own program.

### 30. Entity_destroy_all iterates full 2048 instead of highestIndex
**Files:** entity.c
**Fix:** Use `highestIndex` as upper bound.

### 31. Settings toggle rendering duplication
**Files:** settings.c
**Problem:** 9 identical toggle-rendering code blocks (~200 lines).
**Fix:** Extract a `render_toggle(x, y, label, value)` helper.

### 32. `flashTicksLeft` can go negative producing negative alpha
**Files:** player_stats.c
**Fix:** Clamp to 0 after subtraction: `if (flashTicksLeft < 0) flashTicksLeft = 0;`

### 33. `SLOW_VELOCITY` mislabeled
**Files:** ship.c
**Problem:** `SLOW_VELOCITY = 6400.0` with comment "warp speed right now, not slow".
**Fix:** Rename to match actual usage or adjust the value.

### 34. Inconsistent destroyed-state checking across subroutines
**Files:** Various sub_*.c
**Problem:** Some check `!state->destroyed` via ShipState, some call `Ship_is_destroyed()`, some don't check at all.
**Fix:** Standardize on `Ship_is_destroyed()` for all subs (no parent pointer needed).

### 35. Zone_save doesn't check fprintf return values
**Files:** zone.c
**Fix:** Low priority. Consider write-to-temp-then-rename pattern for data integrity.

---

## Static Memory Summary (informational)

Total BSS footprint: ~36 MB
- Zone struct: ~6 MB (zone.c)
- FoW cache (16 zones + active): ~17 MB (fog_of_war.c)
- Map window pixel buffer: 4 MB (map_window.c)
- BatchRenderer: 5.3 MB (batch.h)
- Circuit atlas vertex buffer: 4 MB (circuit_atlas.c)

Not a problem — all BSS segment, zero runtime cost. Just worth knowing.
