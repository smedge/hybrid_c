#ifndef CURSOR_H
#define CURSOR_H

#include "input.h"
#include "mat4.h"

void cursor_update(const Input *input);
void cursor_render(const Mat4 *projection, const Mat4 *view);

#endif
