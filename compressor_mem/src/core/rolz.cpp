#include "../../include/rolz.h"
#include "../../include/config.h"
#include <cstring>
#include <algorithm>

// * ROLZ: Rank-Order LZ
// * em vez de distancia absoluta, o match e identificado por um indice (rank)
// * dentro de uma tabela de 16 candidatos indexada pelo contexto dos 2 bytes anteriores
// * isso reduz os bits necessarios para codificar a referencia, 4 bits de indice vs distancia absoluta
// * tradeoff: janela efetiva menor, mas overhead por match e menor

struct rolz_ctx {
    u32 tab[65536][16];
    u8  cnt[65536];
};

static void rolz_init(rolz_ctx* ctx) {
    memset(ctx->tab, 0, sizeof(ctx->tab));
    memset(ctx->cnt, 0, sizeof(ctx->cnt));
}

// * hash = 2 bytes anteriores concatenados como u16
// * tanto encoder quanto decoder tem acesso a esses bytes no momento do lookup
static u16 rolz_hash(const u8* dados, u32 pos) {
    if (pos < 2) return 0;
    return (u16)((dados[pos - 2] << 8) | dados[pos - 1]);
}

static rolz_match rolz_busca(rolz_ctx* ctx, const u8* dados, u32 tam, u32 pos) {
    if (pos + 3 > tam) return {0, 0};

    u16 h = rolz_hash(dados, pos);
    u8  n = ctx->cnt[h];

    u32 melhor_len = 0, melhor_idx = 0;

    // ! max_len limitado a 255 porque len e serializado como u8 no stream
    u32 max_len = std::min(255u, tam - pos);

    for (u8 i = 0; i < n && i < 16; i++) {
        u32 ref = ctx->tab[h][i];
        if (ref >= pos || pos - ref > WIN) continue;

        u32 len = 0;
        while (len < max_len && dados[ref + len] == dados[pos + len])
            len++;

        if (len > melhor_len) {
            melhor_len = len;
            melhor_idx = i;
        }
    }

    return {melhor_len, melhor_idx};
}

static void rolz_add(rolz_ctx* ctx, const u8* dados, u32 pos) {
    u16 h = rolz_hash(dados, pos);

    if (ctx->cnt[h] < 16) {
        ctx->tab[h][ctx->cnt[h]++] = pos;
    } else {
        // * descarta o candidato mais antigo (indice 0), desloca os demais
        for (int i = 0; i < 15; i++)
            ctx->tab[h][i] = ctx->tab[h][i + 1];
        ctx->tab[h][15] = pos;
    }
}

// * formato de saida: tam_orig em 4 bytes, depois sequencia de tokens
// * token literal: flag 0x00 seguido do byte
// * token match: flag 0x01 seguido de len e idx, ambos u8
// ! len e idx sao u8, logo max_match efetivo e 255 e max candidatos e 16
std::vector<u8> rolz_enc(const u8* dados, u32 tam) {
    rolz_ctx ctx;
    rolz_init(&ctx);

    std::vector<u8> res;
    res.push_back((tam >> 24) & 0xFF);
    res.push_back((tam >> 16) & 0xFF);
    res.push_back((tam >>  8) & 0xFF);
    res.push_back( tam        & 0xFF);

    u32 pos = 0;
    while (pos < tam) {
        rolz_match m = rolz_busca(&ctx, dados, tam, pos);

        if (m.len >= 3) {
            res.push_back(1);
            res.push_back((u8)m.len);
            res.push_back((u8)m.idx);
            for (u32 i = 0; i < m.len && pos < tam; i++)
                rolz_add(&ctx, dados, pos++);
        } else {
            res.push_back(0);
            res.push_back(dados[pos]);
            rolz_add(&ctx, dados, pos++);
        }
    }

    return res;
}

std::vector<u8> rolz_dec(const u8* dados, u32 tam_ent) {
    if (tam_ent < 4) return {};

    const u8* ptr = dados;
    u32 tam_orig = (ptr[0]<<24)|(ptr[1]<<16)|(ptr[2]<<8)|ptr[3];
    ptr += 4;

    // ! tam_orig vem do stream, limitar para nao alocar demais
    if (tam_orig == 0 || tam_orig > 64u * 1024u * 1024u) return {};

    rolz_ctx ctx;
    rolz_init(&ctx);

    std::vector<u8> res;
    res.reserve(tam_orig);

    while (res.size() < tam_orig && ptr < dados + tam_ent) {
        u8 flag = *ptr++;

        if (flag == 0) {
            if (ptr >= dados + tam_ent) break;
            u8 byte = *ptr++;
            res.push_back(byte);
            rolz_add(&ctx, res.data(), (u32)res.size() - 1);
        } else {
            if (ptr + 1 >= dados + tam_ent) break;
            u8 len = *ptr++;
            u8 idx = *ptr++;

            u16 h = rolz_hash(res.data(), (u32)res.size());

            // ! idx fora do range indica stream corrompido
            if (idx >= ctx.cnt[h]) break;

            u32 ref = ctx.tab[h][idx];

            for (u8 i = 0; i < len && res.size() < tam_orig; i++) {
                u8 b = res[ref + i];
                res.push_back(b);
                rolz_add(&ctx, res.data(), (u32)res.size() - 1);
            }
        }
    }

    return res;
}
