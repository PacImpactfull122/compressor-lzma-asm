#ifndef LZ_H
#define LZ_H

#include "tipos.h"
#include "config.h"

struct match {
    u32 len, dist;
};

struct lz_ctx {
    u32 head[HASH_TAM];
    u32 prev[WIN];
    u32 chain[WIN];
    u32 bt_left[WIN];
    u32 bt_right[WIN];
    // ! ultima_bt rastreia a ultima posicao inserida pelo bt4
    // * lz_ins usa isso para evitar dupla insercao quando lz_busca ja processou a posicao
    u32 ultima_bt;
};

void lz_init(lz_ctx* ctx);
match lz_busca(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 nivel);
// * retorna ate MAX_MULTI matches pareto-otimos: comprimento crescente, distancia decrescente
// * o caller deve verificar quantos matches validos foram retornados (len >= MIN_MATCH)
static constexpr int MAX_MULTI = 8;
int lz_busca_multi(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 nivel, match* saida);
void lz_ins(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 len);

#endif
