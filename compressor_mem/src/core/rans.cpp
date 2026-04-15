#include "../../include/rans.h"
#include "../../include/config.h"
#include <algorithm>

// * rANS simetrico com RANS_M = 65536
// * encode: normaliza estado emitindo bytes baixos ate caber, depois codifica
// * decode: renormaliza consumindo bytes do stream, depois decodifica s = estado % total
// * stream escrito de tras para frente pelo encoder, lido de frente para tras pelo decoder
// * rans_enc_flush escreve estado final, rans_dec_init le os primeiros 4 bytes

void rans_enc_init(rans_enc* e) {
    e->estado = RANS_M;
    e->saida.clear();
}

void rans_enc_put(rans_enc* e, u32 freq, u32 cum, u32 total) {
    if (freq  == 0) freq  = 1;
    if (total == 0) total = 1;

    // ! total > RANS_M causa overflow na divisao de normalizacao
    if (total > RANS_M) total = RANS_M;
    if (freq  > total)  freq  = total;

    // * max_val define o limite de normalizacao, garante que o estado cabe na faixa esperada apos codificar
    u64 max_val = (u64)((RANS_M >> 8) / total) * freq;
    if (max_val == 0) max_val = 1;
    max_val <<= 8;

    while (e->estado >= max_val) {
        e->saida.push_back(e->estado & 0xFF);
        e->estado >>= 8;
    }

    e->estado = ((e->estado / freq) * total) + cum + (e->estado % freq);
}

void rans_enc_flush(rans_enc* e) {
    while (e->estado >= RANS_M) {
        e->saida.push_back(e->estado & 0xFF);
        e->estado >>= 8;
    }
    // * estado final serializado big-endian para que o decoder leia corretamente
    e->saida.push_back((e->estado >> 24) & 0xFF);
    e->saida.push_back((e->estado >> 16) & 0xFF);
    e->saida.push_back((e->estado >>  8) & 0xFF);
    e->saida.push_back( e->estado        & 0xFF);
}

void rans_dec_init(rans_dec* d, const u8* dados, u32 tam) {
    d->ptr = dados;
    d->fim = dados + tam;
    if (tam >= 4) {
        d->estado = (dados[0]<<24)|(dados[1]<<16)|(dados[2]<<8)|dados[3];
        d->ptr = dados + 4;
    } else {
        d->estado = RANS_M;
    }
}

u32 rans_dec_get(rans_dec* d, u32 freq, u32 cum, u32 total) {
    if (freq  == 0) freq  = 1;
    if (total == 0) total = 1;

    while (d->estado < RANS_M && d->ptr < d->fim)
        d->estado = (d->estado << 8) | *d->ptr++;

    if (d->estado == 0) return 0;

    u32 s = d->estado % total;
    d->estado = freq * (d->estado / total) + s - cum;
    return (s >= cum && s < cum + freq) ? 1 : 0;
}
