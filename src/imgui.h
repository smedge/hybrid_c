#ifndef IMGUI_H
#define IMGUI_H

#include "collision.h"
#include "input.h"

typedef struct {
	Rectangle boundingBox;
	bool hover;
	bool active;
} ButtonState;

void imgui_update_button(const Input *input, 
						 ButtonState *buttonState, 
						 void (*on_click)()); 

#endif