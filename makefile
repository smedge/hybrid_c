compile: src/main.c
	gcc -std=c99 -Wall -DGL_SILENCE_DEPRECATION -o hybrid src/*.c -I. -I/opt/homebrew/include/ -L/opt/homebrew/lib -lSDL2 -lSDL2_mixer -framework OpenGL -lm

debug:
	gcc -std=c99 -Wall -DGL_SILENCE_DEPRECATION -g -o hybrid src/*.c -I. -I/opt/homebrew/include/ -L/opt/homebrew/lib -lSDL2 -lSDL2_mixer -framework OpenGL -lm

clean:
	rm hybrid
