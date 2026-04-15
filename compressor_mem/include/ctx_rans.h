#ifndef CTX_RANS_H
#define CTX_RANS_H

#include "tipos.h"
#include "config.h"
#include <vector>

struct ctx_modelo {
    u32 freq[CTX_TAM][256];
    u32 total[CTX_TAM];
    u8 ctx;
};

void ctx_init(ctx_modelo* m);
void ctx_atualizar(ctx_modelo* m, u8 byte);
std::vector<u8> ctx_rans_enc(const u8* dados, u32 tam);
std::vector<u8> ctx_rans_dec(const u8* dados, u32 tam);

#endif
