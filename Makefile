DEBUG?=
WARNINGS=-fstack-protector -pedantic -W -Wall -Wbad-function-cast \
	-Wcast-align -Wcast-qual -Wdisabled-optimization -Wendif-labels \
	-Wfloat-equal -Wformat=2 -Wformat-nonliteral -Winline \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wno-unused-parameter -Wpointer-arith -Wshadow -Wstrict-prototypes \
	-Wstack-protector -Wswitch -Wundef -Wwrite-strings
INCLUDES=
LDFLAGS=-lgc -lm
CFLAGS=$(DEBUG) $(WARNINGS) $(INCLUDES) $(RELEASE) -std=c99
PREFIX?=/usr/local

OBJS=surd.o

LINTABLES=surd.c\
	surd.h

surd-debug:
	$(MAKE) DEBUG='-g' clean surd

surd-sanitize:
	$(MAKE) DEBUG='-g -fsanitize=address -fno-omit-frame-pointer' clean surd

surd-release:
	$(MAKE) RELEASE='-O2' surd

surd: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) main.c -o surd

test: $(OBJS)
	cd t && $(MAKE) && cd ..

clean:
	rm -f *.o surd

lint:
	cppcheck $(LINTABLES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f surd *.o
