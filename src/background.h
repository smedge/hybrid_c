#ifndef BACKGROUND_H
#define BACKGROUND_H

void Background_initialize(void);
void Background_update(unsigned int ticks);
void Background_render(void);
void Background_set_palette(const float colors[4][3]);
void Background_reset_palette(void);

#endif
