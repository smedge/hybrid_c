#ifndef SUB_PEA_H
#define SUB_PEA_H

void Sub_Pea_initialize();
void Sub_Pea_cleanup();

void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Pea_render(const ScreenMetrics &screenMetrics, const Camera &camera);

#endif