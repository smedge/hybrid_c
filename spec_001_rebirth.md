# spec_001: Rebirth Effect

## Summary

The rebirth effect plays once per session — when the player starts a new game or loads a saved game. It does NOT play on zone transitions (that would get tiresome). It represents the Hybrid AI coming to life — a cinematic zoom-in from maximum distance to gameplay view, accompanied by the opening 13 seconds of "Memory Man". Modeled after Samus's spawn-in from the Metroid series.

## Sequence

```
T=0s    Zone loads. Camera at MIN_ZOOM (0.01). Memory Man begins playing.
        World is visible (grid, map cells, mines) but ship is NOT spawned.
        Player input is disabled (no movement, no shooting, no zoom).
        HUD is visible. Cursor is hidden.

T=0-13s Camera smoothly zooms from MIN_ZOOM (0.01) to default zoom (0.5).
        Zoom curve should feel cinematic — slow at the start, accelerating,
        then easing into the final zoom. Not linear.

T=13s   Camera arrives at default zoom (0.5).
        Ship spawns at origin (0,0) with respawn sound (statue_rise.wav).
        Random zone BGM starts playing (looped).
        Memory Man begins 2-second fade out.
        Player input enabled. Cursor shown.

T=15s   Memory Man fully faded out and stopped.
        Normal gameplay in progress.
```

## Camera Zoom Curve

The zoom interpolates from `0.01` to `0.5` over 13 seconds. A linear interpolation would feel mechanical. Use an ease-in-out curve:

- **Ease-in-out cubic** or similar: slow start, fast middle, gentle arrival
- The exact curve can be tuned by feel, but the key is that arrival at gameplay zoom feels like a gentle landing, not an abrupt stop

The zoom value at time `t` (0.0 to 1.0 normalized):
```
// Ease-in-out cubic
if (t < 0.5)
    progress = 4 * t * t * t;
else
    progress = 1 - pow(-2 * t + 2, 3) / 2;

scale = MIN_ZOOM + (DEFAULT_ZOOM - MIN_ZOOM) * progress;
```

## Audio

- **Memory Man**: `resources/music/deadmau5_MemoryMan.mp3`
  - Plays from T=0 for 15 seconds total (13s zoom + 2s fade)
  - At T=13s, `Mix_FadeOutMusic(2000)` begins the 2-second fade
  - SDL_mixer handles the fade natively

- **Zone BGM**: One of the 7 random tracks (existing logic)
  - Starts at T=13s
  - Problem: SDL_mixer only supports one music stream. Memory Man is on the music channel.
  - **Solution**: Play Memory Man as a sound effect on a dedicated channel (not as music). This frees the music channel for zone BGM at T=13s. Use `Mix_LoadWAV` + `Mix_PlayChannel` for Memory Man, and `Mix_FadeOutChannel(channel, 2000)` for the fade.
  - Alternative: Load Memory Man as music, crossfade to BGM at T=13s using `Mix_FadeOutMusic` + delayed `Mix_FadeInMusic`. This is simpler but the crossfade may sound muddy.

## Input

During the rebirth sequence (T=0 to T=13s):
- **Disable**: WASD movement, mouse click (shooting), mouse wheel (zoom)
- **Allow**: Esc (return to menu — user should always be able to bail), F11 (fullscreen toggle), F12 (quit)

At T=13s, full input is restored.

## Ship Spawn

Currently the ship is created in `Ship_initialize()` with `shipState.destroyed = true` and respawns after `DEATH_TIMER` (3000ms). For rebirth:

- Ship entity is created during zone load (so it exists in the world) but remains in destroyed state
- At T=13s, force-spawn the ship: set `destroyed = false`, position to (0,0), play `statue_rise.wav`
- Skip the normal 3-second death timer for this initial spawn

This requires a new function: `Ship_force_spawn()` or similar, called by the rebirth sequence at completion.

## State Management

Add a rebirth state to `mode_gameplay.c`:

```c
typedef enum {
    GAMEPLAY_REBIRTH,   // Zoom-in sequence active
    GAMEPLAY_ACTIVE     // Normal gameplay
} GameplayState;

static GameplayState gameplayState;
static int rebirthTimer;            // Milliseconds elapsed
#define REBIRTH_DURATION 13000      // 13 seconds zoom
#define REBIRTH_FADE_DURATION 2000  // 2 second music fade
```

During `GAMEPLAY_REBIRTH`:
- `Mode_Gameplay_update` ticks `rebirthTimer`, updates zoom, skips entity user updates
- AI updates still run (mines blink, world feels alive during zoom)
- Collision system still runs (in case we want environmental activity)
- When `rebirthTimer >= REBIRTH_DURATION`, transition to `GAMEPLAY_ACTIVE`

## View Changes

`view.c` needs:
- `View_set_scale(double scale)` — allow rebirth to directly set zoom level
- Rebirth sequence calls this each frame with the eased zoom value
- During rebirth, `View_update` should ignore mouse wheel input (or the input system blocks it)

## Files to Modify

| File | Changes |
|------|---------|
| `mode_gameplay.c` | Rebirth state machine, timer, zoom animation, spawn trigger, BGM delay |
| `mode_gameplay.h` | Memory Man path define |
| `view.h` / `view.c` | Add `View_set_scale(double)` |
| `ship.c` / `ship.h` | Add `Ship_force_spawn()` to bypass death timer |
| `audio.c` / `audio.h` | Add channel-based playback for Memory Man (load as chunk, play on reserved channel, fade out). Or implement `Audio_play_music_fade` |
| `cursor.c` | Respect a visibility flag during rebirth (or handle in mode_gameplay render) |

## Edge Cases

- **Player presses Esc during rebirth**: Clean up rebirth state, stop Memory Man, return to menu normally
- **Load game (future)**: Same sequence, but camera zooms to the player's last save/spawn point instead of origin
- **Memory Man file missing**: Graceful fallback — skip audio, still do the zoom

## Future Considerations

- The rebirth effect could evolve visually (particle effects, screen flash at spawn, HUD elements animating in)
- The zoom target could be the player's last save/spawn point rather than always origin
- Different zones could have different rebirth music tracks
