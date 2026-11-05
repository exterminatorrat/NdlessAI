DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
LD  = nspire-ld
GENZEHN = genzehn

RUNTIME = ../runtime
DATA = ../data
GCCFLAGS = -Wall -W -marm -I$(RUNTIME)
LDFLAGS = -Wl,--nspireio
ZEHNFLAGS = --name "stemhelp"

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -Os
else
	GCCFLAGS += -O0 -g
endif

OBJS = main.o pipeline.o bpe.o fixed.o infer.o solvers.o
EXE = stemhelp
DISTDIR = .

all: $(EXE).tns weights.bin.tns

# The tokenizer is compiled into the app, so it must be the one the model was
# trained with. A stale bpe.h compiles fine and produces garbage calls, which
# is indistinguishable from a broken model. Regenerate it every build.
$(RUNTIME)/bpe.h: $(DATA)/tokenizer.json
	uv run --with sympy python ../export_tokenizer.py \
		--tokenizer $(DATA)/tokenizer.json --header $(RUNTIME)/bpe.h

check-tokenizer:
	uv run --with sympy python ../check_tokenizer_sync.py \
		$(DATA)/tokenizer.json $(RUNTIME)/bpe.h

main.o: main.c $(RUNTIME)/bpe.h
	$(GCC) $(GCCFLAGS) -c $< -o $@

%.o: $(RUNTIME)/%.c $(RUNTIME)/bpe.h
	$(GCC) $(GCCFLAGS) -c $< -o $@

$(EXE).elf: $(OBJS)
	$(LD) $^ -o $(DISTDIR)/$@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $(DISTDIR)/$^ --output $(DISTDIR)/$@.zehn $(ZEHNFLAGS)
	make-prg $(DISTDIR)/$@.zehn $(DISTDIR)/$@
	rm $(DISTDIR)/$@.zehn

# TI's transfer software only accepts .tns files, so the model is copied under
# that extension. The contents are unchanged; only the name matters.
weights.bin.tns: ../weights.bin
	cp $< $@
	@echo "packaged $@ ($(shell wc -c < $<) bytes)"

clean:
	rm -f *.o $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn
	rm -f weights.bin.tns

.PHONY: all clean check-tokenizer
