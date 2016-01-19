#include "imgui.h"

ButtonState imgui_update_button(const Input *input,
						 		ButtonState *state, 
						 		void (*on_click)()) 
{
	ButtonState newState;
	newState = *state;
	Rectangle boundingBox = {state->position.x,
							 state->position.y - state->height,
							 state->position.x + state->width,
							 state->position.y};

	if (Collision_point_test(input->mouseX, input->mouseY, boundingBox)) {
		newState.hover = true;

		if (input->mouseLeft) {
			newState.active = true;
		}

		if (newState.active && !input->mouseLeft) {
			newState.active = false;
			on_click();
		}
	}
	else {
		newState.hover = false;
		newState.active = false;
	}

	return newState;
}