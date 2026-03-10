#ifndef BOSS_HUD_H
#define BOSS_HUD_H

#include <stdbool.h>

void BossHUD_set_active(bool active);
void BossHUD_set_health(double current, double max);
void BossHUD_render(void);

#endif
