#ifndef CONFIRM_DIALOG_H
#define CONFIRM_DIALOG_H

#include <stdbool.h>
#include "input.h"
#include "imgui.h"

typedef struct {
	bool open;
	bool confirmed;
	const char *message;
	ButtonState ok_button;
	ButtonState cancel_button;
} ConfirmDialog;

void ConfirmDialog_open(ConfirmDialog *d, const char *message);
void ConfirmDialog_update(ConfirmDialog *d, const Input *input);
void ConfirmDialog_render(const ConfirmDialog *d);

#endif
