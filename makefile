compile: src/main.c
	gcc -g -o hybrid src/*.c -I. -lSDL2  -lGL

clean:
	rm hybrid