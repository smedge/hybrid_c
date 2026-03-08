#ifndef GLOBAL_RENDER_H
#define GLOBAL_RENDER_H

#include "render_pass.h"

typedef void (*GlobalRenderFunc)(void);

void GlobalRender_register(RenderPass pass, GlobalRenderFunc func);
void GlobalRender_pass(RenderPass pass);
void GlobalRender_clear(void);
void GlobalRender_debug_counts(void);

#endif
