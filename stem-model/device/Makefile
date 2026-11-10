DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
LD  = nspire-ld
GENZEHN = genzehn
PYTHON = python3

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

OBJS = main.o graph_ui.o pipeline.o bpe.o fixed.o infer.o graph.o expr.o callmask.o \
       matrix.o solvers.o solvers_math.o equation.o solvers_trig.o solvers_physics.o \
       solvers_chem.o solvers_units.o
EXE = stemhelp
DISTDIR = .

all: tokenizer $(EXE).tns weights.bin.tns

# The tokenizer is compiled into the app, so it must be the one the model was
# trained with. A stale bpe.h compiles fine and produces garbage calls, which
# is indistinguishable from a broken model.
#
# This is phony on purpose. A file-timestamp rule does not work here: git
# checkout gives bpe.h a fresh mtime, so it looks newer than tokenizer.json and
# the rule never fires. The exporter is a no-op when the content already
# matches, so running it every build costs nothing and cannot go stale.
tokenizer:
	$(PYTHON) ../export_tokenizer.py \
		--tokenizer $(DATA)/tokenizer.json --header $(RUNTIME)/bpe.h

check-tokenizer:
	$(PYTHON) ../check_tokenizer_sync.py \
		$(DATA)/tokenizer.json $(RUNTIME)/bpe.h

main.o: main.c tokenizer
	$(GCC) $(GCCFLAGS) -c $< -o $@

graph_ui.o: graph_ui.c graph_ui.h
	$(GCC) $(GCCFLAGS) -c $< -o $@

# The plotter has no tokenizer dependency, so it does not wait on the
# generated bpe.h the way the model objects do.
graph.o: $(RUNTIME)/graph.c $(RUNTIME)/graph.h
	$(GCC) $(GCCFLAGS) -c $< -o $@

%.o: $(RUNTIME)/%.c tokenizer
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

.PHONY: all clean check-tokenizer tokenizer
