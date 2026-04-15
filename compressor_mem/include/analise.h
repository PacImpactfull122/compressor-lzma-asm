#ifndef ANALISE_H
#define ANALISE_H

#include "tipos.h"
#include <cmath>

struct info_bloco {
    double entropia;
    u32 zeros_pct;
    u32 max_run;
    u32 unicos;
    bool repetitivo;
    bool executavel;
    bool texto;
};

enum metodo {
    METODO_STORE = 0,
    METODO_LZ = 1,
    METODO_RANS = 2,
    METODO_BCJ_LZ = 3,
    METODO_RLE = 4,
    METODO_BP = 5,
    METODO_CTX_RANS = 6,
    METODO_ROLZ = 7,
    METODO_HUFFMAN = 8
};

void analisar_bloco(const u8* dados, u32 tam, info_bloco* info);
u32 escolher_metodo(const info_bloco* info, const u8* dados, u32 tam);

#endif
