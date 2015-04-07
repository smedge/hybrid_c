compile: src/main.c
	gcc -Wall -g -o hybrid src/*.c -I. -lSDL2  -lGL

clean:
	rm hybrid