compile: src/main.c
	gcc -std=c99 -Wall -o hybrid src/*.c -I. -I/usr/include/freetype2 -lSDL2 -lSDL2_mixer -lGL -lGLU -lftgl -lm

debug:
	gcc -std=c99 -Wall -g -o hybrid src/*.c -I. -I/usr/include/freetype2 -lSDL2 -lSDL2_mixer -lGL -lGLU -lftgl -lm

clean:
	rm hybrid
