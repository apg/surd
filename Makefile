CC = gcc
CFLAGS = -g -Wall

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

surd: surd.o main.o primitives.o alloc.o
	$(CC) $(CFLAGS) -o $@ $^ -lgc

clean:
	rm -f surd *.o
