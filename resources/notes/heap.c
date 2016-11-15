
Game *game_create() {
	Game *game = (Game *)calloc(1, sizeof(Game));
	game_initialize(game);
	return game;
}

void game_delete(Game *game) {
	game_cleanup(game);
	free(game);
}






typedef struct {
	void *item = 0;
	List_item *next = 0;
} List_item ;



List_item *list;

List_item *List_add(void* item) 
{
	List_item *item = (List_item *)calloc(1, sizeof(List_item));
}

void List_delete_all()
{
 for each List_item in list
}