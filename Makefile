CC = gcc
CFLAGS = -g -Wall

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

surd: surd.o main.o primitives.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f surd *.o