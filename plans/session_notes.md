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
