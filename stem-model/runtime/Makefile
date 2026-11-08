CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDLIBS = -lm

# arm926ej-s has no SIMD, so the benchmark has to time scalar code: a vectorised
# host build measures the host vectoriser instead of the change, and which of
# the two kernels it manages to vectorise is arbitrary.  The switches are
# spelled differently by the two compilers this tree builds with, so probe for
# the pair rather than guessing from the compiler name.
VEC_OFF := $(shell $(CC) -fno-vectorize -fno-slp-vectorize -x c -c /dev/null \
	-o /dev/null 2>/dev/null && echo -fno-vectorize -fno-slp-vectorize || \
	echo -fno-tree-vectorize -fno-tree-slp-vectorize)
BENCH_CFLAGS ?= $(CFLAGS) $(VEC_OFF)

HEADERS = bpe.h fixed.h infer.h solvers.h pipeline.h

all: infer_host main_device test_solvers test_bpe test_fixed test_guard test_graph bench_kernel

infer_host: main_host.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ main_host.c infer.c $(LDLIBS)

main_device: main_device.c pipeline.c bpe.c fixed.c infer.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ main_device.c pipeline.c bpe.c fixed.c infer.c solvers.c $(LDLIBS)

test_solvers: test_solvers.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_solvers.c solvers.c $(LDLIBS)

test_bpe: test_bpe.c bpe.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_bpe.c bpe.c

test_fixed: test_fixed.c fixed.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_fixed.c fixed.c infer.c $(LDLIBS)

test_guard: test_guard.c pipeline.c bpe.c fixed.c infer.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_guard.c pipeline.c bpe.c fixed.c infer.c solvers.c $(LDLIBS)

test_graph: test_graph.c graph.c graph.h
	$(CC) $(CFLAGS) -I. -o $@ test_graph.c graph.c $(LDLIBS)

# Host-only microbenchmark for the int8 MAC kernel.  bench_kernel.c includes
# fixed.c rather than linking it, so the kernel inlines into the row loop the
# way it does on the device; it is never part of the device build, which is
# main_device.c and the sources it links.
bench_kernel: bench_kernel.c fixed.c $(HEADERS)
	$(CC) $(BENCH_CFLAGS) -I. -o $@ bench_kernel.c $(LDLIBS)

check: all
	./test_solvers
	./test_bpe
	./test_fixed ../weights.bin
	./test_guard
	./test_graph
	sh crosscheck.sh

clean:
	rm -f infer_host main_device test_solvers test_bpe test_fixed test_guard test_graph bench_kernel

.PHONY: all clean check
