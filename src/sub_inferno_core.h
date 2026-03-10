#ifndef SUB_INFERNO_CORE_H
#define SUB_INFERNO_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"

/* --- Blob (individual fire particle) --- */

typedef struct {
	bool active;
	Position position;
	Position prevPosition;
	double vx, vy;
	int age;
	int ttl;
	float rotation;
	float sizeScale;
	bool mirror;
} InfernoBlob;

/* --- Config (shared tuning — one global instance) --- */

typedef struct {
	int spawn_interval_ms;
	double blob_speed;
	int blob_ttl_ms;
	float base_size;
	double spread_degrees;
	int sound_interval_ms;
} SubInfernoConfig;

/* --- State (one per beam instance) --- */

typedef struct {
	InfernoBlob *blobs;
	int blob_capacity;
	bool channeling;
	int spawn_timer;
	int sound_timer;
	Position origin;
	double aim_angle;    /* radians — same convention as sub_inferno heading */
} SubInfernoCoreState;

/* Shared config singleton */
const SubInfernoConfig *SubInferno_get_config(void);

/* Audio lifecycle — called once globally, not per state */
void SubInfernoCore_init_audio(void);
void SubInfernoCore_cleanup_audio(void);

/* Per-state lifecycle */
void SubInfernoCore_init(SubInfernoCoreState *state, InfernoBlob *blob_array, int capacity);

/* Caller sets these each frame before update */
void SubInfernoCore_set_origin(SubInfernoCoreState *state, Position pos);
void SubInfernoCore_set_aim_angle(SubInfernoCoreState *state, double radians);
void SubInfernoCore_set_channeling(SubInfernoCoreState *state, bool active);

/* Tick — spawns blobs when channeling, moves/ages/wall-kills all blobs.
   Pass NULL for cfg to use the default shared config. */
void SubInfernoCore_update(SubInfernoCoreState *state, const SubInfernoConfig *cfg, unsigned int ticks);

/* Rendering — pass NULL for cfg to use the default shared config. */
void SubInfernoCore_render(const SubInfernoCoreState *state, const SubInfernoConfig *cfg);
void SubInfernoCore_render_bloom(const SubInfernoCoreState *state, const SubInfernoConfig *cfg);
void SubInfernoCore_render_light(const SubInfernoCoreState *state);

/* Hit detection — piercing (blobs stay active on hit) */
bool SubInfernoCore_check_hit(const SubInfernoCoreState *state, Rectangle target);
bool SubInfernoCore_check_nearby(const SubInfernoCoreState *state, Position pos, double radius);

/* Kill all blobs, reset channeling */
void SubInfernoCore_deactivate_all(SubInfernoCoreState *state);

#endif
