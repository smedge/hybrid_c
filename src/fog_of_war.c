#include "fog_of_war.h"
#include "map.h"
#include "hud.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#define FOW_SAVE_PATH "./save/fog_of_war.bin"

static bool revealed[MAP_SIZE][MAP_SIZE];
static bool fowEnabled = true;

void FogOfWar_initialize(void)
{
	memset(revealed, 0, sizeof(revealed));
	fowEnabled = true;
}

void FogOfWar_cleanup(void)
{
}

void FogOfWar_reset(void)
{
	memset(revealed, 0, sizeof(revealed));
}

void FogOfWar_update(Position player_pos)
{
	/* Convert world position to grid coordinates */
	int center_gx = (int)floor(player_pos.x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int center_gy = (int)floor(player_pos.y / MAP_CELL_SIZE) + HALF_MAP_SIZE;

	/* Derive reveal radius from minimap range */
	float range = Hud_get_minimap_range();
	int radius = (int)(range / (2.0f * MAP_CELL_SIZE));

	/* Clamp iteration bounds to grid */
	int min_x = center_gx - radius;
	int max_x = center_gx + radius;
	int min_y = center_gy - radius;
	int max_y = center_gy + radius;

	if (min_x < 0) min_x = 0;
	if (min_y < 0) min_y = 0;
	if (max_x >= MAP_SIZE) max_x = MAP_SIZE - 1;
	if (max_y >= MAP_SIZE) max_y = MAP_SIZE - 1;

	for (int y = min_y; y <= max_y; y++)
		for (int x = min_x; x <= max_x; x++)
			revealed[x][y] = true;
}

void FogOfWar_reveal_all(void)
{
	memset(revealed, 1, sizeof(revealed));
}

bool FogOfWar_is_revealed(int gx, int gy)
{
	if (!fowEnabled)
		return true;
	if (gx < 0 || gx >= MAP_SIZE || gy < 0 || gy >= MAP_SIZE)
		return false;
	return revealed[gx][gy];
}

void FogOfWar_save_to_file(void)
{
#ifdef _WIN32
	_mkdir("./save");
#else
	mkdir("./save", 0755);
#endif

	FILE *f = fopen(FOW_SAVE_PATH, "wb");
	if (!f) {
		printf("FogOfWar: failed to write %s\n", FOW_SAVE_PATH);
		return;
	}
	fwrite(revealed, sizeof(revealed), 1, f);
	fclose(f);
}

void FogOfWar_load_from_file(void)
{
	FILE *f = fopen(FOW_SAVE_PATH, "rb");
	if (!f)
		return;
	size_t read = fread(revealed, 1, sizeof(revealed), f);
	fclose(f);
	if (read != sizeof(revealed)) {
		printf("FogOfWar: save file size mismatch, resetting\n");
		memset(revealed, 0, sizeof(revealed));
	}
}

void FogOfWar_set_enabled(bool enabled)
{
	fowEnabled = enabled;
}

bool FogOfWar_get_enabled(void)
{
	return fowEnabled;
}
