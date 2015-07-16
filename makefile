compile: src/main.c
	gcc -Wall -g -o hybrid src/*.c -I. -I/usr/include/freetype2 -lSDL2  -lGL  -lftgl

clean:
	rm hybrid