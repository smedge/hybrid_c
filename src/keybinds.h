#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <stdbool.h>
#include <stdio.h>
#include <SDL2/SDL.h>

typedef enum {
	BIND_SLOT_1, BIND_SLOT_2, BIND_SLOT_3, BIND_SLOT_4, BIND_SLOT_5,
	BIND_SLOT_6, BIND_SLOT_7, BIND_SLOT_8, BIND_SLOT_9, BIND_SLOT_0,
	BIND_PRIMARY_WEAPON,
	BIND_SETTINGS, BIND_CATALOG, BIND_DATA_LOGS, BIND_MAP, BIND_GODMODE,
	BIND_MOVE_UP, BIND_MOVE_DOWN, BIND_MOVE_LEFT, BIND_MOVE_RIGHT,
	BIND_COUNT
} BindAction;

typedef enum {
	BIND_DEVICE_NONE,
	BIND_DEVICE_KEYBOARD,
	BIND_DEVICE_MOUSE,
} BindDevice;

typedef struct {
	BindDevice device;
	int code;
} BindInput;

void Keybinds_initialize(void);
void Keybinds_update(void);

bool Keybinds_pressed(BindAction action);
bool Keybinds_held(BindAction action);

BindInput Keybinds_get_binding(BindAction action);
void Keybinds_set_binding(BindAction action, BindInput binding);
void Keybinds_clear_binding(BindAction action);
const char *Keybinds_get_binding_name(BindAction action);
const char *Keybinds_get_action_name(BindAction action);
void Keybinds_reset_defaults(void);

BindInput BindInput_key(SDL_Keycode key);
BindInput BindInput_mouse(int button);
BindInput BindInput_none(void);
bool BindInput_equals(BindInput a, BindInput b);

int Keybinds_find_conflict(BindInput binding, BindAction exclude);
bool Keybinds_is_lmb_rebound(void);

const char *Keybinds_input_name(BindInput b);

void Keybinds_save(FILE *f);
void Keybinds_load_entry(const char *key, int device, int code);

#endif
