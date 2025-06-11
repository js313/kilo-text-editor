kilo: kilo.c
	$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

build-run:
	make kilo && ./kilo

build-run-file:
	make kilo && ./kilo kilo.c

clean:
	rm kilo