#ifndef SAVEPOINT_H
#define SAVEPOINT_H

#include "position.h"
#include "entity.h"
#include "progression.h"
#include "skillbar.h"
#include "fragment.h"
#include "screen.h"
#include <stdbool.h>
#include <stdint.h>

#define SAVEPOINT_COUNT 16
#define SAVEPOINT_SIZE 100.0
#define SAVEPOINT_HALF_SIZE (SAVEPOINT_SIZE * 0.5)

#define SAVE_FILE_PATH "./save/checkpoint.sav"

typedef struct {
	bool valid;
	char zone_path[256];
	char savepoint_id[32];
	Position position;
	uint32_t procgen_seed;
	bool unlocked[SUB_ID_COUNT];
	bool discovered[SUB_ID_COUNT];
	int fragment_counts[FRAG_TYPE_COUNT];
	SkillbarSnapshot skillbar;
} SaveCheckpoint;

void Savepoint_initialize(Position position, const char *id);
void Savepoint_cleanup(void);
void Savepoint_update_all(unsigned int ticks);
void Savepoint_render(const void *state, const PlaceableComponent *placeable);
void Savepoint_render_bloom_source(void);
void Savepoint_render_god_labels(void);
void Savepoint_render_notification(const Screen *screen);

/* Checkpoint API */
const SaveCheckpoint *Savepoint_get_checkpoint(void);

/* Seed an initial in-memory checkpoint (no disk write).
 * Used when starting a new game to give the player a valid
 * respawn point before they trigger a physical savepoint. */
void Savepoint_seed_checkpoint(const char *zone_path, const char *savepoint_id,
                               Position position);

/* Suppress arrival (start deactivated) */
void Savepoint_suppress_by_id(const char *savepoint_id);

/* Disk persistence */
bool Savepoint_has_save_file(void);
bool Savepoint_load_from_disk(void);
void Savepoint_delete_save_file(void);

/* Minimap rendering */
void Savepoint_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range);

#endif
