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

test_delta_2d_bug: test_delta_2d_bug.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_rans_overflow_bug: test_rans_overflow_bug.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_avx_penalty_pbt: test_avx_penalty_pbt.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_bp_assimetria_bug: test_bp_assimetria_bug.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_codec_validacao_bug: test_codec_validacao_bug.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_ctx_modelo_atualizado: test_ctx_modelo_atualizado.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

test_ctx_ordem_sincronizada: test_ctx_ordem_sincronizada.cpp $(COREOBJ) $(ASMOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $^

$(COREDIR)/%.o: $(COREDIR)/%.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

$(ASMDIR)/%.o: $(ASMDIR)/%.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

clean:
	rm -f $(BINDIR)/comp $(BINDIR)/decomp $(BINDIR)/dbg_metodo $(COREOBJ) $(ASMOBJ)

.PHONY: all clean
