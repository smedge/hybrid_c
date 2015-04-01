compile: src/main.c
	gcc -g -o hybrid src/*.c -I. -lSDL2

clean:
	rm hybrid