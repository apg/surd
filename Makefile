CC = gcc
CFLAGS = -g -Wall

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

surd: surd.o main.o
	$(CC) $(CFLAGS) -o $@ $^