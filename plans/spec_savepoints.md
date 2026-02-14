# Save Points — Draft Spec

## Context

Save points are persistent checkpoints in cyberspace. When the player dies, they respawn at the last save point they activated instead of at the zone origin. Checkpoint data persists to disk, enabling the main menu Load button to resume play from the last save.

Future: save points double as fast-travel nodes (Diablo-style waypoint system). This spec does not implement fast travel but designs with it in mind.

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Architecture | Static pool, same as portals/mines | Proven pattern, zero malloc |
| Activation | Dwell time (~1s) | Consistent with portals. Prevents drive-by saves. |
| What's saved | Zone path + save point ID + position + progression + skillbar | Minimum useful checkpoint state |
| Re-activation | Leave and re-enter | Prevents spam-saving while standing still. Deactivates after save until player exits bounding box. |
| Cross-zone | Yes | Dying in zone_002 can respawn you at a save point in zone_001 |
| Save slot count | 1 (last visited) | Single checkpoint. Fast-travel will reference all visited save points later. |
| Persistence | Written to disk | Checkpoint survives game quit/restart. Enables main menu Load button. |
| Main menu Load | Rebirth animation | Loading a save from the main menu uses the full rebirth zoom-in. Death respawns and future fast travel do not. |

---

## Save Point Entity

Follows the portal pattern: static pool, singleton renderable, per-instance state.

- **Pool size**: 16 save points per zone (`SAVEPOINT_COUNT`)
- **Bounding box**: 100x100 world units (same as portal)
- **Components**: RenderableComponent (no collision — direct point test like portals)
- **State per save point**: id, position, dwell_timer, anim_timer, ship_inside, phase, flash_timer

### Detection

`Savepoint_update_all()` checks `Ship_get_position()` against each save point's AABB every frame. Same approach as portals.

---

## Save Point Visuals

### Dot Ring (8 dots in a circle, radius ~100 world units)

The dots are the primary visual. No diamond, no filled shape — just the ring of dots.

| Phase | Dot Behavior | Color |
|-------|-------------|-------|
| **Idle** | Static positions around circle, gentle alpha pulse | Cyan, ~70% alpha |
| **Charging** | Begin spinning, accelerate with charge progress. Ease-in rotation speed. | Cyan brightening toward white |
| **Save flash** | 3 rapid flashes (dots go full white -> dim -> white -> dim -> white -> dim). ~150ms per flash cycle (450ms total). | White flashes |
| **Deactivated** | Dots stop spinning, return to static positions, dimmer alpha (~40%). | Dim cyan |
| **Re-entering** (after leaving and returning) | Dots back to idle brightness. Ready for next activation. | Cyan, ~70% alpha |

### Dot Rendering

- 8 dots evenly spaced around a circle (45 degrees apart)
- Each dot rendered as a small quad (~4-5 world units)
- Orbit radius: ~50 world units
- Spin direction: clockwise
- Spin speed ramps from 0 to ~2 revolutions/sec over the 1s dwell period

### Bloom Source

Dots re-rendered slightly larger into bloom FBO for glow effect. Bloom intensity increases during charging.

### Minimap

Cyan dot (distinct from portal white and enemy red).

---

## Save Point Phases

```
IDLE -> CHARGING -> SAVING -> DEACTIVATED -> (player leaves) -> IDLE
```

| Phase | Duration | Behavior |
|-------|----------|----------|
| `SAVEPOINT_IDLE` | — | Dots static. Waiting for ship. |
| `SAVEPOINT_CHARGING` | 1000ms dwell | Ship inside AABB, dwell_timer incrementing. Pull ship toward center (same ease-in as portal). Dots spinning up. |
| `SAVEPOINT_SAVING` | 450ms | Dwell threshold reached. Save executed + written to disk. 3 flash cycles. Ship released (input works). |
| `SAVEPOINT_DEACTIVATED` | — | Save complete. Dots stop spinning, go dim. No interaction until ship leaves AABB. |

Ship leaving AABB at any point during IDLE or DEACTIVATED resets to IDLE. Ship leaving during CHARGING resets dwell_timer and returns to IDLE. Ship dying during CHARGING cancels — no save.

---

## Ship Pull During Charging

Same mechanic as portals:

```c
float charge = (float)dwell_timer / DWELL_THRESHOLD_MS;
if (charge > 1.0f) charge = 1.0f;
float pull = charge * charge; /* ease-in */
Position pulled;
pulled.x = ship_pos.x + (savepoint_pos.x - ship_pos.x) * pull;
pulled.y = ship_pos.y + (savepoint_pos.y - ship_pos.y) * pull;
Ship_set_position(pulled);
```

---

## What Gets Saved (Global Checkpoint)

The save point writes to a **global static checkpoint** (not per-save-point — there's one active checkpoint at a time):

```c
typedef struct {
    bool valid;                          /* false until first save */
    char zone_path[256];                 /* zone file to load on respawn */
    char savepoint_id[32];              /* save point to spawn at */
    Position position;                   /* save point world position */
    bool unlocked[SUB_ID_COUNT];         /* progression unlocks */
    bool discovered[SUB_ID_COUNT];       /* progression discoveries */
    SkillbarSnapshot skillbar;           /* equipped skills */
} SaveCheckpoint;
```

### Save Sequence (during SAVEPOINT_SAVING)

1. Record current zone filepath + save point ID + position
2. Snapshot `Progression_is_unlocked()` and `Progression_is_discovered()` for each `SUB_ID`
3. Snapshot skillbar via `Skillbar_snapshot()`
4. Mark checkpoint as valid
5. Write checkpoint to disk (see Disk Persistence below)

---

## Disk Persistence

The checkpoint is written to a save file so it survives game quit/restart.

### Save File

Path: `./save/checkpoint.sav`

```
zone ./resources/zones/zone_001.zone
savepoint hub_save
position 5600.0 0.0
unlocked sub_pea sub_egress
discovered sub_pea sub_egress sub_mine
skillbar 0:sub_pea 1:sub_mine 2:sub_egress 4:sub_boost
```

Line-based text format, same philosophy as zone files. Easy to inspect and debug. The `save/` directory is created on first write if it doesn't exist.

### Disk API

```c
bool Savepoint_has_save_file(void);           /* check if save file exists on disk */
bool Savepoint_load_from_disk(void);          /* read save file into checkpoint struct */
void Savepoint_write_to_disk(void);           /* write current checkpoint to disk */
const SaveCheckpoint *Savepoint_get_checkpoint(void);
```

- `Savepoint_write_to_disk()` called at end of save sequence (step 5 above)
- `Savepoint_load_from_disk()` called once at app startup to populate checkpoint state
- `Savepoint_has_save_file()` called by main menu to determine Load button state

---

## Main Menu Integration

### Current State

The main menu (`mode_mainmenu.c`) has three buttons: New, **Load** (disabled), Exit. The Load button is initialized with `disabled: true` and has no click callback wired.

### New Behavior

On `Mode_Mainmenu_initialize()`:
1. Check if a valid save file exists via `Savepoint_has_save_file()`
2. Set `loadButton.disabled = !hasValidSave`
3. Wire Load button's click callback to a `load_game_callback`

### sdlapp.c Changes

The main menu currently receives a single `gameplay_mode_callback` for the New button. Loading from save needs a second callback:

```c
static void load_game_callback(void);

void Mode_Mainmenu_initialize(
    void (*quit)(void),
    void (*new_game)(void),
    void (*load_game)(void));
```

`sdlapp.c` provides `load_game_callback()` which calls `change_mode()` with a flag indicating load-from-save, so `Mode_Gameplay_initialize_from_save()` is called instead of the normal `Mode_Gameplay_initialize()`.

### Load Flow

```
Main Menu -> [click Load] -> load_game_callback()
    -> change_mode(GAMEPLAY) with load flag
    -> Mode_Gameplay_initialize_from_save()
        -> Loads checkpoint from disk (already loaded at startup)
        -> Initializes all systems
        -> Loads zone from checkpoint.zone_path (instead of hardcoded zone_001)
        -> Restores progression unlocks/discoveries from checkpoint
        -> Restores skillbar from checkpoint
        -> Sets ship spawn position to checkpoint.position
        -> Plays REBIRTH animation (full 13s zoom-in + Memory Man audio)
```

### New Gameplay Init Path

```c
void Mode_Gameplay_initialize_from_save(void);
```

This is a variant of `Mode_Gameplay_initialize()` that:
- Loads `checkpoint.zone_path` instead of hardcoded `zone_001.zone`
- After system init, restores progression unlocks/discoveries from checkpoint
- Restores skillbar slot assignments from checkpoint
- Sets `complete_rebirth()` to spawn ship at checkpoint position instead of origin
- Still plays the full rebirth sequence (zoom from 0.01 -> 0.5, Memory Man audio)

### Rebirth Animation Scope

The rebirth animation (13s zoom from 0.01 -> 0.5 scale with Memory Man audio) is used **only** for:
- **New game start** (existing)
- **Load from main menu** (new)

It is **not** used for:
- Death respawn (existing death timer -> instant spawn at checkpoint or origin)
- Portal warp arrival (existing warp sequence)
- Future fast travel (TBD — likely a shorter transition)

---

## Death Respawn Changes

### Current Flow (ship.c)

```c
if (shipState.ticksDestroyed >= DEATH_TIMER) {
    placeable->position.x = 0.0;  // always origin
    placeable->position.y = 0.0;
    ...
}
```

### New Flow

1. `ship.c` respawn code calls `Savepoint_get_checkpoint()`
2. If no valid checkpoint -> origin `(0, 0)` as before
3. If valid checkpoint in **current zone** -> `Ship_force_spawn(checkpoint.position)`, restore progression + skillbar
4. If valid checkpoint in **different zone** -> set a pending cross-zone respawn flag. `mode_gameplay.c` picks it up next frame and does zone swap + spawn (reuses warp infrastructure: unload -> destroy -> reload -> spawn). No cinematic — just load and spawn.

Death respawns are always immediate (after the existing death timer). No rebirth animation, no warp sequence.

---

## Zone File Format

```
savepoint <grid_x> <grid_y> <id>
```

Example:
```
savepoint 512 512 hub_save
savepoint 600 480 east_save
```

### Zone Struct Addition

```c
#define ZONE_MAX_SAVEPOINTS 16

typedef struct {
    int grid_x, grid_y;
    char id[32];
} ZoneSavepoint;

// Added to Zone: ZoneSavepoint savepoints[ZONE_MAX_SAVEPOINTS]; int savepoint_count;
```

Parsed alongside portals. Spawned in `apply_zone_to_world()` after portals. Written by `Zone_save()`.

---

## Audio

| Event | Sound | Notes |
|-------|-------|-------|
| Charging (dots spinning) | `resources/sounds/refill_loop.wav` | Loop while in CHARGING phase. Fade out if player leaves early. |
| Save flash | `resources/sounds/refill_start.wav` | Play once when SAVING phase begins (flashes start). |
| Respawn at checkpoint | Spawn sound (existing) | |
| Load from main menu | Memory Man + rebirth (existing) | |

---

## God Mode

Save points display world-space text label with ID (same as portals). No god-mode placement yet — authored in zone files.

---

## Edge Cases

| Case | Handling |
|------|---------|
| No save point visited yet | Respawn at origin (current behavior). Load button disabled on main menu. |
| Save file missing/corrupted | `Savepoint_load_from_disk()` returns false. Load button stays disabled. Respawn at origin. |
| Save point's zone file deleted/missing | Respawn at origin, log warning, invalidate checkpoint |
| Die during SAVEPOINT_CHARGING | Cancel charge, no save, normal death at origin/last checkpoint |
| Multiple save points in one zone | Each works independently, last one activated becomes the checkpoint |
| Save then acquire new skills before dying | Lost — you respawn with state at time of save. Intentional checkpoint behavior. |
| Warp to new zone, die without saving | Respawn at last save point (may be in previous zone, triggers cross-zone respawn) |
| God mode death | Skipped (god mode already prevents death) |
| Click Load, save file valid but zone file gone | Treat same as corrupted save — stay on main menu, disable Load, log error |
| New game overwrites save | New game does NOT clear save data. Player must save at a new save point to update it. |
| Return to main menu then Load | Loads from disk checkpoint, not from in-memory session state. Always reads the file. |

---

## Future: Fast Travel

Not implemented now, but the design supports it:
- Track an array of visited save point IDs + zone paths (not just the last one)
- UI overlay (similar to catalog) lists all visited save points by name
- Selecting one triggers a warp transition to that zone + save point position
- Each save point gets a display name (could be the zone name + save point id, or a custom label)
- Fast travel uses the **warp cinematic** (not rebirth) since it's an in-game action

The `savepoint_id` field and zone path storage already provide what fast travel needs.

---

## Implementation Steps

| Step | What | Files |
|------|------|-------|
| 1 | Zone file parsing + ZoneSavepoint struct | zone.h, zone.c |
| 2 | Savepoint entity skeleton (pool, static dots render) | NEW savepoint.h/c |
| 3 | Spawn savepoints in apply_zone_to_world | zone.c |
| 4 | Dwell detection + ship pull + phase state machine | savepoint.c |
| 5 | Dot animation (idle static, spin-up, flash, deactivate/stop) | savepoint.c |
| 6 | SaveCheckpoint struct + in-memory save execution | savepoint.c |
| 7 | Disk persistence (write + read + has_save_file) | savepoint.c |
| 8 | Death respawn redirect (same zone) | ship.c, savepoint.h |
| 9 | Death respawn redirect (cross zone) | ship.c, mode_gameplay.c |
| 10 | Main menu Load button activation | mode_mainmenu.c, sdlapp.c, savepoint.h |
| 11 | Mode_Gameplay_initialize_from_save (load path with rebirth) | mode_gameplay.c, mode_gameplay.h |
| 12 | Bloom source | savepoint.c, mode_gameplay.c |
| 13 | Minimap blips (green) | savepoint.c, hud.c |
| 14 | God mode labels | savepoint.c |
| 15 | Add save points to zone_001 and zone_002 | resources/zones/ |

---

## Files Summary

| File | Action |
|------|--------|
| `src/savepoint.h` | **NEW** — save point entity header + checkpoint API |
| `src/savepoint.c` | **NEW** — pool, render, update, dwell, phases, checkpoint, disk I/O |
| `src/zone.h` | Modify — ZoneSavepoint struct, savepoint array in Zone |
| `src/zone.c` | Modify — parse/save savepoint lines, spawn in apply_zone_to_world |
| `src/ship.c` | Modify — respawn checks checkpoint instead of hardcoded origin |
| `src/mode_gameplay.h` | Modify — declare Mode_Gameplay_initialize_from_save |
| `src/mode_gameplay.c` | Modify — load-from-save init path, cross-zone respawn, bloom source, god labels |
| `src/mode_mainmenu.h` | Modify — update initialize signature (add load callback) |
| `src/mode_mainmenu.c` | Modify — wire Load button, check save file on init |
| `src/sdlapp.c` | Modify — load checkpoint at startup, provide load_game_callback |
| `src/hud.c` | Modify — minimap save point blips |
| `resources/zones/zone_001.zone` | Modify — add savepoint line |
| `resources/zones/zone_002.zone` | Modify — add savepoint line |
| `save/` | **NEW directory** — created on first save |
