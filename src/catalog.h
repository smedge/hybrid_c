#ifndef CATALOG_H
#define CATALOG_H

#include <stdbool.h>
#include "input.h"
#include "screen.h"

void Catalog_initialize(void);
void Catalog_cleanup(void);
void Catalog_update(const Input *input, const unsigned int ticks);
void Catalog_render(const Screen *screen);
bool Catalog_is_open(void);
void Catalog_toggle(void);

#endif
