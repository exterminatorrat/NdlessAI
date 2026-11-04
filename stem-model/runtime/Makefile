CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDLIBS = -lm

HEADERS = bpe.h fixed.h infer.h solvers.h

all: infer_host main_device test_solvers test_bpe test_fixed

infer_host: main_host.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ main_host.c infer.c $(LDLIBS)

main_device: main_device.c bpe.c fixed.c infer.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ main_device.c bpe.c fixed.c infer.c solvers.c $(LDLIBS)

test_solvers: test_solvers.c solvers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_solvers.c solvers.c $(LDLIBS)

test_bpe: test_bpe.c bpe.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_bpe.c bpe.c

test_fixed: test_fixed.c fixed.c infer.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ test_fixed.c fixed.c infer.c $(LDLIBS)

check: all
	./test_solvers
	./test_bpe
	./test_fixed ../weights.bin
	sh crosscheck.sh

clean:
	rm -f infer_host main_device test_solvers test_bpe test_fixed

.PHONY: all clean check
