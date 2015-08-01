#include "imgui.h"

void imgui_update_button(const Input *input,
						 ButtonState *buttonState, 
						 void (*on_click)()) 
{
	if (collision_point_test(input->mouseX, input->mouseY, 
								buttonState->boundingBox)) {
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