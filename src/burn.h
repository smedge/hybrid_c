#ifndef BURN_H
#define BURN_H

#include <stdbool.h>
#include "position.h"

#define BURN_MAX_STACKS 3
#define BURN_DPS 10.0          /* damage per second per stack */
#define BURN_DURATION_MS 3000  /* default burn duration (3s) */

/* Embeddable burn state for any entity (player or enemy) */
typedef struct {
	int stacks;
	int duration_ms[BURN_MAX_STACKS]; /* remaining ms per stack */
} BurnState;

/* Apply a burn stack. If at max stacks, refreshes the shortest remaining. */
void Burn_apply(BurnState *state, int duration_ms);

/* Tick burn timers. Returns total damage dealt this tick (caller applies to HP). */
double Burn_update(BurnState *state, unsigned int ticks);

/* Clear all stacks */
void Burn_reset(BurnState *state);

/* Query */
bool Burn_is_active(const BurnState *state);
int Burn_get_stacks(const BurnState *state);

/* Render burn particles at position (call from entity render functions) */
void Burn_render(const BurnState *state, Position pos);

/* Enemy burn helper: tick burn, apply damage to hp, return true if burn killed. */
bool Burn_tick_enemy(BurnState *burn, double *hp, unsigned int ticks);

/* Apply burn stacks from a PlayerDamageResult's burn_hits (if any, and if not shielded). */
void Burn_apply_from_hits(BurnState *burn, int burn_hits);

/* --- Player burn (singleton) --- */
void Burn_apply_to_player(int duration_ms);
void Burn_update_player(unsigned int ticks); /* calls PlayerStats_damage internally */
void Burn_render_player(Position pos);       /* call from ship render */
void Burn_reset_player(void);
bool Burn_player_is_burning(void);

/* Audio lifecycle */
void Burn_initialize_audio(void);
void Burn_cleanup_audio(void);

#endif
