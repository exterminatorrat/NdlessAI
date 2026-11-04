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

all: $(EXE).tns

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

clean:
	rm -f *.o $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn

.PHONY: all clean
