DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
LD  = nspire-ld
GENZEHN = genzehn

GCCFLAGS = -Wall -W -marm -O3
LDFLAGS = -Wl,--nspireio
ZEHNFLAGS = --name "int8bench"

OBJS = $(patsubst %.c, %.o, $(shell find . -name \*.c))
EXE = int8bench
DISTDIR = .
vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)

all: $(EXE).tns

%.o: %.c
	$(GCC) $(GCCFLAGS) -c $<

$(EXE).elf: $(OBJS)
	mkdir -p $(DISTDIR)
	$(LD) $^ -o $(DISTDIR)/$@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $(DISTDIR)/$^ --output $(DISTDIR)/$@.zehn $(ZEHNFLAGS)
	make-prg $(DISTDIR)/$@.zehn $(DISTDIR)/$@
	rm $(DISTDIR)/$@.zehn

clean:
	rm -f *.o $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf

main.o : int8dot.h
int8dot.o : int8dot.h
