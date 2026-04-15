#include "../../include/ctx_rans.h"
#include <cstring>

// * range coder de ordem 1 com carry handling, algoritmo de Schindler
// * encoder e decoder processam i=0..tam-1 na mesma ordem
// * modelo adaptativo: freq indexado por contexto e byte, contexto e o byte anterior
// * normaliza quando range < RC_TOP, carry via buffer de bytes pendentes

#define RC_TOP (1u << 24)

void ctx_init(ctx_modelo* m) {
    for (u32 c = 0; c < CTX_TAM; c++) {
        for (int i = 0; i < 256; i++)
            m->freq[c][i] = 1;
        m->total[c] = 256;
    }
    m->ctx = 0;
}

void ctx_atualizar(ctx_modelo* m, u8 byte) {
    u8 c = m->ctx;
    m->freq[c][byte]++;
    m->total[c]++;
    // ! rescale quando total > RC_TOP para evitar r=0 na divisao range/total
    if (m->total[c] > RC_TOP) {
        m->total[c] = 0;
        for (int i = 0; i < 256; i++) {
            m->freq[c][i] = (m->freq[c][i] + 1) >> 1;
            m->total[c] += m->freq[c][i];
        }
    }
    m->ctx = byte;
}

static u32 cum_de(const ctx_modelo* m, u8 c, u8 byte) {
    u32 cum = 0;
    for (int j = 0; j < byte; j++) cum += m->freq[c][j];
    return cum;
}

// * carry handling: quando low += cum*r causa overflow, o byte anterior precisa ser incrementado
// * bytes 0xFF acumulam como pendentes porque podem virar 0x00 se houver carry posterior
struct enc_rc {
    u32 low, range;
    u32 pendentes;
    int primeiro;
    std::vector<u8> saida;

    enc_rc() : low(0), range(0xFFFFFFFF), pendentes(0), primeiro(-1) {}

    void emitir(u8 b) {
        if (primeiro < 0) {
            primeiro = b;
        } else {
            saida.push_back((u8)primeiro);
            while (pendentes > 0) {
                saida.push_back(primeiro == 0xFF ? 0x00 : 0xFF);
                pendentes--;
            }
            primeiro = b;
        }
    }

    void encode(u32 cum, u32 freq, u32 total) {
        u32 r = range / total;
        if (r == 0) r = 1;
        u32 low_novo = low + cum * r;

        if (low_novo < low) {
            if (primeiro >= 0) primeiro++;
            while (pendentes > 0) {
                saida.push_back((u8)primeiro);
                primeiro = 0x00;
                pendentes--;
            }
        }

        low   = low_novo;
        range = freq * r;
        if (range == 0) range = 1;

        while (range < RC_TOP) {
            u8 b = (u8)(low >> 24);
            if (b == 0xFF) pendentes++;
            else           emitir(b);
            low   <<= 8;
            range <<= 8;
        }
    }

    void flush() {
        for (int i = 0; i < 5; i++) {
            emitir((u8)(low >> 24));
            low <<= 8;
        }
        if (primeiro >= 0) {
            saida.push_back((u8)primeiro);
            while (pendentes > 0) { saida.push_back(0xFF); pendentes--; }
        }
    }
};

std::vector<u8> ctx_rans_enc(const u8* dados, u32 tam) {
    if (tam == 0) return {};

    std::vector<u8> res;
    res.push_back((tam >> 24) & 0xFF);
    res.push_back((tam >> 16) & 0xFF);
    res.push_back((tam >>  8) & 0xFF);
    res.push_back( tam        & 0xFF);

    ctx_modelo m;
    ctx_init(&m);
    enc_rc enc;

    for (u32 i = 0; i < tam; i++) {
        u8 c = (i > 0) ? dados[i - 1] : 0;
        u8 b = dados[i];
        enc.encode(cum_de(&m, c, b), m.freq[c][b], m.total[c]);
        ctx_atualizar(&m, b);
    }
    enc.flush();

    res.insert(res.end(), enc.saida.begin(), enc.saida.end());
    return res;
}

std::vector<u8> ctx_rans_dec(const u8* dados, u32 tam_ent) {
    if (tam_ent < 9) return {};

    const u8* ptr = dados;
    u32 tam_orig = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]; ptr += 4;
    // ! stream corrompido pode ter tam_orig absurdo, alocar gigabytes e travar
    if (tam_orig == 0 || tam_orig > 64u * 1024u * 1024u) return {};

    const u8* fim = dados + tam_ent;

    ctx_modelo m;
    ctx_init(&m);

    u32 low = 0, range = 0xFFFFFFFF, code = 0;
    for (int i = 0; i < 4; i++)
        code = (code << 8) | (ptr < fim ? *ptr++ : 0);

    // * tabela cumulativa lazy: recalcula apenas quando o contexto foi modificado
    u32 cum_tab[CTX_TAM][257];
    u32 versao_tab[CTX_TAM];
    u32 versao_mod[CTX_TAM];
    memset(versao_tab, 0, sizeof(versao_tab));
    memset(versao_mod, 0, sizeof(versao_mod));

    auto recalc = [&](u8 c) {
        cum_tab[c][0] = 0;
        for (int j = 0; j < 256; j++)
            cum_tab[c][j + 1] = cum_tab[c][j] + m.freq[c][j];
        versao_tab[c] = versao_mod[c];
    };
    for (u32 c = 0; c < CTX_TAM; c++) recalc(c);

    std::vector<u8> res(tam_orig);

    for (u32 i = 0; i < tam_orig; i++) {
        u8 c = (i > 0) ? res[i - 1] : 0;
        if (versao_tab[c] != versao_mod[c]) recalc(c);

        u32 tot = m.total[c];
        u32 r   = range / tot;
        if (r == 0) r = 1;
        u32 s = (code - low) / r;
        if (s >= tot) s = tot - 1;

        int lo = 0, hi = 255;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (cum_tab[c][mid + 1] <= s) lo = mid + 1;
            else hi = mid;
        }
        u8 byte = (u8)lo;

        low   += cum_tab[c][byte] * r;
        range  = m.freq[c][byte] * r;
        if (range == 0) range = 1;

        while (range < RC_TOP) {
            code  = (code << 8) | (ptr < fim ? *ptr++ : 0);
            low   <<= 8;
            range <<= 8;
        }

        res[i] = byte;
        u8 ctx_mod = m.ctx;
        ctx_atualizar(&m, byte);
        versao_mod[ctx_mod]++;
    }

    return res;
}
