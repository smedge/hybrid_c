#ifndef FOG_OF_WAR_H
#define FOG_OF_WAR_H

#include <stdbool.h>
#include "entity.h"

void FogOfWar_initialize(void);
void FogOfWar_cleanup(void);
void FogOfWar_reset_active(void);
void FogOfWar_update(Position player_pos);
void FogOfWar_reveal_all(void);
bool FogOfWar_is_revealed(int gx, int gy);

/* Zone transitions — swaps active revealed grid with in-memory buffer */
void FogOfWar_set_zone(const char *zone_path);

/* Checkpoint save — flush all in-memory buffers (active + cached) to disk */
void FogOfWar_save_all_to_disk(void);

/* Death / load-from-save — reload all buffers from disk, discard unsaved progress */
void FogOfWar_load_all_from_disk(void);

/* Delete all per-zone fog save files */
void FogOfWar_delete_all_saves(void);

bool FogOfWar_consume_dirty(void);
void FogOfWar_set_enabled(bool enabled);
bool FogOfWar_get_enabled(void);

#endif
