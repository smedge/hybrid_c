#include "imgui.h"

void imgui_update_button(const Input *input,
						 ButtonState *buttonState, 
						 void (*on_click)()) 
{
	Rectangle boundingBox = {buttonState->position.x,
							 buttonState->position.y - buttonState->height,
							 buttonState->position.x + buttonState->width,
							 buttonState->position.y};

	if (collision_point_test(input->mouseX, input->mouseY, boundingBox)) {
		buttonState->hover = true;

		if (input->mouseLeft) {
			buttonState->active = true;
		}

		if (buttonState->active && !input->mouseLeft) {
			buttonState->active = false;
			on_click();
		}
	}
	else {
		buttonState->hover = false;
		buttonState->active = false;
	}
}