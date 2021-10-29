setup:
	gcc -std=gnu99 -o smallsh main.c smallsh.c builtIn.c helpers.c

clean:
	rm smallsh