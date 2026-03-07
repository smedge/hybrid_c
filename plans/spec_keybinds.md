# Keybinds System Spec

## Overview

Replace all hardcoded key assignments with a centralized, rebindable keybind system. Keys are configured in the Settings > Keybinds tab and persisted to `settings.cfg`. Every gameplay action — skill slots, UI panels, movement, and primary weapon — routes through the keybind system.

## Design Principles

1. **One source of truth** — `keybinds.c/h` owns all key-to-action mappings. No file checks `input->keyF` directly; everything queries `Keybinds_is_pressed(ACTION_X, input)`.
2. **Slot numbers are permanent** — Skill slots 1-0 always respond to their number key. An alternate key can be assigned per slot that works *in addition to* the number.
3. **No hardcoded skill keys** — The old system where F=shield, G=heal, Space=deploy, Shift=movement is eliminated. Skills activate via their slot key (number or alternate), not via a baked-in key per skill type.
4. **Instant-use activation** — All non-projectile skills fire immediately on slot key press (no toggle). Projectile skills still toggle active/inactive via slot key; firing uses LMB (or its alternate bind).
5. **Device-agnostic bindings** — Bindings are not just keyboard keys. A binding can be a keyboard key, a mouse button, or (future) a gamepad button. The system stores a device type + input code pair, so all input devices are first-class citizens.

---

## Bindable Actions

### Skill Slots (10)

| Action ID | Default Key | Permanent Key | Notes |
|---|---|---|---|
| `BIND_SLOT_1` | *(none)* | `1` | Alternate key for skill slot 1 |
| `BIND_SLOT_2` | *(none)* | `2` | Alternate key for skill slot 2 |
| `BIND_SLOT_3` | *(none)* | `3` | Alternate key for skill slot 3 |
| `BIND_SLOT_4` | *(none)* | `4` | Alternate key for skill slot 4 |
| `BIND_SLOT_5` | *(none)* | `5` | Alternate key for skill slot 5 |
| `BIND_SLOT_6` | *(none)* | `6` | Alternate key for skill slot 6 |
| `BIND_SLOT_7` | *(none)* | `7` | Alternate key for skill slot 7 |
| `BIND_SLOT_8` | *(none)* | `8` | Alternate key for skill slot 8 |
| `BIND_SLOT_9` | *(none)* | `9` | Alternate key for skill slot 9 |
| `BIND_SLOT_0` | *(none)* | `0` | Alternate key for skill slot 10 |

Slot alternate keys default to unbound. The number key always works regardless.

### Primary Weapon

| Action ID | Default Key | Notes |
|---|---|---|
| `BIND_PRIMARY_WEAPON` | *(none)* | Keyboard alternative to LMB for firing the active projectile sub. LMB always works regardless. |

### UI Panels

| Action ID | Default Key | Notes |
|---|---|---|
| `BIND_SETTINGS` | `I` | Toggle settings window |
| `BIND_CATALOG` | `P` | Toggle subroutine catalog |
| `BIND_DATA_LOGS` | `L` | Toggle data logs |
| `BIND_MAP` | `M` | Toggle map window |
| `BIND_GODMODE` | `O` | Toggle god mode (dev) |

### Movement

| Action ID | Default Key | Notes |
|---|---|---|
| `BIND_MOVE_UP` | `W` | Move up |
| `BIND_MOVE_DOWN` | `S` | Move down |
| `BIND_MOVE_LEFT` | `A` | Move left |
| `BIND_MOVE_RIGHT` | `D` | Move right |
| `BIND_SLOW` | `LCtrl` | Slow movement modifier |

### Total: 21 bindable actions

---

## Instant-Use Skill Activation (Major Change)

Currently, `toggle_slot()` in `skillbar.c` toggles activation state for most skills, and individual `sub_*.c` files check hardcoded keys like `input->keyF`, `input->keyG`, `input->keySpace`, `input->keyLShift` to fire abilities. This changes:

### New behavior by skill type

| Skill Type | Slot key behavior | Fire mechanism |
|---|---|---|
| **Projectile** (pea, mgun, tgun, inferno, disintegrate, ember, flak) | Toggle active/inactive | LMB (or `BIND_PRIMARY_WEAPON`) fires while active |
| **Deployable** (mine, cinder) | **Instant deploy** on press | Slot key press directly deploys (was Space) |
| **Movement** (boost, egress, sprint, blaze) | **Instant activate** on press | Slot key press directly activates (was Shift) |
| **Shield** (aegis, immolate, resist, temper) | **Instant activate** on press | Slot key press directly activates (was F) |
| **Healing** (mend, cauterize) | **Instant activate** on press | Slot key press directly activates (was G) |
| **Stealth** (stealth, smolder) | **Instant activate** on press | Slot key press directly activates (already works this way) |
| **Control** (gravwell) | **Instant activate** on press | Slot key press directly activates (already works this way) |
| **Area Effect** (emp, heatwave) | **Instant activate** on press | Slot key press directly activates (already works this way) |

### Files requiring activation refactor

Each of these files currently checks a hardcoded key. The hardcoded key check must be removed and replaced with activation via `toggle_slot()` (which becomes `activate_slot()` for instant-use types):

| File | Current hardcoded key | Change |
|---|---|---|
| `sub_boost.c` | `input->keyLShift` | Remove key check from update. Activation comes from skillbar slot press. |
| `sub_egress.c` | `input->keyLShift` | Same |
| `sub_sprint.c` | `input->keyLShift` | Same |
| `sub_blaze.c` | `input->keyLShift` | Same |
| `sub_mine.c` | `input->keySpace` | Same |
| `sub_cinder.c` | `input->keySpace` | Same |
| `sub_aegis.c` | `input->keyF` | Same |
| `sub_immolate.c` | `input->keyF` | Same |
| `sub_resist.c` | `input->keyF` | Same |
| `sub_mend.c` | `input->keyG` | Same |
| `sub_cauterize.c` | `input->keyG` | Same |
| `sub_pea.c` | `input->mouseLeft` | Keep LMB, add `BIND_PRIMARY_WEAPON` as alt |
| `sub_mgun.c` | `input->mouseLeft` (assumed) | Same |
| `sub_tgun.c` | `input->mouseLeft` (assumed) | Same |
| `sub_inferno.c` | `input->mouseLeft` (assumed) | Same |
| `sub_disintegrate.c` | `input->mouseLeft` (assumed) | Same |
| `sub_ember.c` | `input->mouseLeft` (assumed) | Same |
| `sub_flak.c` | `input->mouseLeft` (assumed) | Same |

### Activation flow (new)

```
Player presses slot key (number or alternate)
  -> Skillbar receives key event
  -> If slot has a projectile sub: toggle active/inactive (existing behavior)
  -> If slot has any other sub type: call try_activate() on that sub directly
     (no toggle state, no "active border" flip — the sub handles its own
      cooldown/duration/feedback checks internally)
```

For movement subs specifically (boost, sprint), the slot key press starts the ability. The sub's own update function handles duration/continuation logic. For hold-to-use subs like boost, the slot alternate key (or number key) must be held — `Keybinds_is_held(BIND_SLOT_N, input)` provides this.

### Hold vs press distinction

- **Press** (edge-triggered): Deploy mine, activate shield, heal, dash, stealth, gravwell, emp. Fires once per key-down event.
- **Hold** (level-triggered): Boost, sprint. Active while key is held, stops on release.
- Projectile subs use **hold** on LMB/primary_weapon to fire continuously.

The keybind system tracks both states. `Keybinds_pressed()` returns true on the frame the key goes down. `Keybinds_held()` returns true every frame the key is down.

---

## Architecture

### New files

#### `keybinds.h`
```c
#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <stdbool.h>
#include "input.h"
#include <SDL2/SDL.h>

typedef enum {
    BIND_SLOT_1, BIND_SLOT_2, BIND_SLOT_3, BIND_SLOT_4, BIND_SLOT_5,
    BIND_SLOT_6, BIND_SLOT_7, BIND_SLOT_8, BIND_SLOT_9, BIND_SLOT_0,
    BIND_PRIMARY_WEAPON,
    BIND_SETTINGS, BIND_CATALOG, BIND_DATA_LOGS, BIND_MAP, BIND_GODMODE,
    BIND_MOVE_UP, BIND_MOVE_DOWN, BIND_MOVE_LEFT, BIND_MOVE_RIGHT,
    BIND_SLOW,
    BIND_COUNT
} BindAction;

/* Input device types — extensible for future gamepad support */
typedef enum {
    BIND_DEVICE_NONE,       /* unbound */
    BIND_DEVICE_KEYBOARD,   /* code = SDL_Keycode */
    BIND_DEVICE_MOUSE,      /* code = SDL mouse button (SDL_BUTTON_LEFT, etc.) */
    /* BIND_DEVICE_GAMEPAD, — future */
} BindDevice;

typedef struct {
    BindDevice device;
    int code;               /* SDL_Keycode for keyboard, SDL_BUTTON_* for mouse */
} BindInput;

void Keybinds_initialize(void);
void Keybinds_update(const Input *input);

/* Query current frame state */
bool Keybinds_pressed(BindAction action);   /* true on key-down edge only */
bool Keybinds_held(BindAction action);      /* true while key is down */

/* Binding management */
BindInput Keybinds_get_binding(BindAction action);
void Keybinds_set_binding(BindAction action, BindInput binding);
void Keybinds_clear_binding(BindAction action);
const char *Keybinds_get_binding_name(BindAction action);
const char *Keybinds_get_action_name(BindAction action);

/* Helpers to create bindings */
BindInput BindInput_key(SDL_Keycode key);
BindInput BindInput_mouse(int button);      /* SDL_BUTTON_LEFT, etc. */
BindInput BindInput_none(void);

/* Conflict resolution — returns action that already uses this input, or -1 */
int Keybinds_find_conflict(BindInput binding, BindAction exclude);

/* Persistence (called by Settings_save / Settings_load) */
void Keybinds_save(FILE *f);
void Keybinds_load_entry(const char *key, int device, int code);

#endif
```

#### `keybinds.c`

- Static array of `BindInput bindings[BIND_COUNT]` with defaults
- Each `BindInput` stores a `BindDevice` (none/keyboard/mouse) + `int code`
- Static arrays `bool prev_state[BIND_COUNT]` and `bool curr_state[BIND_COUNT]` for edge detection
- `Keybinds_update()` reads `input` struct each frame, resolves each binding's device+code against the current input state to set `curr_state`
  - `BIND_DEVICE_KEYBOARD`: check SDL key state via `input` struct fields or `SDL_GetKeyboardState()`
  - `BIND_DEVICE_MOUSE`: check `input->mouseLeft`, `input->mouseRight`, `input->mouseMiddle` based on button code
- `Keybinds_pressed()` = `curr_state && !prev_state`
- `Keybinds_held()` = `curr_state`
- Slot actions check BOTH the number key (`input->keySlot`) AND the bound alternate (keyboard or mouse)
- `BIND_PRIMARY_WEAPON` checks its bound input; if unbound, LMB still works via the existing projectile sub code
- Movement actions check their bound input (default WASD)
- `Keybinds_get_binding_name()` returns human-readable names: "W", "LShift", "LMB", "RMB", "Mouse4", etc.

### Integration points

#### `settings.c` — Keybinds tab

The existing `TAB_KEYBINDS` (tab index 2) is currently empty. Populate it with:

- Scrollable list of all `BIND_COUNT` actions
- Each row: action name (left) + key button (right)
- Key button shows current binding or "--" if unbound
- Click key button -> enters listen mode ("Press a key or mouse button..." text, pulsing border)
- Listens for ANY input device: keyboard key press OR mouse button click
- Next keyboard key or mouse button assigns that input to the action
- ESC during listen cancels without changing
- LMB during listen binds LMB to that action (does NOT act as a UI click while listening)
- **Conflict swap**: if the pressed input is already bound to another action, that action gets unbound (swapped to none). A brief flash/highlight on the swapped row indicates the change.
- "Reset Defaults" button at bottom restores all bindings to defaults
- Bindings are pending until OK (same as other settings tabs)

#### `settings.cfg` persistence

Keybinds save as `bind_<action_name> <device_int> <code_int>` lines in `settings.cfg`, e.g.:
```
bind_slot_1 1 113        # device=keyboard(1), code=SDLK_q(113)
bind_slot_3 2 1          # device=mouse(2), code=SDL_BUTTON_LEFT(1)
bind_settings 1 105      # device=keyboard(1), code=SDLK_i(105)
bind_move_up 1 119       # device=keyboard(1), code=SDLK_w(119)
```
Unbound actions save as `bind_<action_name> 0 0` (device=none).

#### `skillbar.c` — activation refactor

- `toggle_slot()` renamed/refactored to `activate_slot()`
- For projectile types: still toggles `active_sub[type]`
- For all other types: calls `try_activate()` on the sub directly (no toggle)
- `Skillbar_update()` checks `Keybinds_pressed(BIND_SLOT_N)` instead of `input->keySlot`
- For hold-type subs (boost, sprint): `Skillbar_update()` also checks `Keybinds_held(BIND_SLOT_N)` and routes to sub's hold/release

#### `mode_gameplay.c` — UI key migration

Replace all hardcoded UI key checks:
```c
// Before:
if (input->keyI && !godModeActive && !dataNodeReading)

// After:
if (Keybinds_pressed(BIND_SETTINGS) && !godModeActive && !dataNodeReading)
```

Same for `keyP` -> `BIND_CATALOG`, `keyL` -> `BIND_DATA_LOGS`, `keyM` -> `BIND_MAP`, `keyO` -> `BIND_GODMODE`.

#### `ship.c` — movement migration

Replace `input->keyW/A/S/D` with `Keybinds_held(BIND_MOVE_UP/LEFT/DOWN/RIGHT)`.
Replace `input->keyLControl` with `Keybinds_held(BIND_SLOW)`.

#### `sub_*.c` — remove hardcoded keys

All `input->keyF`, `input->keyG`, `input->keySpace`, `input->keyLShift` checks in sub files are removed. Activation comes from `skillbar.c` calling into each sub's `try_activate()` function.

For projectile subs, the LMB check changes to:
```c
// LMB fires projectiles UNLESS LMB has been rebound to another action
bool fire = Keybinds_held(BIND_PRIMARY_WEAPON);
if (!Keybinds_is_lmb_rebound() && userInput->mouseLeft)
    fire = true;
if (fire && ...)
```
`Keybinds_is_lmb_rebound()` returns true if any action has `BIND_DEVICE_MOUSE, SDL_BUTTON_LEFT` assigned. This prevents LMB from double-firing a skill AND a projectile.

---

## Implementation Order

### Phase 1: Keybind infrastructure
1. Create `keybinds.c/h` with action enum, defaults, state tracking
2. Wire `Keybinds_update()` into the game loop (in `mode_gameplay.c` update, before anything reads input)
3. Add persistence (save/load in `settings.c`)
4. `make compile` — verify clean build

### Phase 2: UI panel key migration
1. Replace `input->keyI/P/L/M/O` in `mode_gameplay.c` with `Keybinds_pressed()` calls
2. Verify all panels still open/close correctly
3. `make compile`

### Phase 3: Movement migration
1. Replace `input->keyW/A/S/D` + `keyLControl` in `ship.c` with `Keybinds_held()` calls
2. Also update `god_mode_update()` if it reads movement keys
3. `make compile`

### Phase 4: Skill activation refactor
1. Refactor `toggle_slot()` -> instant-use activation for non-projectile types
2. Remove hardcoded key checks from all `sub_*.c` files (F, G, Space, Shift)
3. Add `BIND_PRIMARY_WEAPON` as alt-fire for projectile subs
4. Handle hold-type subs (boost, sprint) — slot key hold = active, release = stop
5. `make compile` + test each skill type

### Phase 5: Settings UI
1. Build the Keybinds tab in `settings.c` with click-to-bind UI
2. Implement conflict detection + swap
3. Add "Reset Defaults" button
4. Pending/OK/Cancel flow (matches existing settings pattern)
5. `make compile` + test rebinding

---

## Edge Cases

- **God mode**: When god mode is active, number keys jump to zones (existing behavior in `mode_gameplay.c:953`). Slot alternate keys should NOT activate skills in god mode. Movement rebinds should still work in god mode.
- **Text input mode**: When `textInputActive` (zone label editing in god mode), all keybinds are suppressed.
- **Panels open**: When catalog/settings/map/logs are open, skill slot keys are suppressed (existing behavior where input is filtered).
- **Data node reading**: Keybinds for panels/skills suppressed while data node overlay is showing (existing behavior).
- **Mouse buttons are bindable**: LMB, RMB, MMB, Mouse4, Mouse5 are all valid bind targets. A player could bind RMB to a skill slot or LMB to a movement action if they want.
- **LMB for UI clicks**: LMB always works for UI interaction (clicking buttons, tabs, catalog drag, etc.) regardless of what it's bound to. During gameplay, if LMB is bound to a non-projectile action, that action fires AND projectile subs do NOT auto-fire on LMB (the binding takes priority). If LMB is not bound to anything, it retains its default behavior of firing the active projectile sub.
- **ESC key**: Not rebindable. Always closes panels / cancels operations.
- **Number keys 1-0**: Not rebindable for slot activation. They are permanent. The alternate bind is an *additional* key, not a replacement.
- **Modifier keys as binds**: LShift, LCtrl, Tab, Space, etc. are valid bind targets. A player could bind LShift to slot 3 if they want.
- **Unbound actions**: Movement defaults are always set (WASD). UI defaults are always set. Slot alternates and primary_weapon default to unbound (`BIND_DEVICE_NONE`).
- **Listen mode and mouse**: When the keybind UI is in listen mode, mouse clicks bind to the action instead of acting as UI clicks. ESC exits listen mode.

## Future Work (out of scope for now)

- **Gamepad / controller support** — `BindDevice` enum is designed to extend with `BIND_DEVICE_GAMEPAD`. The listen mode, conflict resolution, and persistence all work with the device+code pair, so adding a new device type is additive. When implemented, the listen mode will also capture gamepad button presses.

## Non-Goals

- Multiple key combos (chord bindings) per action
- Per-profile keybind sets
- Rebinding ESC or number keys 1-0
