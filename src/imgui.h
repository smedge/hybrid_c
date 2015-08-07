#ifndef IMGUI_H
#define IMGUI_H

#include "collision.h"
#include "input.h"
#include "position.h"

typedef struct {
	Position position;
	int width, height;
	Rectangle boundingBox;
	bool hover, active;
	char* text;
} ButtonState;

void imgui_update_button(const Input *input, 
						 ButtonState *buttonState, 
						 void (*on_click)()); 

#endif