#include "zone.h"
#include "entity.h"
#include "procgen.h"
#include "mine.h"
#include "hunter.h"
#include "seeker.h"
#include "defender.h"
#include "stalker.h"
#include "portal.h"
#include "savepoint.h"
#include "background.h"
#include "enemy_registry.h"
#include "fog_of_war.h"
#include "spatial_grid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Undo system --- */

typedef enum {
	UNDO_PLACE_CELL,
	UNDO_REMOVE_CELL,
	UNDO_PLACE_SPAWN,
	UNDO_REMOVE_SPAWN,
	UNDO_PLACE_PORTAL,
	UNDO_REMOVE_PORTAL,
	UNDO_PLACE_SAVEPOINT,
	UNDO_REMOVE_SAVEPOINT
} UndoType;

typedef struct {
	UndoType type;
	int grid_x, grid_y;
	int cell_type_index;
	ZoneSpawn spawn;
	int spawn_index;
	ZonePortal portal;
	int portal_index;
	ZoneSavepoint savepoint;
	int savepoint_index;
} UndoEntry;

static Zone zone;
static UndoEntry undoStack[ZONE_MAX_UNDO];
static int undoCount = 0;
static bool zoneDirty = false;

static int find_cell_type(const char *id);
static void apply_zone_to_world(void);
static void rebuild_enemies(void);
static void respawn_portals(void);
static void respawn_savepoints(void);
static void push_undo(UndoEntry entry);

/* --- Loading --- */

void Zone_load(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		printf("Zone_load: failed to open '%s'\n", path);
		return;
	}

	/* Reset zone state */
	memset(&zone, 0, sizeof(zone));
	for (int x = 0; x < MAP_SIZE; x++)
		for (int y = 0; y < MAP_SIZE; y++)
			zone.cell_grid[x][y] = -1;

	zone.size = MAP_SIZE;
	strncpy(zone.filepath, path, sizeof(zone.filepath) - 1);
	undoCount = 0;

	/* Noise param defaults (set before parsing so 0.0 isn't a sentinel) */
	zone.noise_octaves = 5;
	zone.noise_frequency = 0.01;
	zone.noise_lacunarity = 2.0;
	zone.noise_persistence = 0.5;
	zone.noise_wall_threshold = -0.1;

	/* Hotspot defaults */
	zone.hotspot_count = 10;
	zone.hotspot_edge_margin = 80;
	zone.hotspot_min_separation = 150;
	zone.landmark_min_separation = 120;

	/* Obstacle scatter defaults */
	zone.obstacle_density = 0.08f;
	zone.obstacle_min_spacing = 8;

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		/* Strip newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
		if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

		/* Skip empty lines and comments */
		if (len == 0 || line[0] == '#' ||
		    (len >= 2 && line[0] == '/' && line[1] == '/'))
			continue;

		if (strncmp(line, "name ", 5) == 0) {
			strncpy(zone.name, line + 5, sizeof(zone.name) - 1);
		}
		else if (strncmp(line, "size ", 5) == 0) {
			sscanf(line + 5, "%d", &zone.size);
		}
		else if (strncmp(line, "celltype ", 9) == 0) {
			if (zone.cell_type_count >= ZONE_MAX_CELL_TYPES) continue;

			ZoneCellType *ct = &zone.cell_types[zone.cell_type_count];
			int pr, pg, pb, pa, or_, og, ob, oa;
			int n = sscanf(line + 9, "%31s %d %d %d %d %d %d %d %d %15s",
				ct->id, &pr, &pg, &pb, &pa, &or_, &og, &ob, &oa, ct->pattern);
			if (n >= 9) {
				ct->primaryColor = (ColorRGB){pr, pg, pb, pa};
				ct->outlineColor = (ColorRGB){or_, og, ob, oa};
				if (n < 10)
					strcpy(ct->pattern, "none");
				/* Track as wall type for procgen */
				if (zone.wall_type_count < ZONE_MAX_CELL_TYPES)
					zone.wall_type_indices[zone.wall_type_count++] = zone.cell_type_count;
				zone.cell_type_count++;
			}
		}
		else if (strncmp(line, "cell ", 5) == 0) {
			int gx, gy;
			char type_id[32];
			char drop[64] = "";
			int n = sscanf(line + 5, "%d %d %31s %63s", &gx, &gy, type_id, drop);
			if (n >= 3) {
				int idx = find_cell_type(type_id);
				if (idx >= 0 && gx >= 0 && gx < MAP_SIZE && gy >= 0 && gy < MAP_SIZE) {
					zone.cell_grid[gx][gy] = idx;
					zone.cell_hand_placed[gx][gy] = true;
					if (n >= 4 && strncmp(drop, "drop:", 5) == 0 &&
					    zone.destructible_count < ZONE_MAX_DESTRUCTIBLES) {
						ZoneDestructible *d = &zone.destructibles[zone.destructible_count++];
						d->grid_x = gx;
						d->grid_y = gy;
						strncpy(d->drop_sub, drop + 5, sizeof(d->drop_sub) - 1);
						d->drop_sub[sizeof(d->drop_sub) - 1] = '\0';
					}
				}
			}
		}
		else if (strncmp(line, "clearcell ", 10) == 0) {
			int gx, gy;
			if (sscanf(line + 10, "%d %d", &gx, &gy) == 2) {
				if (gx >= 0 && gx < MAP_SIZE && gy >= 0 && gy < MAP_SIZE) {
					zone.cell_hand_placed[gx][gy] = true;
					zone.cell_grid[gx][gy] = -1;
				}
			}
		}
		else if (strncmp(line, "spawn ", 6) == 0) {
			if (zone.spawn_count >= ZONE_MAX_SPAWNS) continue;

			ZoneSpawn *sp = &zone.spawns[zone.spawn_count];
			if (sscanf(line + 6, "%15s %lf %lf", sp->enemy_type, &sp->world_x, &sp->world_y) == 3) {
				sp->probability = 1.0f;
				zone.spawn_count++;
			}
		}
		else if (strncmp(line, "portal ", 7) == 0) {
			if (zone.portal_count >= ZONE_MAX_PORTALS) continue;

			ZonePortal *p = &zone.portals[zone.portal_count];
			if (sscanf(line + 7, "%d %d %31s %255s %31s",
				&p->grid_x, &p->grid_y, p->id, p->dest_zone, p->dest_portal_id) == 5) {
				if (strcmp(p->dest_zone, "-") == 0) p->dest_zone[0] = '\0';
				if (strcmp(p->dest_portal_id, "-") == 0) p->dest_portal_id[0] = '\0';
				zone.portal_count++;
			}
		}
		else if (strncmp(line, "savepoint ", 10) == 0) {
			if (zone.savepoint_count >= ZONE_MAX_SAVEPOINTS) continue;

			int gx, gy;
			char sid[32];
			if (sscanf(line + 10, "%d %d %31s", &gx, &gy, sid) == 3) {
				zone.savepoints[zone.savepoint_count].grid_x = gx;
				zone.savepoints[zone.savepoint_count].grid_y = gy;
				strncpy(zone.savepoints[zone.savepoint_count].id, sid, 31);
				zone.savepoints[zone.savepoint_count].id[31] = '\0';
				zone.savepoint_count++;
			}
		}
		else if (strncmp(line, "bgcolor ", 8) == 0) {
			int idx, r, g, b;
			if (sscanf(line + 8, "%d %d %d %d", &idx, &r, &g, &b) == 4) {
				if (idx >= 0 && idx < 4) {
					zone.bg_colors[idx] = (ColorRGB){r, g, b, 255};
					zone.has_bg_colors = true;
				}
			}
		}
		else if (strncmp(line, "music ", 6) == 0) {
			if (zone.music_count < ZONE_MAX_MUSIC) {
				strncpy(zone.music_paths[zone.music_count], line + 6, 255);
				zone.music_paths[zone.music_count][255] = '\0';
				zone.music_count++;
			}
		}
		else if (strncmp(line, "procgen ", 8) == 0) {
			if (strncmp(line + 8, "true", 4) == 0)
				zone.procgen = true;
		}
		else if (strncmp(line, "noise_octaves ", 14) == 0) {
			sscanf(line + 14, "%d", &zone.noise_octaves);
		}
		else if (strncmp(line, "noise_frequency ", 16) == 0) {
			sscanf(line + 16, "%lf", &zone.noise_frequency);
		}
		else if (strncmp(line, "noise_lacunarity ", 17) == 0) {
			sscanf(line + 17, "%lf", &zone.noise_lacunarity);
		}
		else if (strncmp(line, "noise_persistence ", 18) == 0) {
			sscanf(line + 18, "%lf", &zone.noise_persistence);
		}
		else if (strncmp(line, "noise_wall_threshold ", 21) == 0) {
			sscanf(line + 21, "%lf", &zone.noise_wall_threshold);
		}
		else if (strncmp(line, "hotspot_count ", 14) == 0) {
			sscanf(line + 14, "%d", &zone.hotspot_count);
		}
		else if (strncmp(line, "hotspot_edge_margin ", 20) == 0) {
			sscanf(line + 20, "%d", &zone.hotspot_edge_margin);
		}
		else if (strncmp(line, "hotspot_min_separation ", 23) == 0) {
			sscanf(line + 23, "%d", &zone.hotspot_min_separation);
		}
		else if (strncmp(line, "landmark_min_separation ", 24) == 0) {
			sscanf(line + 24, "%d", &zone.landmark_min_separation);
		}
		else if (strncmp(line, "landmark_portal ", 16) == 0) {
			/* landmark_portal <landmark_type> <portal_id> <dest_zone> <dest_portal_id> */
			char lm_type[32], pid[32], dzone[256], dpid[32];
			if (sscanf(line + 16, "%31s %31s %255s %31s",
			           lm_type, pid, dzone, dpid) == 4) {
				/* Find matching landmark def */
				for (int i = zone.landmark_count - 1; i >= 0; i--) {
					if (strcmp(zone.landmarks[i].type, lm_type) == 0) {
						LandmarkDef *lm = &zone.landmarks[i];
						if (lm->portal_count < LANDMARK_MAX_PORTALS) {
							LandmarkPortalWiring *w = &lm->portals[lm->portal_count++];
							strncpy(w->portal_id, pid, 31);
							w->portal_id[31] = '\0';
							strncpy(w->dest_zone, dzone, 255);
							w->dest_zone[255] = '\0';
							strncpy(w->dest_portal_id, dpid, 31);
							w->dest_portal_id[31] = '\0';
							if (strcmp(w->dest_zone, "-") == 0) w->dest_zone[0] = '\0';
							if (strcmp(w->dest_portal_id, "-") == 0) w->dest_portal_id[0] = '\0';
						}
						break;
					}
				}
			}
		}
		else if (strncmp(line, "landmark_savepoint ", 19) == 0) {
			/* landmark_savepoint <landmark_type> <savepoint_id> */
			char lm_type[32], sid[32];
			if (sscanf(line + 19, "%31s %31s", lm_type, sid) == 2) {
				for (int i = zone.landmark_count - 1; i >= 0; i--) {
					if (strcmp(zone.landmarks[i].type, lm_type) == 0) {
						LandmarkDef *lm = &zone.landmarks[i];
						if (lm->savepoint_count < LANDMARK_MAX_SAVEPOINTS) {
							LandmarkSavepointWiring *w = &lm->savepoints[lm->savepoint_count++];
							strncpy(w->savepoint_id, sid, 31);
							w->savepoint_id[31] = '\0';
						}
						break;
					}
				}
			}
		}
		else if (strncmp(line, "landmark ", 9) == 0) {
			if (zone.landmark_count >= ZONE_MAX_LANDMARKS) continue;
			LandmarkDef *lm = &zone.landmarks[zone.landmark_count];
			memset(lm, 0, sizeof(*lm));
			char inf_type_str[32] = "";
			int n = sscanf(line + 9, "%31s %255s %d %31s %f %f %f",
			               lm->type, lm->chunk_path, &lm->priority,
			               inf_type_str, &lm->influence.radius,
			               &lm->influence.strength, &lm->influence.falloff);
			if (n >= 7) {
				if (strcmp(inf_type_str, "dense") == 0)
					lm->influence.type = INFLUENCE_DENSE;
				else if (strcmp(inf_type_str, "sparse") == 0)
					lm->influence.type = INFLUENCE_SPARSE;
				else
					lm->influence.type = INFLUENCE_MODERATE;
				zone.landmark_count++;
			}
		}
		else if (strncmp(line, "obstacle_density ", 17) == 0) {
			sscanf(line + 17, "%f", &zone.obstacle_density);
		}
		else if (strncmp(line, "obstacle_min_spacing ", 21) == 0) {
			sscanf(line + 21, "%d", &zone.obstacle_min_spacing);
		}
		else if (strncmp(line, "obstacle ", 9) == 0) {
			if (zone.obstacle_def_count >= ZONE_MAX_OBSTACLE_DEFS) continue;
			ObstacleDef *od = &zone.obstacle_defs[zone.obstacle_def_count];
			if (sscanf(line + 9, "%63s %f", od->name, &od->weight) == 2) {
				if (od->weight > 0.0f)
					zone.obstacle_def_count++;
			}
		}
	}

	fclose(f);

	/* Record hand-placed counts before procgen adds more */
	zone.hand_portal_count = zone.portal_count;
	zone.hand_savepoint_count = zone.savepoint_count;
	zone.hand_spawn_count = zone.spawn_count;

	/* Generate procgen terrain before applying to world */
	if (zone.procgen)
		Procgen_generate(&zone);

	apply_zone_to_world();

	printf("Zone_load: loaded '%s' (%d cell types, %d spawns, %d portals, %d savepoints)\n",
		zone.name, zone.cell_type_count, zone.spawn_count, zone.portal_count, zone.savepoint_count);
}

void Zone_unload(void)
{
	SpatialGrid_clear();
	Map_clear();
	Map_clear_boundary_cell();
	Mine_cleanup();
	Hunter_cleanup();
	Seeker_cleanup();
	Defender_cleanup();
	Stalker_cleanup();
	EnemyRegistry_clear();
	Portal_cleanup();
	Savepoint_cleanup();
	Entity_recalculate_highest_index();
	memset(&zone, 0, sizeof(zone));
	undoCount = 0;
}

const Zone *Zone_get(void)
{
	return &zone;
}

/* --- Saving --- */

void Zone_save(void)
{
	if (zone.filepath[0] == '\0') return;

	FILE *f = fopen(zone.filepath, "w");
	if (!f) {
		printf("Zone_save: failed to open '%s' for writing\n", zone.filepath);
		return;
	}

	fprintf(f, "# Zone: %s\n", zone.name);
	fprintf(f, "name %s\n", zone.name);
	fprintf(f, "size %d\n", zone.size);

	/* Background colors */
	if (zone.has_bg_colors) {
		for (int i = 0; i < 4; i++) {
			fprintf(f, "bgcolor %d %d %d %d\n", i,
				zone.bg_colors[i].red, zone.bg_colors[i].green,
				zone.bg_colors[i].blue);
		}
	}

	/* Music playlist */
	for (int i = 0; i < zone.music_count; i++)
		fprintf(f, "music %s\n", zone.music_paths[i]);

	fprintf(f, "\n");

	/* Cell types */
	for (int i = 0; i < zone.cell_type_count; i++) {
		ZoneCellType *ct = &zone.cell_types[i];
		fprintf(f, "celltype %s %d %d %d %d %d %d %d %d %s\n",
			ct->id,
			ct->primaryColor.red, ct->primaryColor.green,
			ct->primaryColor.blue, ct->primaryColor.alpha,
			ct->outlineColor.red, ct->outlineColor.green,
			ct->outlineColor.blue, ct->outlineColor.alpha,
			ct->pattern);
	}
	fprintf(f, "\n");

	/* Procgen parameters */
	if (zone.procgen) {
		fprintf(f, "procgen true\n");
		fprintf(f, "noise_octaves %d\n", zone.noise_octaves);
		fprintf(f, "noise_frequency %g\n", zone.noise_frequency);
		fprintf(f, "noise_lacunarity %g\n", zone.noise_lacunarity);
		fprintf(f, "noise_persistence %g\n", zone.noise_persistence);
		fprintf(f, "noise_wall_threshold %g\n", zone.noise_wall_threshold);

		/* Hotspot params (only if non-default) */
		fprintf(f, "hotspot_count %d\n", zone.hotspot_count);
		fprintf(f, "hotspot_edge_margin %d\n", zone.hotspot_edge_margin);
		fprintf(f, "hotspot_min_separation %d\n", zone.hotspot_min_separation);
		fprintf(f, "landmark_min_separation %d\n", zone.landmark_min_separation);

		/* Landmarks */
		for (int i = 0; i < zone.landmark_count; i++) {
			const LandmarkDef *lm = &zone.landmarks[i];
			const char *inf_str = "moderate";
			if (lm->influence.type == INFLUENCE_DENSE) inf_str = "dense";
			else if (lm->influence.type == INFLUENCE_SPARSE) inf_str = "sparse";
			fprintf(f, "landmark %s %s %d %s %g %g %g\n",
			        lm->type, lm->chunk_path, lm->priority,
			        inf_str, lm->influence.radius,
			        lm->influence.strength, lm->influence.falloff);
			/* Portal wiring */
			for (int j = 0; j < lm->portal_count; j++) {
				const LandmarkPortalWiring *w = &lm->portals[j];
				fprintf(f, "landmark_portal %s %s %s %s\n",
				        lm->type, w->portal_id,
				        w->dest_zone[0] ? w->dest_zone : "-",
				        w->dest_portal_id[0] ? w->dest_portal_id : "-");
			}
			/* Savepoint wiring */
			for (int j = 0; j < lm->savepoint_count; j++) {
				fprintf(f, "landmark_savepoint %s %s\n",
				        lm->type, lm->savepoints[j].savepoint_id);
			}
		}

		/* Obstacle scatter */
		if (zone.obstacle_def_count > 0) {
			fprintf(f, "obstacle_density %g\n", zone.obstacle_density);
			fprintf(f, "obstacle_min_spacing %d\n", zone.obstacle_min_spacing);
			for (int i = 0; i < zone.obstacle_def_count; i++) {
				fprintf(f, "obstacle %s %g\n",
				        zone.obstacle_defs[i].name,
				        zone.obstacle_defs[i].weight);
			}
		}

		fprintf(f, "\n");
	}

	/* Cells — for procgen zones, only save hand-placed cells */
	for (int x = 0; x < MAP_SIZE; x++) {
		for (int y = 0; y < MAP_SIZE; y++) {
			if (zone.procgen && !zone.cell_hand_placed[x][y])
				continue;
			if (zone.cell_grid[x][y] >= 0) {
				int idx = zone.cell_grid[x][y];
				const ZoneDestructible *d = Zone_get_destructible(x, y);
				if (d)
					fprintf(f, "cell %d %d %s drop:%s\n", x, y,
						zone.cell_types[idx].id, d->drop_sub);
				else
					fprintf(f, "cell %d %d %s\n", x, y, zone.cell_types[idx].id);
			} else if (zone.procgen && zone.cell_hand_placed[x][y]) {
				/* Hand-cleared position — persist removal */
				fprintf(f, "clearcell %d %d\n", x, y);
			}
		}
	}
	fprintf(f, "\n");

	/* Spawns (hand-placed only — procgen spawns are regenerated from landmarks) */
	int save_spawn_count = zone.procgen ? zone.hand_spawn_count : zone.spawn_count;
	for (int i = 0; i < save_spawn_count; i++) {
		ZoneSpawn *sp = &zone.spawns[i];
		fprintf(f, "spawn %s %.1f %.1f\n", sp->enemy_type, sp->world_x, sp->world_y);
	}

	/* Portals (hand-placed only) */
	int save_portal_count = zone.procgen ? zone.hand_portal_count : zone.portal_count;
	if (save_portal_count > 0)
		fprintf(f, "\n");
	for (int i = 0; i < save_portal_count; i++) {
		ZonePortal *p = &zone.portals[i];
		fprintf(f, "portal %d %d %s %s %s\n",
			p->grid_x, p->grid_y, p->id,
			p->dest_zone[0] ? p->dest_zone : "-",
			p->dest_portal_id[0] ? p->dest_portal_id : "-");
	}

	/* Save points (hand-placed only) */
	int save_savepoint_count = zone.procgen ? zone.hand_savepoint_count : zone.savepoint_count;
	if (save_savepoint_count > 0)
		fprintf(f, "\n");
	for (int i = 0; i < save_savepoint_count; i++) {
		fprintf(f, "savepoint %d %d %s\n",
			zone.savepoints[i].grid_x, zone.savepoints[i].grid_y,
			zone.savepoints[i].id);
	}

	fclose(f);
}

void Zone_save_if_dirty(void)
{
	if (!zoneDirty) return;
	zoneDirty = false;
	Zone_save();
}

/* --- Editing API --- */

void Zone_place_cell(int grid_x, int grid_y, const char *type_id)
{
	int idx = find_cell_type(type_id);
	if (idx < 0) return;
	if (grid_x < 0 || grid_x >= MAP_SIZE || grid_y < 0 || grid_y >= MAP_SIZE) return;

	/* Record undo */
	UndoEntry undo;
	undo.type = (zone.cell_grid[grid_x][grid_y] >= 0) ? UNDO_PLACE_CELL : UNDO_REMOVE_CELL;
	undo.grid_x = grid_x;
	undo.grid_y = grid_y;
	undo.cell_type_index = zone.cell_grid[grid_x][grid_y];
	push_undo(undo);

	zone.cell_grid[grid_x][grid_y] = idx;
	zone.cell_hand_placed[grid_x][grid_y] = true;

	/* Update world */
	ZoneCellType *ct = &zone.cell_types[idx];
	MapCell cell = {false, strcmp(ct->pattern, "circuit") == 0,
		ct->primaryColor, ct->outlineColor};
	Map_set_cell(grid_x, grid_y, &cell);

	zoneDirty = true;
}

void Zone_remove_cell(int grid_x, int grid_y)
{
	if (grid_x < 0 || grid_x >= MAP_SIZE || grid_y < 0 || grid_y >= MAP_SIZE) return;
	if (zone.cell_grid[grid_x][grid_y] < 0) return;

	UndoEntry undo;
	undo.type = UNDO_PLACE_CELL;
	undo.grid_x = grid_x;
	undo.grid_y = grid_y;
	undo.cell_type_index = zone.cell_grid[grid_x][grid_y];
	push_undo(undo);

	zone.cell_grid[grid_x][grid_y] = -1;
	if (zone.procgen)
		zone.cell_hand_placed[grid_x][grid_y] = true;
	Map_clear_cell(grid_x, grid_y);

	zoneDirty = true;
}

void Zone_place_spawn(const char *enemy_type, double world_x, double world_y)
{
	if (zone.spawn_count >= ZONE_MAX_SPAWNS) return;

	UndoEntry undo;
	undo.type = UNDO_REMOVE_SPAWN;
	undo.spawn_index = zone.spawn_count;
	push_undo(undo);

	ZoneSpawn *sp = &zone.spawns[zone.spawn_count];
	strncpy(sp->enemy_type, enemy_type, sizeof(sp->enemy_type) - 1);
	sp->world_x = world_x;
	sp->world_y = world_y;
	sp->probability = 1.0f;
	zone.spawn_count++;

	/* Spawn in world */
	if (strcmp(enemy_type, "mine") == 0) {
		Position pos = {world_x, world_y};
		Mine_initialize(pos);
	} else if (strcmp(enemy_type, "hunter") == 0) {
		Position pos = {world_x, world_y};
		Hunter_initialize(pos);
	} else if (strcmp(enemy_type, "seeker") == 0) {
		Position pos = {world_x, world_y};
		Seeker_initialize(pos);
	} else if (strcmp(enemy_type, "defender") == 0) {
		Position pos = {world_x, world_y};
		Defender_initialize(pos);
	} else if (strcmp(enemy_type, "stalker") == 0) {
		Position pos = {world_x, world_y};
		Stalker_initialize(pos);
	}

	zoneDirty = true;
}

void Zone_remove_spawn(int index)
{
	if (index < 0 || index >= zone.spawn_count) return;

	UndoEntry undo;
	undo.type = UNDO_PLACE_SPAWN;
	undo.spawn = zone.spawns[index];
	undo.spawn_index = index;
	push_undo(undo);

	/* Shift spawns down */
	for (int i = index; i < zone.spawn_count - 1; i++)
		zone.spawns[i] = zone.spawns[i + 1];
	zone.spawn_count--;

	rebuild_enemies();
	zoneDirty = true;
}

int Zone_find_spawn_near(double world_x, double world_y, double radius)
{
	double best_dist = radius * radius;
	int best_index = -1;
	for (int i = 0; i < zone.spawn_count; i++) {
		double dx = zone.spawns[i].world_x - world_x;
		double dy = zone.spawns[i].world_y - world_y;
		double d2 = dx * dx + dy * dy;
		if (d2 < best_dist) {
			best_dist = d2;
			best_index = i;
		}
	}
	return best_index;
}

void Zone_place_portal(int grid_x, int grid_y,
	const char *id, const char *dest_zone, const char *dest_portal_id)
{
	if (zone.portal_count >= ZONE_MAX_PORTALS) return;

	/* Check for duplicate at same grid position */
	for (int i = 0; i < zone.portal_count; i++) {
		if (zone.portals[i].grid_x == grid_x &&
		    zone.portals[i].grid_y == grid_y)
			return;
	}

	UndoEntry undo;
	undo.type = UNDO_REMOVE_PORTAL;
	undo.portal_index = zone.portal_count;
	push_undo(undo);

	ZonePortal *p = &zone.portals[zone.portal_count];
	p->grid_x = grid_x;
	p->grid_y = grid_y;
	strncpy(p->id, id, 31);
	p->id[31] = '\0';
	strncpy(p->dest_zone, dest_zone, 255);
	p->dest_zone[255] = '\0';
	strncpy(p->dest_portal_id, dest_portal_id, 31);
	p->dest_portal_id[31] = '\0';
	zone.portal_count++;

	/* Spawn in world */
	double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	Position pos = {wx, wy};
	Portal_initialize(pos, id, dest_zone, dest_portal_id);

	zoneDirty = true;
}

void Zone_remove_portal(int grid_x, int grid_y)
{
	int index = -1;
	for (int i = 0; i < zone.portal_count; i++) {
		if (zone.portals[i].grid_x == grid_x &&
		    zone.portals[i].grid_y == grid_y) {
			index = i;
			break;
		}
	}
	if (index < 0) return;

	UndoEntry undo;
	undo.type = UNDO_PLACE_PORTAL;
	undo.portal = zone.portals[index];
	undo.portal_index = index;
	push_undo(undo);

	for (int i = index; i < zone.portal_count - 1; i++)
		zone.portals[i] = zone.portals[i + 1];
	zone.portal_count--;

	respawn_portals();
	zoneDirty = true;
}

void Zone_place_savepoint(int grid_x, int grid_y, const char *id)
{
	if (zone.savepoint_count >= ZONE_MAX_SAVEPOINTS) return;

	/* Check for duplicate at same grid position */
	for (int i = 0; i < zone.savepoint_count; i++) {
		if (zone.savepoints[i].grid_x == grid_x &&
		    zone.savepoints[i].grid_y == grid_y)
			return;
	}

	UndoEntry undo;
	undo.type = UNDO_REMOVE_SAVEPOINT;
	undo.savepoint_index = zone.savepoint_count;
	push_undo(undo);

	ZoneSavepoint *sp = &zone.savepoints[zone.savepoint_count];
	sp->grid_x = grid_x;
	sp->grid_y = grid_y;
	strncpy(sp->id, id, 31);
	sp->id[31] = '\0';
	zone.savepoint_count++;

	/* Spawn in world */
	double wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	double wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	Position pos = {wx, wy};
	Savepoint_initialize(pos, id);

	zoneDirty = true;
}

void Zone_remove_savepoint(int grid_x, int grid_y)
{
	int index = -1;
	for (int i = 0; i < zone.savepoint_count; i++) {
		if (zone.savepoints[i].grid_x == grid_x &&
		    zone.savepoints[i].grid_y == grid_y) {
			index = i;
			break;
		}
	}
	if (index < 0) return;

	UndoEntry undo;
	undo.type = UNDO_PLACE_SAVEPOINT;
	undo.savepoint = zone.savepoints[index];
	undo.savepoint_index = index;
	push_undo(undo);

	for (int i = index; i < zone.savepoint_count - 1; i++)
		zone.savepoints[i] = zone.savepoints[i + 1];
	zone.savepoint_count--;

	respawn_savepoints();
	zoneDirty = true;
}

void Zone_undo(void)
{
	if (undoCount <= 0) return;

	UndoEntry undo = undoStack[--undoCount];

	switch (undo.type) {
	case UNDO_PLACE_CELL:
		/* Restore previous cell type directly — no full world rebuild */
		zone.cell_grid[undo.grid_x][undo.grid_y] = undo.cell_type_index;
		{
			ZoneCellType *ct = &zone.cell_types[undo.cell_type_index];
			MapCell cell = {false, strcmp(ct->pattern, "circuit") == 0,
				ct->primaryColor, ct->outlineColor};
			Map_set_cell(undo.grid_x, undo.grid_y, &cell);
		}
		break;
	case UNDO_REMOVE_CELL:
		/* Cell was empty before — clear it directly */
		zone.cell_grid[undo.grid_x][undo.grid_y] = -1;
		Map_clear_cell(undo.grid_x, undo.grid_y);
		break;
	case UNDO_PLACE_SPAWN:
		/* Re-insert spawn at original index */
		if (zone.spawn_count < ZONE_MAX_SPAWNS) {
			for (int i = zone.spawn_count; i > undo.spawn_index; i--)
				zone.spawns[i] = zone.spawns[i - 1];
			zone.spawns[undo.spawn_index] = undo.spawn;
			zone.spawn_count++;
		}
		rebuild_enemies();
		break;
	case UNDO_REMOVE_SPAWN:
		/* Remove last spawn */
		if (zone.spawn_count > undo.spawn_index)
			zone.spawn_count = undo.spawn_index;
		rebuild_enemies();
		break;
	case UNDO_PLACE_PORTAL:
		/* Re-insert portal at original index */
		if (zone.portal_count < ZONE_MAX_PORTALS) {
			for (int i = zone.portal_count; i > undo.portal_index; i--)
				zone.portals[i] = zone.portals[i - 1];
			zone.portals[undo.portal_index] = undo.portal;
			zone.portal_count++;
		}
		respawn_portals();
		break;
	case UNDO_REMOVE_PORTAL:
		/* Remove last portal */
		if (zone.portal_count > undo.portal_index)
			zone.portal_count = undo.portal_index;
		respawn_portals();
		break;
	case UNDO_PLACE_SAVEPOINT:
		/* Re-insert savepoint at original index */
		if (zone.savepoint_count < ZONE_MAX_SAVEPOINTS) {
			for (int i = zone.savepoint_count; i > undo.savepoint_index; i--)
				zone.savepoints[i] = zone.savepoints[i - 1];
			zone.savepoints[undo.savepoint_index] = undo.savepoint;
			zone.savepoint_count++;
		}
		respawn_savepoints();
		break;
	case UNDO_REMOVE_SAVEPOINT:
		/* Remove last savepoint */
		if (zone.savepoint_count > undo.savepoint_index)
			zone.savepoint_count = undo.savepoint_index;
		respawn_savepoints();
		break;
	}

	zoneDirty = true;
}

const ZoneDestructible *Zone_get_destructible(int grid_x, int grid_y)
{
	for (int i = 0; i < zone.destructible_count; i++) {
		if (zone.destructibles[i].grid_x == grid_x &&
		    zone.destructibles[i].grid_y == grid_y)
			return &zone.destructibles[i];
	}
	return NULL;
}

/* --- Procgen regeneration --- */

void Zone_regenerate_procgen(void)
{
	if (!zone.procgen) return;

	/* Reset non-hand-placed cells to empty, clear chunk stamps */
	for (int x = 0; x < zone.size; x++) {
		for (int y = 0; y < zone.size; y++) {
			zone.cell_chunk_stamped[x][y] = false;
			if (!zone.cell_hand_placed[x][y])
				zone.cell_grid[x][y] = -1;
		}
	}

	/* Truncate procgen-placed portals/savepoints/spawns */
	zone.portal_count = zone.hand_portal_count;
	zone.savepoint_count = zone.hand_savepoint_count;
	zone.spawn_count = zone.hand_spawn_count;

	/* Regenerate from current master seed */
	Procgen_generate(&zone);

	/* Rebuild world (does NOT set zoneDirty — this is a preview, not an edit) */
	apply_zone_to_world();
	FogOfWar_reset_active();
	Zone_spawn_enemies();
}

/* --- Internal --- */

static int find_cell_type(const char *id)
{
	for (int i = 0; i < zone.cell_type_count; i++) {
		if (strcmp(zone.cell_types[i].id, id) == 0)
			return i;
	}
	return -1;
}

/* Editor rebuild: tear down existing enemies and respawn from zone data */
static void rebuild_enemies(void)
{
	Mine_cleanup();
	Hunter_cleanup();
	Seeker_cleanup();
	Defender_cleanup();
	Stalker_cleanup();
	EnemyRegistry_clear();
	Entity_recalculate_highest_index();
	Zone_spawn_enemies();
}

/* Spawn enemy entities from zone spawn data */
void Zone_spawn_enemies(void)
{
	for (int i = 0; i < zone.spawn_count; i++) {
		ZoneSpawn *sp = &zone.spawns[i];

		/* Roll probability — hand-placed spawns are 1.0 (always pass) */
		if (sp->probability < 1.0f) {
			float roll = (float)rand() / (float)RAND_MAX;
			if (roll > sp->probability)
				continue;
		}

		Position pos = {sp->world_x, sp->world_y};
		if (strcmp(sp->enemy_type, "mine") == 0)
			Mine_initialize(pos);
		else if (strcmp(sp->enemy_type, "hunter") == 0)
			Hunter_initialize(pos);
		else if (strcmp(sp->enemy_type, "seeker") == 0)
			Seeker_initialize(pos);
		else if (strcmp(sp->enemy_type, "defender") == 0)
			Defender_initialize(pos);
		else if (strcmp(sp->enemy_type, "stalker") == 0)
			Stalker_initialize(pos);
	}
}

/* Lightweight rebuild: only portals */
static void respawn_portals(void)
{
	Portal_cleanup();

	for (int i = 0; i < zone.portal_count; i++) {
		ZonePortal *p = &zone.portals[i];
		double wx = (p->grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		double wy = (p->grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Position pos = {wx, wy};
		Portal_initialize(pos, p->id, p->dest_zone, p->dest_portal_id);
	}
}

/* Lightweight rebuild: only savepoints */
static void respawn_savepoints(void)
{
	Savepoint_cleanup();

	for (int i = 0; i < zone.savepoint_count; i++) {
		double wx = (zone.savepoints[i].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		double wy = (zone.savepoints[i].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Position pos = {wx, wy};
		Savepoint_initialize(pos, zone.savepoints[i].id);
	}
}

static void apply_zone_to_world(void)
{
	Map_clear();
	Mine_cleanup();
	Hunter_cleanup();
	Seeker_cleanup();
	Defender_cleanup();
	Stalker_cleanup();
	EnemyRegistry_clear();
	Portal_cleanup();
	Savepoint_cleanup();
	Entity_recalculate_highest_index();

	/* Apply background palette */
	if (zone.has_bg_colors) {
		float colors[4][3];
		for (int i = 0; i < 4; i++) {
			colors[i][0] = zone.bg_colors[i].red / 255.0f;
			colors[i][1] = zone.bg_colors[i].green / 255.0f;
			colors[i][2] = zone.bg_colors[i].blue / 255.0f;
		}
		Background_set_palette(colors);
	} else {
		Background_reset_palette();
	}
	Background_initialize();

	/* Place cells */
	for (int x = 0; x < MAP_SIZE; x++) {
		for (int y = 0; y < MAP_SIZE; y++) {
			int idx = zone.cell_grid[x][y];
			if (idx < 0) continue;

			ZoneCellType *ct = &zone.cell_types[idx];
			MapCell cell = {false, strcmp(ct->pattern, "circuit") == 0,
				ct->primaryColor, ct->outlineColor};
			Map_set_cell(x, y, &cell);
		}
	}

	/* Enemies are NOT spawned here — callers use Zone_spawn_enemies() */

	/* Spawn portals */
	for (int i = 0; i < zone.portal_count; i++) {
		ZonePortal *p = &zone.portals[i];
		/* Convert grid coords to world position */
		double wx = (p->grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		double wy = (p->grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Position pos = {wx, wy};
		Portal_initialize(pos, p->id, p->dest_zone, p->dest_portal_id);
	}

	/* Spawn save points */
	for (int i = 0; i < zone.savepoint_count; i++) {
		double wx = (zone.savepoints[i].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		double wy = (zone.savepoints[i].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Position pos = {wx, wy};
		Savepoint_initialize(pos, zone.savepoints[i].id);
	}

	/* Set boundary cell from first cell type (solid walls) */
	if (zone.cell_type_count > 0) {
		ZoneCellType *ct = &zone.cell_types[0];
		MapCell boundary = {false, false, ct->primaryColor, ct->outlineColor};
		Map_set_boundary_cell(&boundary);
	}
}

static void push_undo(UndoEntry entry)
{
	if (undoCount >= ZONE_MAX_UNDO) {
		/* Shift stack to make room */
		memmove(&undoStack[0], &undoStack[1],
			(ZONE_MAX_UNDO - 1) * sizeof(UndoEntry));
		undoCount = ZONE_MAX_UNDO - 1;
	}
	undoStack[undoCount++] = entry;
}
