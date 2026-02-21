#include "fog_of_war.h"
#include "map.h"
#include "hud.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#define FOW_SAVE_PATH "./save/fog_of_war.bin"
#define FOW_BLOCK_SIZE 16

static bool revealed[MAP_SIZE][MAP_SIZE];
static bool fowEnabled = true;
static bool fowDirty = true;
static int lastBlockX = -1;
static int lastBlockY = -1;

void FogOfWar_initialize(void)
{
	memset(revealed, 0, sizeof(revealed));
	fowEnabled = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_cleanup(void)
{
}

void FogOfWar_reset(void)
{
	memset(revealed, 0, sizeof(revealed));
	fowDirty = true;
	lastBlockX = -1;
	lastBlockY = -1;
}

void FogOfWar_update(Position player_pos)
{
	/* Convert world position to grid, then to block coordinates */
	int center_gx = (int)floor(player_pos.x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int center_gy = (int)floor(player_pos.y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int block_x = center_gx / FOW_BLOCK_SIZE;
	int block_y = center_gy / FOW_BLOCK_SIZE;

	/* Only reveal when entering a new block */
	if (block_x == lastBlockX && block_y == lastBlockY)
		return;
	lastBlockX = block_x;
	lastBlockY = block_y;

	/* Derive reveal radius in blocks from minimap range */
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

	/* Reveal entire 16x16 blocks within range */
	for (int by = min_by; by <= max_by; by++) {
		for (int bx = min_bx; bx <= max_bx; bx++) {
			int x0 = bx * FOW_BLOCK_SIZE;
			int y0 = by * FOW_BLOCK_SIZE;

			/* Quick check: skip if block already fully revealed */
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
	fowDirty = true;
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
