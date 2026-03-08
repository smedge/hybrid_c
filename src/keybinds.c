#include "keybinds.h"

#include <string.h>

static BindInput bindings[BIND_COUNT];
static bool curr_state[BIND_COUNT];
static bool prev_state[BIND_COUNT];

static const char *action_names[BIND_COUNT] = {
	[BIND_SLOT_1]         = "Skill Slot 1",
	[BIND_SLOT_2]         = "Skill Slot 2",
	[BIND_SLOT_3]         = "Skill Slot 3",
	[BIND_SLOT_4]         = "Skill Slot 4",
	[BIND_SLOT_5]         = "Skill Slot 5",
	[BIND_SLOT_6]         = "Skill Slot 6",
	[BIND_SLOT_7]         = "Skill Slot 7",
	[BIND_SLOT_8]         = "Skill Slot 8",
	[BIND_SLOT_9]         = "Skill Slot 9",
	[BIND_SLOT_0]         = "Skill Slot 0",
	[BIND_PRIMARY_WEAPON] = "Primary Weapon",
	[BIND_SETTINGS]       = "Settings",
	[BIND_CATALOG]        = "Catalog",
	[BIND_DATA_LOGS]      = "Data Logs",
	[BIND_MAP]            = "Map",
	[BIND_GODMODE]        = "God Mode",
	[BIND_MOVE_UP]        = "Move Up",
	[BIND_MOVE_DOWN]      = "Move Down",
	[BIND_MOVE_LEFT]      = "Move Left",
	[BIND_MOVE_RIGHT]     = "Move Right",
};

/* Persistence keys (must match load_entry parsing) */
static const char *persist_keys[BIND_COUNT] = {
	[BIND_SLOT_1]         = "bind_slot_1",
	[BIND_SLOT_2]         = "bind_slot_2",
	[BIND_SLOT_3]         = "bind_slot_3",
	[BIND_SLOT_4]         = "bind_slot_4",
	[BIND_SLOT_5]         = "bind_slot_5",
	[BIND_SLOT_6]         = "bind_slot_6",
	[BIND_SLOT_7]         = "bind_slot_7",
	[BIND_SLOT_8]         = "bind_slot_8",
	[BIND_SLOT_9]         = "bind_slot_9",
	[BIND_SLOT_0]         = "bind_slot_0",
	[BIND_PRIMARY_WEAPON] = "bind_primary_weapon",
	[BIND_SETTINGS]       = "bind_settings",
	[BIND_CATALOG]        = "bind_catalog",
	[BIND_DATA_LOGS]      = "bind_data_logs",
	[BIND_MAP]            = "bind_map",
	[BIND_GODMODE]        = "bind_godmode",
	[BIND_MOVE_UP]        = "bind_move_up",
	[BIND_MOVE_DOWN]      = "bind_move_down",
	[BIND_MOVE_LEFT]      = "bind_move_left",
	[BIND_MOVE_RIGHT]     = "bind_move_right",
};

static BindInput default_bindings[BIND_COUNT];

static void set_defaults(void)
{
	for (int i = 0; i < BIND_COUNT; i++)
		bindings[i] = BindInput_none();

	/* Slot alternates default to unbound */

	/* Primary weapon defaults to unbound (LMB always works via sub code) */

	/* UI panels */
	bindings[BIND_SETTINGS]  = BindInput_key(SDLK_i);
	bindings[BIND_CATALOG]   = BindInput_key(SDLK_p);
	bindings[BIND_DATA_LOGS] = BindInput_key(SDLK_l);
	bindings[BIND_MAP]       = BindInput_key(SDLK_m);
	bindings[BIND_GODMODE]   = BindInput_key(SDLK_o);

	/* Movement */
	bindings[BIND_MOVE_UP]    = BindInput_key(SDLK_w);
	bindings[BIND_MOVE_DOWN]  = BindInput_key(SDLK_s);
	bindings[BIND_MOVE_LEFT]  = BindInput_key(SDLK_a);
	bindings[BIND_MOVE_RIGHT] = BindInput_key(SDLK_d);
}

static bool poll_binding(BindInput b)
{
	if (b.device == BIND_DEVICE_NONE)
		return false;

	if (b.device == BIND_DEVICE_KEYBOARD) {
		SDL_Scancode sc = SDL_GetScancodeFromKey((SDL_Keycode)b.code);
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		return keys[sc] != 0;
	}

	if (b.device == BIND_DEVICE_MOUSE) {
		Uint32 buttons = SDL_GetMouseState(NULL, NULL);
		return (buttons & SDL_BUTTON(b.code)) != 0;
	}

	return false;
}

void Keybinds_initialize(void)
{
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;

	set_defaults();

	/* Save defaults for reset */
	memcpy(default_bindings, bindings, sizeof(bindings));

	memset(curr_state, 0, sizeof(curr_state));
	memset(prev_state, 0, sizeof(prev_state));
}

void Keybinds_update(void)
{
	memcpy(prev_state, curr_state, sizeof(curr_state));

	for (int i = 0; i < BIND_COUNT; i++)
		curr_state[i] = poll_binding(bindings[i]);
}

bool Keybinds_pressed(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return false;
	return curr_state[action] && !prev_state[action];
}

bool Keybinds_held(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return false;
	return curr_state[action];
}

BindInput Keybinds_get_binding(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return BindInput_none();
	return bindings[action];
}

void Keybinds_set_binding(BindAction action, BindInput binding)
{
	if (action < 0 || action >= BIND_COUNT)
		return;
	bindings[action] = binding;
}

void Keybinds_clear_binding(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return;
	bindings[action] = BindInput_none();
}

const char *Keybinds_input_name(BindInput b)
{
	if (b.device == BIND_DEVICE_NONE)
		return "--";

	if (b.device == BIND_DEVICE_MOUSE) {
		switch (b.code) {
		case SDL_BUTTON_LEFT:   return "LMB";
		case SDL_BUTTON_RIGHT:  return "RMB";
		case SDL_BUTTON_MIDDLE: return "MMB";
		case SDL_BUTTON_X1:     return "Mouse4";
		case SDL_BUTTON_X2:     return "Mouse5";
		default:                return "Mouse?";
		}
	}

	if (b.device == BIND_DEVICE_KEYBOARD) {
		const char *name = SDL_GetKeyName((SDL_Keycode)b.code);
		if (name && name[0] != '\0')
			return name;
		return "???";
	}

	return "--";
}

const char *Keybinds_get_binding_name(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return "--";
	return Keybinds_input_name(bindings[action]);
}

const char *Keybinds_get_action_name(BindAction action)
{
	if (action < 0 || action >= BIND_COUNT)
		return "";
	return action_names[action];
}

void Keybinds_reset_defaults(void)
{
	memcpy(bindings, default_bindings, sizeof(bindings));
}

BindInput BindInput_key(SDL_Keycode key)
{
	BindInput b;
	b.device = BIND_DEVICE_KEYBOARD;
	b.code = (int)key;
	return b;
}

BindInput BindInput_mouse(int button)
{
	BindInput b;
	b.device = BIND_DEVICE_MOUSE;
	b.code = button;
	return b;
}

BindInput BindInput_none(void)
{
	BindInput b;
	b.device = BIND_DEVICE_NONE;
	b.code = 0;
	return b;
}

bool BindInput_equals(BindInput a, BindInput b)
{
	return a.device == b.device && a.code == b.code;
}

int Keybinds_find_conflict(BindInput binding, BindAction exclude)
{
	if (binding.device == BIND_DEVICE_NONE)
		return -1;

	for (int i = 0; i < BIND_COUNT; i++) {
		if (i == (int)exclude)
			continue;
		if (BindInput_equals(bindings[i], binding))
			return i;
	}
	return -1;
}

bool Keybinds_is_lmb_rebound(void)
{
	BindInput lmb = BindInput_mouse(SDL_BUTTON_LEFT);
	for (int i = 0; i < BIND_COUNT; i++) {
		if (BindInput_equals(bindings[i], lmb))
			return true;
	}
	return false;
}

void Keybinds_save(FILE *f)
{
	for (int i = 0; i < BIND_COUNT; i++) {
		fprintf(f, "%s %d %d\n",
			persist_keys[i],
			(int)bindings[i].device,
			bindings[i].code);
	}
}

void Keybinds_load_entry(const char *key, int device, int code)
{
	for (int i = 0; i < BIND_COUNT; i++) {
		if (strcmp(key, persist_keys[i]) == 0) {
			bindings[i].device = (BindDevice)device;
			bindings[i].code = code;
			return;
		}
	}
}
