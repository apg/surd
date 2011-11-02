CC = gcc
CFLAGS = -g -Wall

all: test_reader test_primitives

TESTS = test_primitives test_reader

test_reader: test_reader.c
	$(CC) $(CFLAGS) -o $@ ../surd.o ../primitives.o $<

test_primitives: test_primitives.c
	$(CC) $(CFLAGS) -o $@ ../surd.o ../primitives.o $<

clean:
	rm $(TESTS)
