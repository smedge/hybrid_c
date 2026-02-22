#include "obstacle.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define MAX_OBSTACLES 64

static ChunkTemplate library[MAX_OBSTACLES];
static int library_count = 0;
static bool loaded = false;

void Obstacle_load_library(void)
{
	if (loaded) return;

	const char *dir_path = "resources/obstacles";
	DIR *dir = opendir(dir_path);
	if (!dir) {
		printf("Obstacle_load_library: no obstacles directory '%s'\n", dir_path);
		loaded = true;
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (library_count >= MAX_OBSTACLES) break;

		/* Only load .chunk files */
		const char *dot = strrchr(entry->d_name, '.');
		if (!dot || strcmp(dot, ".chunk") != 0) continue;

		char filepath[512];
		snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);

		if (Chunk_load(&library[library_count], filepath))
			library_count++;
	}

	closedir(dir);
	loaded = true;

	printf("Obstacle_load_library: loaded %d obstacle templates\n", library_count);
}

void Obstacle_cleanup_library(void)
{
	library_count = 0;
	loaded = false;
}

int Obstacle_get_count(void)
{
	return library_count;
}

const ChunkTemplate *Obstacle_find(const char *name)
{
	for (int i = 0; i < library_count; i++) {
		if (strcmp(library[i].name, name) == 0)
			return &library[i];
	}
	return NULL;
}
