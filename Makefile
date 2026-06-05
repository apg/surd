DEBUG?=
WARNINGS=-fstack-protector -pedantic -W -Wall -Wbad-function-cast \
	-Wcast-align -Wcast-qual -Wdisabled-optimization -Wendif-labels \
	-Wfloat-equal -Wformat=2 -Wformat-nonliteral -Winline \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wno-unused-parameter -Wpointer-arith -Wshadow -Wstrict-prototypes \
	-Wstack-protector -Wswitch -Wundef -Wwrite-strings
INCLUDES=-I/opt/homebrew/include
LDFLAGS=-L/opt/homebrew/lib -lgc -lm
CFLAGS=$(DEBUG) $(WARNINGS) $(INCLUDES) $(RELEASE) -std=c99
PREFIX?=/usr/local

OBJS=surd.o

LINTABLES=surd.c\
	surd.h

surd-debug:
	$(MAKE) DEBUG='-g -DPROFILE' clean surd

surd-sanitize:
	$(MAKE) DEBUG='-g -fsanitize=address -fno-omit-frame-pointer' clean surd

surd-release:
	$(MAKE) RELEASE='-O2' surd

surd: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o surd

check: $(OBJS)
	cd t && $(MAKE) check && cd ..

clean:
	rm -f *.o surd

lint:
	cppcheck $(LINTABLES)

bench: bench-map-square bench-fact

bench-fact: surd
	rm -f eval_map_square.log
	for n in `seq 100`; do ./surd t/code/eval_fact.surd 2>&1 | awk '/elapsed load time/ {print $$4}' >> eval_fact.log; done
	ministat bench-baseline/eval_fact.log eval_fact.log

bench-map-square: surd
	for n in `seq 100`; do ./surd t/code/eval_map_square.surd 2>&1 | awk '/elapsed load time/ {print $$4}' >> eval_map_square.log; done
	ministat bench-baseline/eval_map_square.log eval_map_square.log

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f surd *.o
