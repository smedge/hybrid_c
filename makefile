compile: src/main.c
	gcc -o hybrid src/*.c -I. -lSDL2

clean:
	rm hybrid