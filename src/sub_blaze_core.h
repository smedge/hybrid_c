#ifndef SUB_BLAZE_CORE_H
#define SUB_BLAZE_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"
#include "burn.h"

/* A single corridor segment left behind during a blaze dash */
typedef struct {
	bool active;
	Position position;
	int life_ms;
	int burn_tick_ms;   /* countdown per-segment for interval burn */
	BurnState burn;     /* for visual registration with burn system */
} BlazeCorridorSegment;

/* Config — single source of truth for corridor tuning */
typedef struct {
	int corridor_life_ms;       /* how long segments persist */
	double corridor_radius;     /* collision/render radius */
	int burn_interval_ms;       /* min ms between burns per segment */
	int segment_spawn_ms;       /* ms between segment deposits during dash */
} SubBlazeConfig;

/* Runtime state — points to an external segment buffer */
typedef struct {
	BlazeCorridorSegment *segments;
	int max_segments;
	int spawn_timer;
} SubBlazeCore;

const SubBlazeConfig *SubBlaze_get_config(void);

void SubBlaze_init(SubBlazeCore *core, BlazeCorridorSegment *buffer, int max_segments);
void SubBlaze_spawn_segment(SubBlazeCore *core, Position pos);
void SubBlaze_update_corridor(SubBlazeCore *core, const SubBlazeConfig *cfg, unsigned int ticks);
int SubBlaze_check_corridor_burn(SubBlazeCore *core, const SubBlazeConfig *cfg, Rectangle target);
void SubBlaze_deactivate_all(SubBlazeCore *core);

/* Rendering */
void SubBlaze_render_corridor(const SubBlazeCore *core, const SubBlazeConfig *cfg);
void SubBlaze_render_corridor_bloom_source(const SubBlazeCore *core, const SubBlazeConfig *cfg);
void SubBlaze_render_corridor_light_source(const SubBlazeCore *core, const SubBlazeConfig *cfg);

#endif
