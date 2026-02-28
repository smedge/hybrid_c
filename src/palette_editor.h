#ifndef PALETTE_EDITOR_H
#define PALETTE_EDITOR_H

#include "input.h"
#include "screen.h"

void PaletteEditor_enter(void);
void PaletteEditor_exit(void);
void PaletteEditor_update(const Input *input, unsigned int ticks);
void PaletteEditor_render(const Screen *screen);

#endif
