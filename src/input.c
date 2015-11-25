#include "input.h"

void input_initialize(Input *input)
{
	input->showMouse = true;
	input->mouseX = 0;
	input->mouseY = 0;
	input->mouseWheelUp = false;
	input->mouseWheelDown = false;
	input->mouseLeft = false;
	input->mouseMiddle = false;
	input->mouseRight = false;

	input->keyW = false;
	input->keyA = false;
	input->keyS = false;
	input->keyD = false;

	input->keyLShift = false;
	input->keyLControl = false;
}