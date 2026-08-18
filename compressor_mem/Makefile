CC = g++
CFLAGS = -O3 -march=native -Wall -Wextra -Wno-unused-parameter
NASM = nasm
NASMFLAGS = -f elf64

SRCDIR = src
COREDIR = $(SRCDIR)/core
ASMDIR = $(SRCDIR)/asm
INCDIR = include
BINDIR = bin

CORESRC = $(COREDIR)/lz.cpp $(COREDIR)/rans.cpp $(COREDIR)/bcj.cpp $(COREDIR)/bitplane.cpp $(COREDIR)/codec.cpp $(COREDIR)/analise.cpp $(COREDIR)/rle.cpp $(COREDIR)/delta.cpp $(COREDIR)/huff.cpp $(COREDIR)/ctx_rans.cpp $(COREDIR)/rolz.cpp $(COREDIR)/rans_lz.cpp $(COREDIR)/mtf.cpp
ASMSRC = $(ASMDIR)/hash_compare.asm $(ASMDIR)/bt4_search.asm $(ASMDIR)/bt4_opt.asm $(ASMDIR)/rans_simd.asm

COREOBJ = $(CORESRC:.cpp=.o)
ASMOBJ = $(ASMSRC:.asm=.o)

all: $(BINDIR)/comp $(BINDIR)/decomp $(BINDIR)/dbg_metodo

$(BINDIR)/comp: comp.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^

$(BINDIR)/decomp: decomp.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^

$(BINDIR)/dbg_metodo: src/tools/dbg_metodo.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^

$(BINDIR):
	mkdir -p $(BINDIR)

$(COREDIR)/%.o: $(COREDIR)/%.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

$(ASMDIR)/%.o: $(ASMDIR)/%.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

clean:
	rm -f $(BINDIR)/comp $(BINDIR)/decomp $(BINDIR)/dbg_metodo $(COREOBJ) $(ASMOBJ)

.PHONY: all clean
