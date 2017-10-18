#ifndef MODE_H
#define MODE_H

#include "input.h"

typedef enum {
	INTRO, MAINMENU, GAMEPLAY //, CREDITS
} Mode;

typedef struct {
	void (*initialize)(void (*quit)(void), void (*gameplay_mode)(void));
	void (*cleanup)(void);
	void (*update)(const Input *input, const unsigned int ticks);
	void (*render)(void);
} AppModeDispatch;

#endif