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

	bool keyF;
	bool keyG;
	bool keyH;
	bool keyJ;
	bool keyL;
	bool keyN;
	bool keyO;
	bool keyX;
	bool keyZ;
	bool keyI;
	bool keyM;
	bool keyP;
	bool keyV;
	bool keyTab;
	bool keyE;
	bool keyQ;
	bool keySpace;
	bool keyEsc;
	int keySlot;  /* -1 = none, 0-9 = skill bar slot pressed */

	/* Text input (SDL_TEXTINPUT mode) */
	bool textInputActive;
	char textInputBuffer[64];
	int textInputLen;
	bool textInputConfirmed;
	bool textInputCancelled;
} Input;

void input_initialize(Input *input);

#endif