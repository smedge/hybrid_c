#ifndef SUB_MINE_CORE_H
#define SUB_MINE_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"

typedef enum {
	MINE_PHASE_DORMANT,
	MINE_PHASE_ARMED,
	MINE_PHASE_EXPLODING,
	MINE_PHASE_DEAD
} MinePhase;

typedef struct {
	MinePhase phase;
	Position position;
	unsigned int phaseTicks;
	unsigned int blinkTimer;
} SubMineCore;

typedef struct {
	unsigned int armed_duration_ms;
	unsigned int exploding_duration_ms;
	unsigned int dead_duration_ms;
	float explosion_half_size;
	float body_half_size;
	/* Visual colors (as float RGBA) */
	float body_r, body_g, body_b, body_a;
	float blink_r, blink_g, blink_b, blink_a;
	float explosion_r, explosion_g, explosion_b, explosion_a;
	/* Light */
	float light_armed_radius;
	float light_armed_r, light_armed_g, light_armed_b, light_armed_a;
	float light_explode_radius;
	float light_explode_r, light_explode_g, light_explode_b, light_explode_a;
} SubMineConfig;

void SubMine_initialize_audio(void);
void SubMine_cleanup_audio(void);

void SubMine_init(SubMineCore *core);
bool SubMine_arm(SubMineCore *core, const SubMineConfig *cfg, Position pos);
void SubMine_detonate(SubMineCore *core);
MinePhase SubMine_update(SubMineCore *core, const SubMineConfig *cfg, unsigned int ticks);
bool SubMine_check_explosion_hit(const SubMineCore *core, const SubMineConfig *cfg, Rectangle target);
bool SubMine_is_active(const SubMineCore *core);
bool SubMine_is_exploding(const SubMineCore *core);
MinePhase SubMine_get_phase(const SubMineCore *core);

void SubMine_render(const SubMineCore *core, const SubMineConfig *cfg);
void SubMine_render_bloom(const SubMineCore *core, const SubMineConfig *cfg);
void SubMine_render_light(const SubMineCore *core, const SubMineConfig *cfg);

#endif
