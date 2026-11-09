CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDLIBS = -lm

HEADERS = bpe.h fixed.h infer.h solvers.h pipeline.h graph.h expr.h

all: infer_host main_device test_solvers test_bpe test_fixed test_guard test_graph test_typed

infer_host: main_host.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ main_host.c infer.c $(LDLIBS)

main_device: main_device.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ main_device.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(LDLIBS)

test_solvers: test_solvers.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_solvers.c solvers.c $(LDLIBS)

test_bpe: test_bpe.c bpe.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_bpe.c bpe.c

test_fixed: test_fixed.c fixed.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_fixed.c fixed.c infer.c $(LDLIBS)

test_guard: test_guard.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_guard.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(LDLIBS)

test_graph: test_graph.c graph.c expr.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_graph.c graph.c expr.c $(LDLIBS)

test_typed: test_typed.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_typed.c pipeline.c bpe.c fixed.c infer.c solvers.c graph.c expr.c $(LDLIBS)

check: all
	./test_solvers
	./test_bpe
	./test_fixed ../weights.bin
	./test_guard
	./test_graph
	./test_typed
	sh crosscheck.sh

clean:
	rm -f infer_host main_device test_solvers test_bpe test_fixed test_guard test_graph test_typed

.PHONY: all clean check
