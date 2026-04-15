#ifndef RANS_H
#define RANS_H

#include "tipos.h"
#include <vector>

struct rans_enc {
    u32 estado;
    std::vector<u8> saida;
};

struct rans_dec {
    u32 estado;
    const u8* ptr;
    const u8* fim;
};

void rans_enc_init(rans_enc* e);
void rans_enc_put(rans_enc* e, u32 freq, u32 cum, u32 total);
void rans_enc_flush(rans_enc* e);

void rans_dec_init(rans_dec* d, const u8* dados, u32 tam);
u32 rans_dec_get(rans_dec* d, u32 freq, u32 cum, u32 total);

#endif
