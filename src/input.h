#ifndef INPUT_H
#define INPUT_H

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
} Input;

#endif