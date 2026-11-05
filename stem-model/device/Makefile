DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
LD  = nspire-ld
GENZEHN = genzehn

RUNTIME = ../runtime
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

main.o: main.c
	$(GCC) $(GCCFLAGS) -c $< -o $@

%.o: $(RUNTIME)/%.c
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

.PHONY: all clean
