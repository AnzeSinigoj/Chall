all:
	gcc main.c -o chall -lncurses
run:	all
	./chall
clean:
	rm chall
