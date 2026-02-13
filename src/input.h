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
	bool keyP;
	bool keyTab;
	bool keySpace;
	bool keyEsc;
	int keySlot;  /* -1 = none, 0-9 = skill bar slot pressed */
} Input;

void input_initialize(Input *input);

#endif