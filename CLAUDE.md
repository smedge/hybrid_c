# Hybrid Game — Project Instructions

## Core Extraction Rule (MANDATORY)

When any ability/mechanic is shared between player subroutines and enemy entities, the shared logic **MUST** live in a `sub_*_core.c/h` file. This is not optional and not something to defer.

**Pattern**:
- `sub_*_core.h` — config struct (tuning knobs) + state struct (runtime data) + API
- `sub_*_core.c` — timer logic, render helpers, audio lifecycle (if applicable)
- `sub_*.c` (player wrapper) — input handling, feedback cost, PlayerStats calls, delegates to core
- Entity files (e.g. `corruptor.c`) — AI decisions, embed core state structs, delegate to core

**When writing a new enemy ability**: If the ability will also become a player subroutine (check PRD progression table), write the core FIRST. Do not hand-roll timers/visuals in the entity file and plan to "extract later." Write `sub_*_core.c/h` from the start, then have both the entity and the player wrapper use it.

**When writing a new player subroutine**: If an enemy already uses this ability with hand-rolled logic, extract the core immediately as part of the implementation. Do not ship duplicated logic.

**Existing cores**: sub_projectile_core, sub_mine_core, sub_cinder_core, sub_dash_core, sub_heal_core, sub_shield_core, sub_emp_core, sub_sprint_core, sub_resist_core

## Audio Ownership Rule

- Skill sounds belong to `*_core.c` files — loaded/played/unloaded there
- Entity sounds are ONLY: damage taken, death, respawn
- Never load skill sounds in entity files
- Never pass audio pointers through config structs

## Build

- `make compile` — must pass clean with zero warnings before considering any task complete
- Always compile after changes; don't batch multiple files of changes without checking

## Plans and Specs

- Implementation plans go in `plans/` directory
- Skill balance worksheet: `plans/spec_skill_balance.md` — update when adding/modifying any skill
