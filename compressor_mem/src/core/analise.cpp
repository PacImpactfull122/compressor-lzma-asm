#include "../../include/analise.h"
#include "../../include/rle.h"
#include "../../include/rolz.h"
#include "../../include/ctx_rans.h"
#include "../../include/huff.h"
#include <cstring>
#include <algorithm>

void analisar_bloco(const u8* dados, u32 tam, info_bloco* info) {
    memset(info, 0, sizeof(info_bloco));
    if (tam == 0) return;

    u32 freq[256] = {0};
    u32 max_run   = 0;
    u32 run_atual = 1;
    u8  byte_ant  = dados[0];
    u32 max_freq  = 0;

    freq[dados[0]]++;

    for (u32 i = 1; i < tam; i++) {
        u8 b = dados[i];
        freq[b]++;
        if (b == byte_ant) {
            run_atual++;
            if (run_atual > max_run) max_run = run_atual;
        } else {
            run_atual = 1;
            byte_ant  = b;
        }
    }

    u32 unicos = 0;
    u32 zeros  = freq[0];
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) unicos++;
        if (freq[i] > max_freq) max_freq = freq[i];
    }

    info->entropia = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / tam;
            info->entropia -= p * log2(p);
        }
    }

    info->zeros_pct  = (zeros * 100) / tam;
    info->max_run    = max_run;
    info->unicos     = unicos;
    info->repetitivo = max_freq > (tam / 2);

    // * magic bytes elf/pe identificam executaveis com certeza no primeiro bloco
    // ! heuristico e8/e9 so e aplicado quando entropia < 7.0, dados aleatorios tem ~8.0
    u32 e8 = 0, e9 = 0;
    u32 lim = tam > 5 ? tam - 5 : 0;

    bool magic_exe = (tam >= 4) &&
        ((dados[0] == 0x7F && dados[1] == 'E' && dados[2] == 'L' && dados[3] == 'F') ||
         (dados[0] == 'M'  && dados[1] == 'Z'));

    for (u32 i = 0; i < lim; i++) {
        u8 prox = dados[i + 1];
        if (dados[i] == 0xE8 && prox != 0xE8 && prox != 0xE9 && prox != 0x00) e8++;
        if (dados[i] == 0xE9 && prox != 0xE8 && prox != 0xE9 && prox != 0x00) e9++;
    }
    bool heur_exe = (info->entropia < 7.0) && ((e8 + e9) > (tam / 150));
    info->executavel = magic_exe || heur_exe;

    u32 ascii = 0;
    for (u32 i = 0; i < tam; i++) {
        u8 b = dados[i];
        if ((b >= 32 && b <= 126) || b == 9 || b == 10 || b == 13)
            ascii++;
    }
    info->texto = ascii > (tam * 90 / 100);
}

// * amostra distribuida: inicio, meio e fim do bloco para evitar viés em dados bimodais
// * 3 segmentos de tamanho igual, concatenados, totalizam no maximo 65535 bytes
static u32 custo_amostra(const u8* dados, u32 tam, u32 metodo) {
    const u32 seg = std::min(tam / 3, 21845u);
    const u32 amostra = seg * 3;

    if (amostra == 0) return tam;

    // ! se o bloco e pequeno o suficiente, nao precisa de buffer intermediario
    if (tam <= 65535u) {
        if (metodo == METODO_RLE)      return (u32)rle_enc(dados, tam).size();
        if (metodo == METODO_ROLZ)     return (u32)rolz_enc(dados, tam).size();
        if (metodo == METODO_CTX_RANS) return (u32)ctx_rans_enc(dados, tam).size();
        if (metodo == METODO_HUFFMAN)  return (u32)huff_enc(dados, tam).size();
        return tam;
    }

    u8 buf[65535];
    u32 meio = (tam / 2) - (seg / 2);
    memcpy(buf,           dados,            seg);
    memcpy(buf + seg,     dados + meio,     seg);
    memcpy(buf + seg * 2, dados + tam - seg, seg);

    // * escala o custo da amostra para o tamanho real do bloco
    double escala = (double)tam / amostra;

    if (metodo == METODO_RLE)      return (u32)(rle_enc(buf, amostra).size()      * escala);
    if (metodo == METODO_ROLZ)     return (u32)(rolz_enc(buf, amostra).size()     * escala);
    if (metodo == METODO_CTX_RANS) return (u32)(ctx_rans_enc(buf, amostra).size() * escala);
    if (metodo == METODO_HUFFMAN)  return (u32)(huff_enc(buf, amostra).size()     * escala);

    return tam;
}

u32 escolher_metodo(const info_bloco* info, const u8* dados, u32 tam) {
    // * casos deterministas que nao precisam de amostragem
    if (info->executavel)      return METODO_BCJ_LZ;
    if (info->zeros_pct > 70)  return METODO_RLE;

    u32 melhor_metodo = METODO_LZ;
    u32 melhor_custo  = tam;

    u32 custo_rolz = custo_amostra(dados, tam, METODO_ROLZ);
    u32 custo_ctx  = custo_amostra(dados, tam, METODO_CTX_RANS);

    if (custo_rolz < melhor_custo) { melhor_custo = custo_rolz; melhor_metodo = METODO_ROLZ; }
    if (custo_ctx  < melhor_custo) { melhor_custo = custo_ctx;  melhor_metodo = METODO_CTX_RANS; }

    // * huffman e competitivo na faixa de entropia media onde ctx-rans tem overhead de modelo alto
    // * abaixo de 5.0 rolz/lz dominam, acima de 6.5 ctx-rans supera huffman
    if (info->entropia >= 5.0 && info->entropia <= 6.5) {
        u32 custo_huff = custo_amostra(dados, tam, METODO_HUFFMAN);
        if (custo_huff < melhor_custo) { melhor_custo = custo_huff; melhor_metodo = METODO_HUFFMAN; }
    }

    // ! se o melhor candidato nao bate o store, retorna store
    if (melhor_custo >= tam) return METODO_STORE;

    return melhor_metodo;
}
