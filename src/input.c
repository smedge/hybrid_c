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

	input->keyF = false;
	input->keyG = false;
	input->keyH = false;
	input->keyI = false;
	input->keyJ = false;
	input->keyL = false;
	input->keyN = false;
	input->keyO = false;
	input->keyX = false;
	input->keyZ = false;
	input->keyM = false;
	input->keyP = false;
	input->keyTab = false;
	input->keyE = false;
	input->keyQ = false;
	input->keySpace = false;
	input->keyEsc = false;
	input->keySlot = -1;
}