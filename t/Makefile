CFLAGS = -g -Wall
LDFLAGS = -lgc
DEPS = ../surd.c

TESTS = test_reader test_primitives test_eval

check: $(TESTS)
	./run-tests.sh $(TESTS)

test_reader: test_reader.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DEPS) $<

test_primitives: test_primitives.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DEPS) $<

test_eval: test_eval.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DEPS) $<

clean:
	rm $(TESTS)
