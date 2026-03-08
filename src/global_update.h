#ifndef GLOBAL_UPDATE_H
#define GLOBAL_UPDATE_H

typedef void (*GlobalUpdateFunc)(const unsigned int ticks);

void GlobalUpdate_register_pre_collision(GlobalUpdateFunc func);
void GlobalUpdate_register_post_collision(GlobalUpdateFunc func);
void GlobalUpdate_pre_collision(const unsigned int ticks);
void GlobalUpdate_post_collision(const unsigned int ticks);
void GlobalUpdate_clear(void);

#endif
