#include "../../include/huff.h"
#include <algorithm>
#include <queue>
#include <cstring>

struct no_huff {
    u32 freq;
    u16 simbolo;
    no_huff* esq;
    no_huff* dir;
    no_huff(u32 f, u16 s) : freq(f), simbolo(s), esq(nullptr), dir(nullptr) {}
    no_huff(u32 f, no_huff* e, no_huff* d) : freq(f), simbolo(0xFFFF), esq(e), dir(d) {}
};

struct comp_no {
    bool operator()(no_huff* a, no_huff* b) { return a->freq > b->freq; }
};

static void gerar_codigos(no_huff* no, u32 cod, u8 bits, u32* codigos, u8* tam_bits) {
    if (!no) return;
    if (no->simbolo != 0xFFFF) {
        codigos[no->simbolo] = cod;
        tam_bits[no->simbolo] = bits;
        return;
    }
    gerar_codigos(no->esq, cod << 1,       bits + 1, codigos, tam_bits);
    gerar_codigos(no->dir, (cod << 1) | 1, bits + 1, codigos, tam_bits);
}

static void liberar(no_huff* no) {
    if (!no) return;
    liberar(no->esq);
    liberar(no->dir);
    delete no;
}

std::vector<u8> huff_enc(const u8* dados, u32 tam) {
    if (tam == 0) return {};

    u32 freq[256] = {0};
    for (u32 i = 0; i < tam; i++) freq[dados[i]]++;

    std::priority_queue<no_huff*, std::vector<no_huff*>, comp_no> fila;
    for (int i = 0; i < 256; i++)
        if (freq[i] > 0) fila.push(new no_huff(freq[i], i));

    if (fila.size() == 1) {
        no_huff* u = fila.top(); fila.pop();
        fila.push(new no_huff(u->freq, u, new no_huff(0, 256)));
    }

    while (fila.size() > 1) {
        no_huff* d = fila.top(); fila.pop();
        no_huff* e = fila.top(); fila.pop();
        fila.push(new no_huff(e->freq + d->freq, e, d));
    }

    no_huff* raiz = fila.top();
    u32 codigos[256] = {0};
    u8  tam_bits[256] = {0};
    gerar_codigos(raiz, 0, 0, codigos, tam_bits);
    liberar(raiz);

// * canonicaliza: ordena simbolos por profundidade e valor, reatribui codigos sequenciais
    u32 cnt[33] = {0};
    for (int i = 0; i < 256; i++)
        if (tam_bits[i] > 0 && tam_bits[i] <= 32) cnt[tam_bits[i]]++;

    u32 prox[33] = {0};
    for (int b = 1; b <= 32; b++)
        prox[b] = (prox[b-1] + cnt[b-1]) << 1;

    for (int i = 0; i < 256; i++) {
        if (tam_bits[i] == 0 || tam_bits[i] > 32) continue;
        u32 cod = prox[tam_bits[i]]++;
        // ! inverte bits para leitura lsb-first no encoder/decoder de bits
        u32 inv = 0;
        for (int b = 0; b < tam_bits[i]; b++)
            inv = (inv << 1) | ((cod >> b) & 1);
        codigos[i] = inv;
    }

    std::vector<u8> res;
    for (int i = 0; i < 256; i++) res.push_back(tam_bits[i]);
    res.push_back((tam >> 24) & 0xFF);
    res.push_back((tam >> 16) & 0xFF);
    res.push_back((tam >>  8) & 0xFF);
    res.push_back( tam        & 0xFF);

    u64 cache = 0;
    int nbits = 0;
    for (u32 i = 0; i < tam; i++) {
        u8 s = dados[i];
        cache |= ((u64)codigos[s] << nbits);
        nbits += tam_bits[s];
        while (nbits >= 8) { res.push_back(cache & 0xFF); cache >>= 8; nbits -= 8; }
    }
    if (nbits > 0) res.push_back(cache & 0xFF);

    return res;
}

// * decoder por lookup table de 15 bits
// * cada entrada da tabela guarda o simbolo e o numero de bits consumidos
// * para codigos mais curtos, a mesma entrada e replicada para todos os sufixos possiveis
// * isso elimina a travessia de arvore e reduz o decoder a um acesso de tabela por simbolo
#define HUFF_BITS 15

struct entrada_lut {
    u16 simbolo;
    u8  bits;
};

std::vector<u8> huff_dec(const u8* dados, u32 tam) {
    if (tam < 260) return {};

    u8  tam_bits[256];
    memcpy(tam_bits, dados, 256);
    u32 tam_orig = (dados[256]<<24)|(dados[257]<<16)|(dados[258]<<8)|dados[259];

    // * reconstroi codigos canonicos identicos ao encoder
    u32 cnt[33] = {0};
    for (int i = 0; i < 256; i++)
        if (tam_bits[i] > 0 && tam_bits[i] <= 32) cnt[tam_bits[i]]++;

    u32 prox[33] = {0};
    for (int b = 1; b <= 32; b++)
        prox[b] = (prox[b-1] + cnt[b-1]) << 1;

    u32 codigos[256] = {0};
    u32 tmp[33];
    memcpy(tmp, prox, sizeof(prox));
    for (int i = 0; i < 256; i++) {
        if (tam_bits[i] == 0 || tam_bits[i] > 32) continue;
        u32 cod = tmp[tam_bits[i]]++;
        u32 inv = 0;
        for (int b = 0; b < tam_bits[i]; b++)
            inv = (inv << 1) | ((cod >> b) & 1);
        codigos[i] = inv;
    }

    // * preenche a lookup table: para cada simbolo, replica a entrada em todas as posicoes
    // * que tem o codigo do simbolo como prefixo, os bits restantes sao sufixo irrelevante
    const u32 LUT_TAM = 1 << HUFF_BITS;
    std::vector<entrada_lut> lut(LUT_TAM, {0xFFFF, 0});

    for (int i = 0; i < 256; i++) {
        u8 nb = tam_bits[i];
        if (nb == 0 || nb > HUFF_BITS) continue;
        u32 cod = codigos[i];
        u32 rep = 1 << (HUFF_BITS - nb);
        for (u32 j = 0; j < rep; j++)
            lut[cod | (j << nb)] = {(u16)i, nb};
    }

    std::vector<u8> res;
    res.reserve(tam_orig);

    const u8* ptr  = dados + 260;
    u32 bytes_rest = tam - 260;
    u64 cache = 0;
    int nbits = 0;
    u32 pos   = 0;

    while (res.size() < tam_orig) {
        // * alimenta o cache ate ter pelo menos HUFF_BITS bits disponiveis
        while (nbits < HUFF_BITS && pos < bytes_rest) {
            cache |= ((u64)ptr[pos++] << nbits);
            nbits += 8;
        }

        if (nbits == 0) break;

        u32 idx = cache & (LUT_TAM - 1);
        entrada_lut e = lut[idx];

        // ! simbolo 0xFFFF indica codigo nao encontrado na tabela, stream corrompido
        if (e.simbolo == 0xFFFF || e.bits == 0) break;

        res.push_back((u8)e.simbolo);
        cache >>= e.bits;
        nbits  -= e.bits;
    }

    return res;
}
