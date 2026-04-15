#include "../../include/lz.h"
#include "../../include/config.h"
#include <cstring>
#include <algorithm>

extern "C" {
    u32 compara_bytes_asm(const u8* a, const u8* b, u32 max_len);
}

void lz_init(lz_ctx* ctx) {
    memset(ctx, 0, sizeof(lz_ctx));
    ctx->ultima_bt = (u32)-1;
}

static u32 hash4(const u8* d, u32 p) {
    u32 h = *(const u32*)&d[p];
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h & (HASH_TAM - 1);
}

// * bt4: busca e insercao simultaneas na arvore binaria
// * a arvore e indexada por pos mascarado com WIN-1, colisoes descartam o candidato mais antigo
// * nivel controla profundidade maxima: nivel*16 descidas na arvore
// * retorna o match mais longo encontrado
static match bt4_busca(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 nivel) {
    if (pos + MIN_MATCH > tam) return {0, 0};

    u32 max_len  = std::min(MAX_MATCH, tam - pos);
    u32 melhor_len  = 0;
    u32 melhor_dist = 0;
    u32 limite   = nivel * 16;

    u32 h = hash4(dados, pos);
    u32 cur = ctx->head[h];

    // * ponteiros para os filhos do no "virtual" que sera inserido
    // * ao final da busca, left_ptr e right_ptr apontam para onde pos deve ser ligado
    u32* esq_ptr = &ctx->bt_left[pos & (WIN - 1)];
    u32* dir_ptr = &ctx->bt_right[pos & (WIN - 1)];
    *esq_ptr = 0;
    *dir_ptr = 0;

    while (cur != 0 && limite > 0) {
        if (cur >= pos || pos - cur >= WIN) {
            // ! candidato fora da janela ou invalido, corta o ramo
            *esq_ptr = 0;
            *dir_ptr = 0;
            break;
        }

        u32 dist = pos - cur;
        u32 len  = compara_bytes_asm(&dados[cur], &dados[pos], max_len);

        if (len > melhor_len) {
            melhor_len  = len;
            melhor_dist = dist;
            if (len >= max_len) {
                // * match maximo encontrado, liga os dois ramos e sai
                *esq_ptr = ctx->bt_left[cur  & (WIN - 1)];
                *dir_ptr = ctx->bt_right[cur & (WIN - 1)];
                break;
            }
        }

        // * desce na arvore comparando o byte na posicao len (primeiro byte diferente)
        if (dados[cur + len] < dados[pos + len]) {
            *esq_ptr = cur;
            esq_ptr  = &ctx->bt_right[cur & (WIN - 1)];
            cur      = *esq_ptr;
        } else {
            *dir_ptr = cur;
            dir_ptr  = &ctx->bt_left[cur & (WIN - 1)];
            cur      = *dir_ptr;
        }

        limite--;
    }

    // * atualiza head para apontar para pos
    ctx->prev[pos & (WIN - 1)] = ctx->head[h];
    ctx->head[h] = pos;
    ctx->ultima_bt = pos;

    return {melhor_len, melhor_dist};
}

match lz_busca(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 nivel) {
    return bt4_busca(ctx, dados, tam, pos, nivel);
}

int lz_busca_multi(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 nivel, match* saida) {
    if (pos + MIN_MATCH > tam) return 0;

    u32 max_len  = std::min(MAX_MATCH, tam - pos);
    u32 limite   = nivel * 16;

    u32 h   = hash4(dados, pos);
    u32 cur = ctx->head[h];

    u32* esq_ptr = &ctx->bt_left[pos & (WIN - 1)];
    u32* dir_ptr = &ctx->bt_right[pos & (WIN - 1)];
    *esq_ptr = 0;
    *dir_ptr = 0;

    // * melhor_dist: menor distancia encontrada para cada comprimento l
    u32 melhor_dist[MAX_MATCH + 1];
    for (u32 i = 0; i <= MAX_MATCH; i++) melhor_dist[i] = WIN;
    u32 melhor_global = 0;

    while (cur != 0 && limite > 0) {
        if (cur >= pos || pos - cur >= WIN) {
            *esq_ptr = 0;
            *dir_ptr = 0;
            break;
        }

        u32 dist = pos - cur;
        u32 len  = compara_bytes_asm(&dados[cur], &dados[pos], max_len);

        if (len >= MIN_MATCH) {
            for (u32 l = MIN_MATCH; l <= len; l++) {
                if (dist < melhor_dist[l]) melhor_dist[l] = dist;
            }
            if (len > melhor_global) melhor_global = len;
        }

        if (len >= max_len) {
            *esq_ptr = ctx->bt_left[cur  & (WIN - 1)];
            *dir_ptr = ctx->bt_right[cur & (WIN - 1)];
            break;
        }

        if (dados[cur + len] < dados[pos + len]) {
            *esq_ptr = cur;
            esq_ptr  = &ctx->bt_right[cur & (WIN - 1)];
            cur      = *esq_ptr;
        } else {
            *dir_ptr = cur;
            dir_ptr  = &ctx->bt_left[cur & (WIN - 1)];
            cur      = *dir_ptr;
        }

        limite--;
    }

    ctx->prev[pos & (WIN - 1)] = ctx->head[h];
    ctx->head[h] = pos;
    ctx->ultima_bt = pos;

    if (melhor_global < MIN_MATCH) return 0;

    int n = 0;
    u32 dist_ant = WIN;
    for (u32 l = MIN_MATCH; l <= melhor_global && n < MAX_MULTI; l++) {
        if (melhor_dist[l] < dist_ant) {
            saida[n++] = {l, melhor_dist[l]};
            dist_ant = melhor_dist[l];
        }
    }
    if (n == 0 || saida[n-1].len < melhor_global) {
        if (n < MAX_MULTI) saida[n++] = {melhor_global, melhor_dist[melhor_global]};
        else saida[n-1] = {melhor_global, melhor_dist[melhor_global]};
    }

    return n;
}

void lz_ins(lz_ctx* ctx, const u8* dados, u32 tam, u32 pos, u32 len) {
    for (u32 i = 0; i < len && pos + i + MIN_MATCH <= tam; i++) {
        // ! pula posicoes que ja foram inseridas pelo bt4_busca nesta mesma passagem
        if (pos + i == ctx->ultima_bt) continue;
        bt4_busca(ctx, dados, tam, pos + i, 1);
    }
}
