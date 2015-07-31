#include "imgui.h"

void update_button(const Input *input,
					ButtonState *buttonState, 
					void (*on_click)()) 
{
	if (collision_point_test(input->mouseX, input->mouseY, 
								buttonState->boundingBox)) {
		buttonState->hover = true;

		// begin quitting on mouse down
		if (input->mouseLeft) {
			buttonState->active = true;
		}

		// quit on mouse up
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