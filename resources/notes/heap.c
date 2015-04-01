
Game *game_create() {
	Game *game = (Game *)calloc(1, sizeof(Game));
	game_initialize(game);
	return game;
}

void game_delete(Game *game) {
	game_cleanup(game);
	free(game);
}