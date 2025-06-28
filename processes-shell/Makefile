CC = gcc
CFLAGS = -Wall -Werror -std=gnu23

wish : wish.o

debug : CFLAGS += -Og -g
debug : wish.o

test: wish
	./test-wish.sh

.PHONY : make
clean:
	rm -f wish wish.o
