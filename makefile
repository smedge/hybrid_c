compile: src/main.c
	gcc -o hybrid src/main.c src/game.c -I. -lSDL2

clean:
	rm hybrid