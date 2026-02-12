#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
	bool showMouse;
	int mouseX;
	int mouseY;
	bool mouseWheelUp;
	bool mouseWheelDown;
	bool mouseLeft;
	bool mouseMiddle;
	bool mouseRight;

	bool keyW;
	bool keyA;
	bool keyS;
	bool keyD;

	bool keyLShift;
	bool keyLControl;

	bool keyG;
	bool keyZ;
	bool keyTab;
} Input;

void input_initialize(Input *input);

#endif