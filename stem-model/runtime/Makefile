CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
LDLIBS = -lm

all: infer_host

infer_host: main_host.c infer.c infer.h
	$(CC) $(CFLAGS) -o $@ main_host.c infer.c $(LDLIBS)

clean:
	rm -f infer_host

.PHONY: all clean
