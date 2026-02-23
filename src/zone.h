#ifndef ZONE_H
#define ZONE_H

#include <stdbool.h>
#include "map.h"
#include "color.h"

#define ZONE_MAX_CELL_TYPES 32
#define ZONE_MAX_SPAWNS 65536
#define ZONE_MAX_UNDO 512
#define ZONE_MAX_DESTRUCTIBLES 32
#define ZONE_MAX_PORTALS 16
#define ZONE_MAX_SAVEPOINTS 16
#define ZONE_MAX_LANDMARKS 16
#define ZONE_MAX_OBSTACLE_DEFS 32
#define ZONE_MAX_LABELS 64

typedef enum {
	INFLUENCE_DENSE,
	INFLUENCE_MODERATE,
	INFLUENCE_SPARSE
} InfluenceType;

typedef struct {
	InfluenceType type;
	float radius;
	float strength;
	float falloff;
} TerrainInfluence;

#define LANDMARK_MAX_PORTALS 4
#define LANDMARK_MAX_SAVEPOINTS 4

typedef struct {
	char portal_id[32];
	char dest_zone[256];
	char dest_portal_id[32];
} LandmarkPortalWiring;

typedef struct {
	char savepoint_id[32];
} LandmarkSavepointWiring;

typedef struct {
	char type[32];
	char chunk_path[256];
	int priority;
	TerrainInfluence influence;

	LandmarkPortalWiring portals[LANDMARK_MAX_PORTALS];
	int portal_count;
	LandmarkSavepointWiring savepoints[LANDMARK_MAX_SAVEPOINTS];
	int savepoint_count;
} LandmarkDef;

typedef struct {
	int grid_x, grid_y;
	char drop_sub[32];
} ZoneDestructible;

typedef struct {
	char name[64];
	float weight;
} ObstacleDef;

typedef struct {
	int grid_x, grid_y;
	char text[64];
} ZoneLabel;

typedef struct {
	char id[32];
	ColorRGB primaryColor;
	ColorRGB outlineColor;
	char pattern[16];
} ZoneCellType;

typedef struct {
	char enemy_type[16];
	double world_x, world_y;
	float probability;
} ZoneSpawn;

typedef struct {
	int grid_x, grid_y;
	char id[32];
	char dest_zone[256];
	char dest_portal_id[32];
} ZonePortal;

typedef struct {
	int grid_x, grid_y;
	char id[32];
} ZoneSavepoint;

#define ZONE_MAX_MUSIC 3

typedef struct {
	char name[64];
	int size;
	char filepath[256];

	ZoneCellType cell_types[ZONE_MAX_CELL_TYPES];
	int cell_type_count;

	int cell_grid[MAP_SIZE][MAP_SIZE];

	ZoneSpawn spawns[ZONE_MAX_SPAWNS];
	int spawn_count;

	ZoneDestructible destructibles[ZONE_MAX_DESTRUCTIBLES];
	int destructible_count;

	ZonePortal portals[ZONE_MAX_PORTALS];
	int portal_count;

	ZoneSavepoint savepoints[ZONE_MAX_SAVEPOINTS];
	int savepoint_count;

	bool has_bg_colors;
	ColorRGB bg_colors[4];

	char music_paths[ZONE_MAX_MUSIC][256];
	int music_count;

	/* Procgen fields */
	bool procgen;
	int noise_octaves;
	double noise_frequency;
	double noise_lacunarity;
	double noise_persistence;
	double noise_wall_threshold;
	bool cell_hand_placed[MAP_SIZE][MAP_SIZE];
	bool cell_chunk_stamped[MAP_SIZE][MAP_SIZE];
	int wall_type_indices[ZONE_MAX_CELL_TYPES];
	int wall_type_count;

	/* Hotspot generation params */
	int hotspot_count;
	int hotspot_edge_margin;
	int hotspot_min_separation;

	/* Landmarks */
	LandmarkDef landmarks[ZONE_MAX_LANDMARKS];
	int landmark_count;
	int landmark_min_separation;

	/* Obstacle scatter */
	ObstacleDef obstacle_defs[ZONE_MAX_OBSTACLE_DEFS];
	int obstacle_def_count;
	float obstacle_density;
	int obstacle_min_spacing;

	/* Labels (godmode designer annotations) */
	ZoneLabel labels[ZONE_MAX_LABELS];
	int label_count;

	/* Hand-placed counts (for procgen regen — truncate back to these) */
	int hand_portal_count;
	int hand_savepoint_count;
	int hand_spawn_count;
} Zone;

void Zone_load(const char *path);
void Zone_unload(void);
void Zone_save(void);
void Zone_save_if_dirty(void);
const Zone *Zone_get(void);

/* Editing API (for God Mode) */
void Zone_place_cell(int grid_x, int grid_y, const char *type_id);
void Zone_remove_cell(int grid_x, int grid_y);
void Zone_place_spawn(const char *enemy_type, double world_x, double world_y);
void Zone_remove_spawn(int index);
int Zone_find_spawn_near(double world_x, double world_y, double radius);
void Zone_place_portal(int grid_x, int grid_y,
	const char *id, const char *dest_zone, const char *dest_portal_id);
void Zone_remove_portal(int grid_x, int grid_y);
void Zone_place_savepoint(int grid_x, int grid_y, const char *id);
void Zone_remove_savepoint(int grid_x, int grid_y);
void Zone_place_label(int grid_x, int grid_y, const char *text);
void Zone_remove_label(int grid_x, int grid_y);
void Zone_undo(void);

const ZoneDestructible *Zone_get_destructible(int grid_x, int grid_y);

/* Enemy lifecycle (for deferred spawning during rebirth) */
void Zone_spawn_enemies(void);

/* Procgen: regenerate non-hand-placed cells from current master seed */
void Zone_regenerate_procgen(void);

#endif
