#include "zone.h"
#include "mine.h"
#include "portal.h"
#include "savepoint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Undo system --- */

typedef enum {
	UNDO_PLACE_CELL,
	UNDO_REMOVE_CELL,
	UNDO_PLACE_SPAWN,
	UNDO_REMOVE_SPAWN
} UndoType;

typedef struct {
	UndoType type;
	int grid_x, grid_y;
	int cell_type_index;
	ZoneSpawn spawn;
	int spawn_index;
} UndoEntry;

static Zone zone;
static UndoEntry undoStack[ZONE_MAX_UNDO];
static int undoCount = 0;

static int find_cell_type(const char *id);
static void apply_zone_to_world(void);
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

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		/* Strip newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
		if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

		/* Skip empty lines and comments */
		if (len == 0 || (len >= 2 && line[0] == '/' && line[1] == '/'))
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
		else if (strncmp(line, "spawn ", 6) == 0) {
			if (zone.spawn_count >= ZONE_MAX_SPAWNS) continue;

			ZoneSpawn *sp = &zone.spawns[zone.spawn_count];
			if (sscanf(line + 6, "%15s %lf %lf", sp->enemy_type, &sp->world_x, &sp->world_y) == 3)
				zone.spawn_count++;
		}
		else if (strncmp(line, "portal ", 7) == 0) {
			if (zone.portal_count >= ZONE_MAX_PORTALS) continue;

			ZonePortal *p = &zone.portals[zone.portal_count];
			if (sscanf(line + 7, "%d %d %31s %255s %31s",
				&p->grid_x, &p->grid_y, p->id, p->dest_zone, p->dest_portal_id) == 5)
				zone.portal_count++;
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
	}

	fclose(f);

	apply_zone_to_world();

	printf("Zone_load: loaded '%s' (%d cell types, %d spawns, %d portals, %d savepoints)\n",
		zone.name, zone.cell_type_count, zone.spawn_count, zone.portal_count, zone.savepoint_count);
}

void Zone_unload(void)
{
	Map_clear();
	Mine_cleanup();
	Portal_cleanup();
	Savepoint_cleanup();
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

	/* Cells */
	for (int x = 0; x < MAP_SIZE; x++) {
		for (int y = 0; y < MAP_SIZE; y++) {
			if (zone.cell_grid[x][y] >= 0) {
				int idx = zone.cell_grid[x][y];
				const ZoneDestructible *d = Zone_get_destructible(x, y);
				if (d)
					fprintf(f, "cell %d %d %s drop:%s\n", x, y,
						zone.cell_types[idx].id, d->drop_sub);
				else
					fprintf(f, "cell %d %d %s\n", x, y, zone.cell_types[idx].id);
			}
		}
	}
	fprintf(f, "\n");

	/* Spawns */
	for (int i = 0; i < zone.spawn_count; i++) {
		ZoneSpawn *sp = &zone.spawns[i];
		fprintf(f, "spawn %s %.1f %.1f\n", sp->enemy_type, sp->world_x, sp->world_y);
	}

	/* Portals */
	if (zone.portal_count > 0)
		fprintf(f, "\n");
	for (int i = 0; i < zone.portal_count; i++) {
		ZonePortal *p = &zone.portals[i];
		fprintf(f, "portal %d %d %s %s %s\n",
			p->grid_x, p->grid_y, p->id, p->dest_zone, p->dest_portal_id);
	}

	/* Save points */
	if (zone.savepoint_count > 0)
		fprintf(f, "\n");
	for (int i = 0; i < zone.savepoint_count; i++) {
		fprintf(f, "savepoint %d %d %s\n",
			zone.savepoints[i].grid_x, zone.savepoints[i].grid_y,
			zone.savepoints[i].id);
	}

	fclose(f);
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

	/* Update world */
	ZoneCellType *ct = &zone.cell_types[idx];
	MapCell cell = {false, strcmp(ct->pattern, "circuit") == 0,
		ct->primaryColor, ct->outlineColor};
	Map_set_cell(grid_x, grid_y, &cell);

	Zone_save();
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
	Map_clear_cell(grid_x, grid_y);

	Zone_save();
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
	zone.spawn_count++;

	/* Spawn in world */
	if (strcmp(enemy_type, "mine") == 0) {
		Position pos = {world_x, world_y};
		Mine_initialize(pos);
	}

	Zone_save();
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

	/* Reload world to reflect removed spawn */
	apply_zone_to_world();
	Zone_save();
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
		apply_zone_to_world();
		break;
	case UNDO_REMOVE_SPAWN:
		/* Remove last spawn */
		if (zone.spawn_count > undo.spawn_index)
			zone.spawn_count = undo.spawn_index;
		apply_zone_to_world();
		break;
	}

	Zone_save();
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

/* --- Internal --- */

static int find_cell_type(const char *id)
{
	for (int i = 0; i < zone.cell_type_count; i++) {
		if (strcmp(zone.cell_types[i].id, id) == 0)
			return i;
	}
	return -1;
}

static void apply_zone_to_world(void)
{
	Map_clear();
	Mine_cleanup();
	Portal_cleanup();
	Savepoint_cleanup();

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

	/* Spawn enemies */
	for (int i = 0; i < zone.spawn_count; i++) {
		ZoneSpawn *sp = &zone.spawns[i];
		if (strcmp(sp->enemy_type, "mine") == 0) {
			Position pos = {sp->world_x, sp->world_y};
			Mine_initialize(pos);
		}
	}

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
