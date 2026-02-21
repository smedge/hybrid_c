#include "fog_of_war.h"
#include "map.h"
#include "hud.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>

#define FOW_SAVE_DIR "./save/"
#define FOW_SAVE_PREFIX "fog_"
#define FOW_SAVE_EXT ".bin"
#define FOW_BLOCK_SIZE 16
#define FOW_MAX_ZONES 16

/* Active zone's revealed grid — what the map window reads */
static bool revealed[MAP_SIZE][MAP_SIZE];
static bool fowEnabled = true;
static bool fowDirty = true;
static int lastBlockX = -1;
static int lastBlockY = -1;

/* Per-zone in-memory cache — persists across zone transitions until death or save */
typedef struct {
	char zone_path[256];
	bool data[MAP_SIZE][MAP_SIZE];
	bool in_use;
} FowZoneCache;

static FowZoneCache zoneCache[FOW_MAX_ZONES];
static char activeZonePath[256];

/* --- Helpers --- */

static void build_save_path(const char *zone_path, char *out, int out_size)
{
	const char *base = zone_path;
	const char *slash = strrchr(zone_path, '/');
	if (slash) base = slash + 1;

	int len = (int)strlen(base);
	const char *dot = strrchr(base, '.');
	if (dot) len = (int)(dot - base);

	snprintf(out, out_size, "%s%s%.*s%s", FOW_SAVE_DIR, FOW_SAVE_PREFIX, len, base, FOW_SAVE_EXT);
}

static FowZoneCache *find_cache(const char *zone_path)
{
	for (int i = 0; i < FOW_MAX_ZONES; i++)
		if (zoneCache[i].in_use && strcmp(zoneCache[i].zone_path, zone_path) == 0)
			return &zoneCache[i];
	return NULL;
}

static FowZoneCache *alloc_cache(const char *zone_path)
{
	for (int i = 0; i < FOW_MAX_ZONES; i++) {
		if (!zoneCache[i].in_use) {
			zoneCache[i].in_use = true;
			strncpy(zoneCache[i].zone_path, zone_path, sizeof(zoneCache[i].zone_path) - 1);
			zoneCache[i].zone_path[sizeof(zoneCache[i].zone_path) - 1] = '\0';
			memset(zoneCache[i].data, 0, sizeof(zoneCache[i].data));
			return &zoneCache[i];
		}
	}
	printf("FogOfWar: zone cache full (%d zones), dropping oldest\n", FOW_MAX_ZONES);
	/* Evict slot 0 */
	zoneCache[0].in_use = true;
	strncpy(zoneCache[0].zone_path, zone_path, sizeof(zoneCache[0].zone_path) - 1);
	zoneCache[0].zone_path[sizeof(zoneCache[0].zone_path) - 1] = '\0';
	memset(zoneCache[0].data, 0, sizeof(zoneCache[0].data));
	return &zoneCache[0];
}

static void save_zone_to_disk(const char *zone_path, const bool data[MAP_SIZE][MAP_SIZE])
{
#ifdef _WIN32
	_mkdir("./save");
#else
	mkdir("./save", 0755);
#endif

	char path[512];
	build_save_path(zone_path, path, sizeof(path));

	FILE *f = fopen(path, "wb");
	if (!f) {
		printf("FogOfWar: failed to write %s\n", path);
		return;
	}
	fwrite(data, MAP_SIZE * MAP_SIZE * sizeof(bool), 1, f);
	fclose(f);
}

static bool load_zone_from_disk(const char *zone_path, bool data[MAP_SIZE][MAP_SIZE])
{
	char path[512];
	build_save_path(zone_path, path, sizeof(path));

	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	size_t r = fread(data, 1, MAP_SIZE * MAP_SIZE * sizeof(bool), f);
	fclose(f);
	if (r != MAP_SIZE * MAP_SIZE * sizeof(bool)) {
		printf("FogOfWar: save file size mismatch for %s, resetting\n", path);
		memset(data, 0, MAP_SIZE * MAP_SIZE * sizeof(bool));
		return false;
	}
	return true;
}

/* --- Public API --- */

void FogOfWar_initialize(void)
{
	memset(revealed, 0, sizeof(revealed));
	memset(zoneCache, 0, sizeof(zoneCache));
	activeZonePath[0] = '\0';
	fowEnabled = true;
	fowDirty = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_cleanup(void)
{
}

void FogOfWar_reset_active(void)
{
	memset(revealed, 0, sizeof(revealed));
	fowDirty = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_set_zone(const char *zone_path)
{
	if (!zone_path || !zone_path[0]) return;

	/* If same zone, nothing to do */
	if (activeZonePath[0] && strcmp(activeZonePath, zone_path) == 0)
		return;

	/* Stash current active grid into cache */
	if (activeZonePath[0]) {
		FowZoneCache *cur = find_cache(activeZonePath);
		if (!cur) cur = alloc_cache(activeZonePath);
		memcpy(cur->data, revealed, sizeof(revealed));
	}

	/* Load destination from cache, or start fresh */
	FowZoneCache *dest = find_cache(zone_path);
	if (dest) {
		memcpy(revealed, dest->data, sizeof(revealed));
	} else {
		memset(revealed, 0, sizeof(revealed));
	}

	strncpy(activeZonePath, zone_path, sizeof(activeZonePath) - 1);
	activeZonePath[sizeof(activeZonePath) - 1] = '\0';
	fowDirty = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_save_all_to_disk(void)
{
	/* Sync active grid back to cache first */
	if (activeZonePath[0]) {
		FowZoneCache *cur = find_cache(activeZonePath);
		if (!cur) cur = alloc_cache(activeZonePath);
		memcpy(cur->data, revealed, sizeof(revealed));
	}

	/* Write all cached zones to disk */
	for (int i = 0; i < FOW_MAX_ZONES; i++) {
		if (zoneCache[i].in_use)
			save_zone_to_disk(zoneCache[i].zone_path, zoneCache[i].data);
	}
}

void FogOfWar_load_all_from_disk(void)
{
	/* Reload all cached zones from disk — discard unsaved in-memory progress */
	for (int i = 0; i < FOW_MAX_ZONES; i++) {
		if (zoneCache[i].in_use) {
			if (!load_zone_from_disk(zoneCache[i].zone_path, zoneCache[i].data))
				memset(zoneCache[i].data, 0, sizeof(zoneCache[i].data));
		}
	}

	/* Reload active zone — from cache if present, otherwise directly from disk */
	if (activeZonePath[0]) {
		FowZoneCache *cur = find_cache(activeZonePath);
		if (cur) {
			memcpy(revealed, cur->data, sizeof(revealed));
		} else if (!load_zone_from_disk(activeZonePath, revealed)) {
			memset(revealed, 0, sizeof(revealed));
		}
	}
	fowDirty = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_delete_all_saves(void)
{
	DIR *dir = opendir(FOW_SAVE_DIR);
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, FOW_SAVE_PREFIX, 4) == 0) {
				char path[512];
				snprintf(path, sizeof(path), "%s%s", FOW_SAVE_DIR, entry->d_name);
				remove(path);
			}
		}
		closedir(dir);
	}
}

void FogOfWar_update(Position player_pos)
{
	int center_gx = (int)floor(player_pos.x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int center_gy = (int)floor(player_pos.y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int block_x = center_gx / FOW_BLOCK_SIZE;
	int block_y = center_gy / FOW_BLOCK_SIZE;

	if (block_x == lastBlockX && block_y == lastBlockY)
		return;
	lastBlockX = block_x;
	lastBlockY = block_y;

	float range = Hud_get_minimap_range();
	int radius_cells = (int)(range / (2.0f * MAP_CELL_SIZE));
	int radius_blocks = (radius_cells + FOW_BLOCK_SIZE - 1) / FOW_BLOCK_SIZE;

	int min_bx = block_x - radius_blocks;
	int max_bx = block_x + radius_blocks;
	int min_by = block_y - radius_blocks;
	int max_by = block_y + radius_blocks;

	int max_block = MAP_SIZE / FOW_BLOCK_SIZE;
	if (min_bx < 0) min_bx = 0;
	if (min_by < 0) min_by = 0;
	if (max_bx >= max_block) max_bx = max_block - 1;
	if (max_by >= max_block) max_by = max_block - 1;

	for (int by = min_by; by <= max_by; by++) {
		for (int bx = min_bx; bx <= max_bx; bx++) {
			int x0 = bx * FOW_BLOCK_SIZE;
			int y0 = by * FOW_BLOCK_SIZE;

			if (revealed[x0][y0] && revealed[x0 + FOW_BLOCK_SIZE - 1][y0 + FOW_BLOCK_SIZE - 1])
				goto next_block;

			for (int y = y0; y < y0 + FOW_BLOCK_SIZE; y++)
				for (int x = x0; x < x0 + FOW_BLOCK_SIZE; x++)
					if (!revealed[x][y]) {
						revealed[x][y] = true;
						fowDirty = true;
					}
			next_block:;
		}
	}
}

void FogOfWar_reveal_all(void)
{
	memset(revealed, 1, sizeof(revealed));
	fowDirty = true;
}

bool FogOfWar_is_revealed(int gx, int gy)
{
	if (!fowEnabled)
		return true;
	if (gx < 0 || gx >= MAP_SIZE || gy < 0 || gy >= MAP_SIZE)
		return false;
	return revealed[gx][gy];
}

bool FogOfWar_consume_dirty(void)
{
	if (!fowDirty)
		return false;
	fowDirty = false;
	return true;
}

void FogOfWar_set_enabled(bool enabled)
{
	fowEnabled = enabled;
}

bool FogOfWar_get_enabled(void)
{
	return fowEnabled;
}
