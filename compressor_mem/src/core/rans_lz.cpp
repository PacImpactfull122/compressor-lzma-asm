#include "../../include/rans_lz.h"
#include "../../include/lz.h"
#include "../../include/config.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdio>

// * tabelas de codificacao LZ77 — identicas as de codec.cpp
// * duplicadas aqui para manter o arquivo autocontido
struct fx { u32 base; u8 bits; };

static const fx FX_LEN[16] = {
    {3,0},{4,0},{5,0},{6,0},{7,1},{9,1},{11,2},{15,2},
    {19,3},{27,3},{35,4},{51,4},{67,5},{99,5},{131,6},{195,6}
};

static const fx FX_DIST[48] = {
    {1,0},{2,0},{3,0},{4,0},{5,1},{7,1},{9,2},{13,2},
    {17,3},{25,3},{33,4},{49,4},{65,5},{97,5},{129,6},{193,6},
    {257,7},{385,7},{513,8},{769,8},{1025,9},{1537,9},{2049,10},{3073,10},
    {4097,11},{6145,11},{8193,12},{12289,12},{16385,13},{24577,13},{32769,14},{49153,14},
    {65537,15},{98305,15},{131073,16},{196609,16},{262145,17},{393217,17},{524289,18},{786433,18},
    {1048577,19},{1572865,19},{2097153,20},{3145729,20},{3670017,21},{3932161,21},{4063233,21},{4128769,21}
};

static u8 cod_len(u32 l) {
    for (int i = 15; i >= 0; i--) if (l >= FX_LEN[i].base) return i;
    return 0;
}

static u8 cod_dist(u32 d) {
    for (int i = 47; i >= 0; i--) if (d >= FX_DIST[i].base) return i;
    return 0;
}

// * huff_lz minimo para o DP estimar custos — o parsing otimo para huffman
// * e uma boa aproximacao para range coder, nao vale reimplementar o DP inteiro
struct huff_lz {
    u32 freq[320];
    u32 cod[320];
    u8  nb[320];

    void construir(int n) {
        struct no { u32 freq; int idx; int esq, dir; };
        std::vector<no> nos(n);
        for (int i = 0; i < n; i++) nos[i] = {freq[i], i, -1, -1};

        std::vector<int> heap;
        for (int i = 0; i < n; i++) if (freq[i] > 0) heap.push_back(i);
        auto cmp = [&](int a, int b){ return nos[a].freq > nos[b].freq; };
        std::make_heap(heap.begin(), heap.end(), cmp);

        while (heap.size() > 1) {
            std::pop_heap(heap.begin(), heap.end(), cmp);
            int d = heap.back(); heap.pop_back();
            std::pop_heap(heap.begin(), heap.end(), cmp);
            int e = heap.back(); heap.pop_back();
            no novo = {nos[e].freq + nos[d].freq, -1, e, d};
            nos.push_back(novo);
            heap.push_back((int)nos.size() - 1);
            std::push_heap(heap.begin(), heap.end(), cmp);
        }

        memset(nb, 0, sizeof(nb));
        if (nos.empty()) return;
        int raiz = heap[0];
        struct frame { int idx; u8 prof; };
        std::vector<frame> pilha;
        pilha.push_back({raiz, 0});
        while (!pilha.empty()) {
            auto [idx, prof] = pilha.back(); pilha.pop_back();
            if (nos[idx].esq == -1 && nos[idx].dir == -1)
                nb[nos[idx].idx] = (u8)std::min((int)prof, 15);
            else {
                if (nos[idx].dir != -1) pilha.push_back({nos[idx].dir, (u8)(prof+1)});
                if (nos[idx].esq != -1) pilha.push_back({nos[idx].esq, (u8)(prof+1)});
            }
        }

        u32 kraft2 = 0;
        for (int i = 0; i < n; i++) if (nb[i]) kraft2 += (1u << (15 - nb[i]));
        if (kraft2 > (1u << 15)) {
            std::vector<int> ativos;
            for (int i = 0; i < n; i++) if (nb[i]) ativos.push_back(i);
            std::sort(ativos.begin(), ativos.end(), [&](int a, int b_) {
                if (nb[a] != nb[b_]) return nb[a] > nb[b_];
                return freq[a] < freq[b_];
            });
            for (int i : ativos) {
                if (kraft2 <= (1u << 15)) break;
                if (nb[i] >= 15) continue;
                kraft2 -= (1u << (15 - nb[i]));
                nb[i]++;
                kraft2 += (1u << (15 - nb[i]));
            }
        }

        u32 cnt[16] = {}, prox[16] = {};
        for (int i = 0; i < n; i++) if (nb[i]) cnt[nb[i]]++;
        for (int b = 1; b <= 15; b++) prox[b] = (prox[b-1] + cnt[b-1]) << 1;
        for (int i = 0; i < n; i++) {
            if (!nb[i]) { cod[i] = 0; continue; }
            u32 c = prox[nb[i]]++;
            u32 inv = 0;
            for (int b = 0; b < nb[i]; b++) inv = (inv << 1) | ((c >> b) & 1);
            cod[i] = inv;
        }
    }
};

static inline float custo_match(const float* c_len, const float* c_dist, u32 len, u32 dist) {
    u8 fc = cod_len(len);
    u8 fd = cod_dist(dist);
    return c_len[fc] + FX_LEN[fc].bits + c_dist[fd] + FX_DIST[fd].bits;
}

// * overhead_rep: custo de flag=2 + custo medio de rep_idx, calculado das frequencias reais
// * passado como parametro para que o DP use o valor correto por estado
static inline float custo_rep_match(const float* c_len, u32 len, float overhead) {
    u8 fc = cod_len(len);
    return overhead + c_len[fc] + FX_LEN[fc].bits;
}

// * range coder de Schindler com carry handling
// * identico ao enc_rc de ctx_rans.cpp
#define RC_TOP (1u << 24)

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

// * modelo de simbolo adaptativo com rescale
// * LIMIAR controla quando rescale ocorre — dist usa 131072, demais usam 32768
template<int N, u32 LIMIAR = 32768>
struct modelo_sim {
    u32 freq[N];
    u32 total;

    void init() {
        for (int i = 0; i < N; i++) freq[i] = 1;
        total = N;
    }

    u32 cum(int s) const {
        u32 c = 0;
        for (int i = 0; i < s; i++) c += freq[i];
        return c;
    }

    void atualizar(int s) {
        freq[s]++;
        total++;
        if (total > LIMIAR) {
            total = 0;
            for (int i = 0; i < N; i++) {
                freq[i] = (freq[i] + 1) >> 1;
                total += freq[i];
            }
        }
    }
};

// * estado do modelo completo para o stream LZ
// * literais condicionados no byte anterior do output descomprimido
// * flag: 0=literal, 1=match_normal, 2=rep_match
// * flag condicionado nos ultimos 2 tipos: est = tipo_atual*3 + tipo_anterior
// *   0=lit/lit, 1=lit/match, 2=lit/rep, 3=match/lit, 4=match/match, 5=match/rep
// *   6=rep/lit, 7=rep/match, 8=rep/rep
// * dist condicionado em fc e est mod 3, captura correlacao entre tipo anterior e distancia
// * alinha condicionado em fc — alinhamento de estruturas varia com comprimento do match
struct estado_modelo {
    modelo_sim<256>         lit[256];
    modelo_sim<3>           flag[9];
    modelo_sim<8>           rep_idx[9];
    modelo_sim<16>          comp_match[9];
    modelo_sim<16>          comp_rep[9];
    // * 3 contextos por fc: apos lit, apos match, apos rep
    modelo_sim<48, 131072>  dist[16][3];
    // * alinhamento condicionado no comprimento do match
    modelo_sim<16>          alinha[16];

    void init() {
        for (int i = 0; i < 256; i++) lit[i].init();
        for (int i = 0; i < 9; i++) flag[i].init();
        for (int i = 0; i < 9; i++) rep_idx[i].init();
        for (int i = 0; i < 9; i++) comp_match[i].init();
        for (int i = 0; i < 9; i++) comp_rep[i].init();
        for (int i = 0; i < 16; i++)
            for (int j = 0; j < 3; j++) dist[i][j].init();
        for (int i = 0; i < 16; i++) alinha[i].init();
    }
};

std::vector<u8> rans_lz_comp(const u8* dados, u32 tam, u32 nivel) {
    if (tam == 0) return {};

    struct sim { u16 s; u32 el; u8 fd; u32 ed; u8 rep; };

    huff_lz hll, hd;
    memset(hll.freq, 0, sizeof(hll.freq));
    memset(hd.freq,  0, sizeof(hd.freq));

    // * pass 1: greedy lazy para construir frequencias huffman para o DP
    {
        lz_ctx* ctx = new lz_ctx;
        lz_init(ctx);
        u32 pos = 0;
        while (pos < tam) {
            match m = lz_busca(ctx, dados, tam, pos, nivel);
            if (m.len >= MIN_MATCH && m.dist > 0 && m.dist <= pos) {
                if (pos + 1 < tam) {
                    match m2 = lz_busca(ctx, dados, tam, pos + 1, nivel);
                    if (m2.len > m.len && m2.dist > 0 && m2.dist <= pos + 1) {
                        hll.freq[dados[pos]]++;
                        lz_ins(ctx, dados, tam, pos, 1);
                        pos++;
                        m = m2;
                    }
                }
                hll.freq[256 + cod_len(m.len)]++;
                hd.freq[cod_dist(m.dist)]++;
                lz_ins(ctx, dados, tam, pos, m.len);
                pos += m.len;
            } else {
                hll.freq[dados[pos]]++;
                lz_ins(ctx, dados, tam, pos, 1);
                pos++;
            }
        }
        delete ctx;
    }

    for (int i = 0; i < 272; i++) if (!hll.freq[i]) hll.freq[i] = 1;
    for (int i = 0; i < 48;  i++) if (!hd.freq[i] && FX_DIST[i].base <= WIN) hd.freq[i] = 1;

    u32 tot_lit = 0, tot_len = 0, tot_dist = 0;
    for (int i = 0; i < 256; i++) tot_lit  += hll.freq[i];
    for (int i = 0; i < 16;  i++) tot_len  += hll.freq[256 + i];
    for (int i = 0; i < 48;  i++) tot_dist += hd.freq[i];

    float c_lit[256], c_len[16], c_dist[48];
    for (int i = 0; i < 256; i++) c_lit[i]  = -log2f((float)hll.freq[i]       / (float)tot_lit);
    for (int i = 0; i < 16;  i++) c_len[i]  = -log2f((float)hll.freq[256 + i] / (float)tot_len);
    for (int i = 0; i < 48;  i++) c_dist[i] = -log2f((float)hd.freq[i]        / (float)tot_dist);

    std::vector<sim> simbolos;
    simbolos.reserve(tam);

    // * dp_run agora rastreia dp_est[i] — estado do modelo em cada posicao
    // * permite calcular custos por estado no terceiro pass
    // * overhead_rep: custo de flag rep mais custo medio de rep_idx, por estado
    auto dp_run = [&](
        const float* cl,
        const float* clen_m,
        const float* clen_r,
        const float* cd,
        const float  overhead_rep[9],
        std::vector<u8>* dp_est_out
    ) {
        std::vector<sim> seq;
        seq.reserve(tam);

        const float INF = 1e30f;
        std::vector<float> dp(tam + 1, INF);
        std::vector<u32>   prev(tam + 1, 0);
        std::vector<u32>   mdist(tam + 1, 0);
        std::vector<u8>    dp_est(tam + 1, 0);
        dp[0] = 0.0f;

        // * rd_arr: rep_dists por posicao, flat com stride 8
        std::vector<u32> rd_arr((tam + 1) * 8, 1u);

        lz_ctx* ctx = new lz_ctx;
        lz_init(ctx);

        for (u32 i = 0; i < tam; i++) {
            if (dp[i] >= INF) continue;
            u32* rd = &rd_arr[i * 8];
            u8   est = dp_est[i];

            // * literal: atualiza est para refletir que o token atual e literal
            float c_l = dp[i] + cl[dados[i]];
            if (c_l < dp[i + 1]) {
                dp[i + 1]   = c_l;
                prev[i + 1] = 1;
                mdist[i + 1] = 0;
                dp_est[i + 1] = (u8)(0 * 3 + (est / 3));
                memcpy(&rd_arr[(i + 1) * 8], rd, 32);
            }

            // * rep_matches: verificar as 8 distancias recentes
            float ov_rep = overhead_rep[est];
            for (int j = 0; j < 8; j++) {
                u32 d = rd[j];
                if (d == 0 || d > i) continue;
                u32 max_l = std::min(tam - i, 258u);
                u32 l = 0;
                while (l < max_l && dados[i + l] == dados[i - d + l]) l++;
                if (l < (u32)MIN_MATCH) continue;
                u8 est_rep = (u8)(2 * 3 + (est / 3));
                for (u32 ll = (u32)MIN_MATCH; ll <= l && i + ll <= tam; ll++) {
                    float c = dp[i] + custo_rep_match(clen_r, ll, ov_rep);
                    if (c < dp[i + ll]) {
                        dp[i + ll]    = c;
                        prev[i + ll]  = ll;
                        mdist[i + ll] = d;
                        dp_est[i + ll] = est_rep;
                        u32* rn = &rd_arr[(i + ll) * 8];
                        memcpy(rn, rd, 32);
                        for (int k = 7; k > 0; k--) rn[k] = rn[k - 1];
                        rn[0] = d;
                    }
                }
            }

            match ms[MAX_MULTI];
            int nm = lz_busca_multi(ctx, dados, tam, i, nivel, ms);
            u8 est_match = (u8)(1 * 3 + (est / 3));
            for (int mi = 0; mi < nm; mi++) {
                if (ms[mi].dist == 0 || ms[mi].dist > i) continue;
                u32 l_ini = (mi == 0) ? MIN_MATCH : ms[mi - 1].len + 1;
                u32 l_fim = ms[mi].len;
                for (u32 l = l_ini; l <= l_fim && i + l <= tam; l++) {
                    float c = dp[i] + custo_match(clen_m, cd, l, ms[mi].dist);
                    if (c < dp[i + l]) {
                        dp[i + l]    = c;
                        prev[i + l]  = l;
                        mdist[i + l] = ms[mi].dist;
                        dp_est[i + l] = est_match;
                        u32* rn = &rd_arr[(i + l) * 8];
                        memcpy(rn, rd, 32);
                        for (int k = 7; k > 0; k--) rn[k] = rn[k - 1];
                        rn[0] = ms[mi].dist;
                    }
                }
            }
            lz_ins(ctx, dados, tam, i, 1);
        }
        delete ctx;

        if (dp_est_out) *dp_est_out = dp_est;

        std::vector<u32> caminho;
        u32 p = tam;
        while (p > 0) { caminho.push_back(p); p -= prev[p]; }
        std::reverse(caminho.begin(), caminho.end());

        u32 pos = 0;
        for (u32 fim : caminho) {
            u32 len  = fim - pos;
            u32 dist = mdist[fim];
            if (dist == 0) {
                for (u32 i = 0; i < len; i++)
                    seq.push_back({dados[pos + i], 0, 0, 0, 0xFF});
            } else {
                u8 fc = cod_len(len);
                u8 fd = cod_dist(dist);
                u32 el = FX_LEN[fc].bits  > 0 ? len  - FX_LEN[fc].base  : 0;
                u32 ed = FX_DIST[fd].bits > 0 ? dist - FX_DIST[fd].base : 0;
                // * verificar se dist esta nas rep_dists da posicao de origem
                // * rd_arr na posicao pos contem o estado das rep_dists antes deste simbolo
                u8 rep_idx_val = 0xFF;
                u32* rd_pos = &rd_arr[pos * 8];
                for (int j = 0; j < 8; j++) {
                    if (rd_pos[j] == dist) { rep_idx_val = (u8)j; break; }
                }
                seq.push_back({(u16)(256 + fc), el, fd, ed, rep_idx_val});
            }
            pos = fim;
        }
        return seq;
    };

    // * overhead inicial: estimativa conservadora antes de ter frequencias reais
    float ov_init[9];
    for (int i = 0; i < 9; i++) ov_init[i] = 5.0f;

    simbolos = dp_run(c_lit, c_len, c_len, c_dist, ov_init, nullptr);

    // * refinar custos com as frequencias reais da sequencia do primeiro DP
    // * separa flen_rep de flen_match e calcula overhead_rep por estado
    auto refinar = [&](const std::vector<sim>& seq, std::vector<u8>* dp_est_out) -> std::vector<sim> {
        u32 fl[256] = {}, flen_m[16] = {}, flen_r[16] = {}, fd_arr[48] = {};
        // * frequencias por estado para calcular overhead_rep real
        u32 ff[9][3] = {};
        u32 fri[9][8] = {};

        // * simular estado para calcular frequencias por estado
        u8 est_sim = 0;
        for (auto& s : seq) {
            if (s.s < 256) {
                fl[s.s]++;
                ff[est_sim][0]++;
                est_sim = (u8)(0 * 3 + (est_sim / 3));
            } else {
                u8 fc = (u8)(s.s - 256);
                if (s.rep != 0xFF) {
                    flen_r[fc]++;
                    ff[est_sim][2]++;
                    fri[est_sim][(int)s.rep]++;
                    est_sim = (u8)(2 * 3 + (est_sim / 3));
                } else {
                    flen_m[fc]++;
                    fd_arr[s.fd]++;
                    ff[est_sim][1]++;
                    est_sim = (u8)(1 * 3 + (est_sim / 3));
                }
            }
        }

        for (int i = 0; i < 256; i++) if (!fl[i])     fl[i]     = 1;
        for (int i = 0; i < 16;  i++) if (!flen_m[i]) flen_m[i] = 1;
        for (int i = 0; i < 16;  i++) if (!flen_r[i]) flen_r[i] = 1;
        for (int i = 0; i < 48;  i++) if (!fd_arr[i]) fd_arr[i] = 1;

        u32 tl = 0, tlen_m = 0, tlen_r = 0, td = 0;
        for (int i = 0; i < 256; i++) tl     += fl[i];
        for (int i = 0; i < 16;  i++) tlen_m += flen_m[i];
        for (int i = 0; i < 16;  i++) tlen_r += flen_r[i];
        for (int i = 0; i < 48;  i++) td     += fd_arr[i];

        float cl2[256], clen_m2[16], clen_r2[16], cd2[48];
        for (int i = 0; i < 256; i++) cl2[i]     = -log2f((float)fl[i]     / (float)tl);
        for (int i = 0; i < 16;  i++) clen_m2[i] = -log2f((float)flen_m[i] / (float)tlen_m);
        for (int i = 0; i < 16;  i++) clen_r2[i] = -log2f((float)flen_r[i] / (float)tlen_r);
        for (int i = 0; i < 48;  i++) cd2[i]     = -log2f((float)fd_arr[i] / (float)td);

        // * overhead_rep por estado: custo do flag rep mais custo medio do rep_idx
        float ov2[9];
        for (int e = 0; e < 9; e++) {
            u32 tf = ff[e][0] + ff[e][1] + ff[e][2];
            if (tf == 0) { ov2[e] = 5.0f; continue; }
            // ! garantir freq minima de 1 para evitar log(0)
            u32 f2 = ff[e][2] > 0 ? ff[e][2] : 1;
            float custo_flag = -log2f((float)f2 / (float)(tf + (ff[e][2] == 0 ? 1 : 0)));

            u32 tri = 0;
            for (int j = 0; j < 8; j++) tri += fri[e][j];
            float entropia_ri = 0.0f;
            if (tri > 0) {
                for (int j = 0; j < 8; j++) {
                    u32 fj = fri[e][j] > 0 ? fri[e][j] : 1;
                    entropia_ri += (float)fj / (float)(tri + 8) * (-log2f((float)fj / (float)(tri + 8)));
                }
            } else {
                entropia_ri = 3.0f;
            }
            ov2[e] = custo_flag + entropia_ri;
        }

        return dp_run(cl2, clen_m2, clen_r2, cd2, ov2, dp_est_out);
    };

    // * segundo pass: refinar com frequencias do greedy
    std::vector<u8> dp_est2;
    simbolos = refinar(simbolos, &dp_est2);

    // * terceiro pass: refinar com frequencias do segundo pass
    // * dp_est2 contem o estado por posicao do segundo pass — mais preciso
    simbolos = refinar(simbolos, nullptr);

    // * pass final: range coder com modelo de contexto e rep_match
    u32 n_sim = (u32)simbolos.size();
    std::vector<u8> saida;
    saida.push_back((n_sim >> 24) & 0xFF);
    saida.push_back((n_sim >> 16) & 0xFF);
    saida.push_back((n_sim >>  8) & 0xFF);
    saida.push_back( n_sim        & 0xFF);

    estado_modelo mdl;
    mdl.init();

    enc_rc enc;

    std::vector<u8> saida_sim;
    saida_sim.reserve(tam);

    u32 rep_dists[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    u8 est = 0;

    for (u32 k = 0; k < n_sim; k++) {
        auto& s = simbolos[k];
        u8 ctx = saida_sim.empty() ? 0 : saida_sim.back();

        if (s.s < 256) {
            enc.encode(mdl.flag[est].cum(0), mdl.flag[est].freq[0], mdl.flag[est].total);
            mdl.flag[est].atualizar(0);

            u8 byte = (u8)s.s;
            enc.encode(mdl.lit[ctx].cum(byte), mdl.lit[ctx].freq[byte], mdl.lit[ctx].total);
            mdl.lit[ctx].atualizar(byte);

            saida_sim.push_back(byte);
            est = 0 * 3 + (est / 3);
        } else {
            u8 fc = (u8)(s.s - 256);
            u32 len  = FX_LEN[fc].base + s.el;
            u32 dist = FX_DIST[s.fd].base + s.ed;

            int ri = -1;
            for (int j = 0; j < 8; j++) {
                if (rep_dists[j] == dist) { ri = j; break; }
            }

            if (ri >= 0) {
                enc.encode(mdl.flag[est].cum(2), mdl.flag[est].freq[2], mdl.flag[est].total);
                mdl.flag[est].atualizar(2);

                enc.encode(mdl.rep_idx[est].cum(ri), mdl.rep_idx[est].freq[ri], mdl.rep_idx[est].total);
                mdl.rep_idx[est].atualizar(ri);

                enc.encode(mdl.comp_rep[est].cum(fc), mdl.comp_rep[est].freq[fc], mdl.comp_rep[est].total);
                mdl.comp_rep[est].atualizar(fc);

                if (FX_LEN[fc].bits > 0)
                    enc.encode(s.el, 1, 1u << FX_LEN[fc].bits);

                for (int j = ri; j > 0; j--) rep_dists[j] = rep_dists[j - 1];
                rep_dists[0] = dist;
                est = 2 * 3 + (est / 3);
            } else {
                enc.encode(mdl.flag[est].cum(1), mdl.flag[est].freq[1], mdl.flag[est].total);
                mdl.flag[est].atualizar(1);

                enc.encode(mdl.comp_match[est].cum(fc), mdl.comp_match[est].freq[fc], mdl.comp_match[est].total);
                mdl.comp_match[est].atualizar(fc);

                // * dist condicionado em [fc][est%3] — tipo anterior influencia distribuicao de distancias
                u8 ctx_d = est % 3;
                enc.encode(mdl.dist[fc][ctx_d].cum(s.fd), mdl.dist[fc][ctx_d].freq[s.fd], mdl.dist[fc][ctx_d].total);
                mdl.dist[fc][ctx_d].atualizar(s.fd);

                if (FX_LEN[fc].bits > 0)
                    enc.encode(s.el, 1, 1u << FX_LEN[fc].bits);
                if (FX_DIST[s.fd].bits > 0) {
                    u8 nb = FX_DIST[s.fd].bits;
                    if (nb >= 4) {
                        u32 altos  = s.ed >> 4;
                        u32 baixos = s.ed & 0xF;
                        if (nb > 4) enc.encode(altos, 1, 1u << (nb - 4));
                        // * alinha condicionado em fc — alinhamento varia com comprimento
                        enc.encode(mdl.alinha[fc].cum(baixos), mdl.alinha[fc].freq[baixos], mdl.alinha[fc].total);
                        mdl.alinha[fc].atualizar(baixos);
                    } else {
                        enc.encode(s.ed, 1, 1u << nb);
                    }
                }

                for (int j = 7; j > 0; j--) rep_dists[j] = rep_dists[j - 1];
                rep_dists[0] = dist;
                est = 1 * 3 + (est / 3);
            }

            u32 p = (u32)saida_sim.size();
            if (dist > 0 && dist <= p) {
                u32 src = p - dist;
                saida_sim.resize(p + len);
                for (u32 i = 0; i < len; i++)
                    saida_sim[p + i] = saida_sim[src + i];
            }
        }
    }

    enc.flush();
    saida.insert(saida.end(), enc.saida.begin(), enc.saida.end());
    return saida;
}

std::vector<u8> rans_lz_decomp(const u8* dados, u32 tam, u32 tam_orig) {
    if (tam < 4) return {};

    u32 n_sim = ((u32)dados[0] << 24) | ((u32)dados[1] << 16)
              | ((u32)dados[2] <<  8) | dados[3];
    if (n_sim == 0 || n_sim > 64u * 1024u * 1024u) return {};

    const u8* ptr = dados + 4;
    const u8* fim = dados + tam;

    u32 low = 0, range = 0xFFFFFFFF, code = 0;
    for (int i = 0; i < 4; i++)
        code = (code << 8) | (ptr < fim ? *ptr++ : 0);

    auto rc_decode = [&](u32 total) -> u32 {
        u32 r = range / total;
        if (r == 0) r = 1;
        u32 s = (code - low) / r;
        if (s >= total) s = total - 1;
        return s;
    };

    auto decode_sim = [&](u32 cum, u32 freq, u32 total) {
        u32 r = range / total;
        if (r == 0) r = 1;
        low   += cum * r;
        range  = freq * r;
        if (range == 0) range = 1;
        while (range < RC_TOP) {
            code  = (code << 8) | (ptr < fim ? *ptr++ : 0);
            low   <<= 8;
            range <<= 8;
        }
    };

    estado_modelo mdl;
    mdl.init();

    std::vector<u8> res;
    res.reserve(tam_orig);

    u32 rep_dists[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    u8 est = 0;

    for (u32 k = 0; k < n_sim && res.size() < tam_orig; k++) {
        u8 ctx = res.empty() ? 0 : res.back();

        u32 s_flag = rc_decode(mdl.flag[est].total);
        int flag_val = 0;
        {
            u32 c = 0;
            for (int i = 0; i < 3; i++) {
                if (s_flag >= c && s_flag < c + mdl.flag[est].freq[i]) { flag_val = i; break; }
                c += mdl.flag[est].freq[i];
            }
        }
        decode_sim(mdl.flag[est].cum(flag_val), mdl.flag[est].freq[flag_val], mdl.flag[est].total);
        mdl.flag[est].atualizar(flag_val);

        if (flag_val == 0) {
            u32 s_lit = rc_decode(mdl.lit[ctx].total);
            int byte_val = 0;
            {
                u32 c = 0;
                for (int i = 0; i < 256; i++) {
                    if (s_lit >= c && s_lit < c + mdl.lit[ctx].freq[i]) { byte_val = i; break; }
                    c += mdl.lit[ctx].freq[i];
                }
            }
            decode_sim(mdl.lit[ctx].cum(byte_val), mdl.lit[ctx].freq[byte_val], mdl.lit[ctx].total);
            mdl.lit[ctx].atualizar(byte_val);
            res.push_back((u8)byte_val);
            est = 0 * 3 + (est / 3);
        } else if (flag_val == 2) {
            u32 s_ri = rc_decode(mdl.rep_idx[est].total);
            int ri = 0;
            {
                u32 c = 0;
                for (int i = 0; i < 8; i++) {
                    if (s_ri >= c && s_ri < c + mdl.rep_idx[est].freq[i]) { ri = i; break; }
                    c += mdl.rep_idx[est].freq[i];
                }
            }
            decode_sim(mdl.rep_idx[est].cum(ri), mdl.rep_idx[est].freq[ri], mdl.rep_idx[est].total);
            mdl.rep_idx[est].atualizar(ri);

            u32 s_comp = rc_decode(mdl.comp_rep[est].total);
            int fc = 0;
            {
                u32 c = 0;
                for (int i = 0; i < 16; i++) {
                    if (s_comp >= c && s_comp < c + mdl.comp_rep[est].freq[i]) { fc = i; break; }
                    c += mdl.comp_rep[est].freq[i];
                }
            }
            decode_sim(mdl.comp_rep[est].cum(fc), mdl.comp_rep[est].freq[fc], mdl.comp_rep[est].total);
            mdl.comp_rep[est].atualizar(fc);

            u32 len = FX_LEN[fc].base;
            if (FX_LEN[fc].bits > 0) {
                u32 tot = 1u << FX_LEN[fc].bits;
                u32 el  = rc_decode(tot);
                decode_sim(el, 1, tot);
                len += el;
            }

            u32 dist = rep_dists[ri];
            for (int j = ri; j > 0; j--) rep_dists[j] = rep_dists[j - 1];
            rep_dists[0] = dist;

            if (res.size() + len > tam_orig) len = tam_orig - (u32)res.size();
            if (dist == 0 || dist > res.size()) return {};

            u32 p   = (u32)res.size();
            u32 src = p - dist;
            res.resize(p + len);
            for (u32 i = 0; i < len; i++)
                res[p + i] = res[src + i];
            est = 2 * 3 + (est / 3);
        } else {
            u32 s_comp = rc_decode(mdl.comp_match[est].total);
            int fc = 0;
            {
                u32 c = 0;
                for (int i = 0; i < 16; i++) {
                    if (s_comp >= c && s_comp < c + mdl.comp_match[est].freq[i]) { fc = i; break; }
                    c += mdl.comp_match[est].freq[i];
                }
            }
            decode_sim(mdl.comp_match[est].cum(fc), mdl.comp_match[est].freq[fc], mdl.comp_match[est].total);
            mdl.comp_match[est].atualizar(fc);

                // * dist condicionado em fc e est mod 3, espelha o encoder
            u8 ctx_d = est % 3;
            u32 s_dist = rc_decode(mdl.dist[fc][ctx_d].total);
            int fd = 0;
            {
                u32 c = 0;
                for (int i = 0; i < 48; i++) {
                    if (s_dist >= c && s_dist < c + mdl.dist[fc][ctx_d].freq[i]) { fd = i; break; }
                    c += mdl.dist[fc][ctx_d].freq[i];
                }
            }
            decode_sim(mdl.dist[fc][ctx_d].cum(fd), mdl.dist[fc][ctx_d].freq[fd], mdl.dist[fc][ctx_d].total);
            mdl.dist[fc][ctx_d].atualizar(fd);

            u32 len = FX_LEN[fc].base;
            if (FX_LEN[fc].bits > 0) {
                u32 tot = 1u << FX_LEN[fc].bits;
                u32 el  = rc_decode(tot);
                decode_sim(el, 1, tot);
                len += el;
            }

            u32 dist = FX_DIST[fd].base;
            if (FX_DIST[fd].bits > 0) {
                u8 nb = FX_DIST[fd].bits;
                if (nb >= 4) {
                    u32 altos = 0;
                    if (nb > 4) {
                        u32 tot = 1u << (nb - 4);
                        altos = rc_decode(tot);
                        decode_sim(altos, 1, tot);
                    }
                    u32 s_baixos = rc_decode(mdl.alinha[fc].total);
                    int baixos = 0;
                    {
                        u32 c = 0;
                        for (int i = 0; i < 16; i++) {
                            if (s_baixos >= c && s_baixos < c + mdl.alinha[fc].freq[i]) { baixos = i; break; }
                            c += mdl.alinha[fc].freq[i];
                        }
                    }
                    decode_sim(mdl.alinha[fc].cum(baixos), mdl.alinha[fc].freq[baixos], mdl.alinha[fc].total);
                    mdl.alinha[fc].atualizar(baixos);
                    dist += (altos << 4) | (u32)baixos;
                } else {
                    u32 tot = 1u << nb;
                    u32 ed  = rc_decode(tot);
                    decode_sim(ed, 1, tot);
                    dist += ed;
                }
            }

            for (int j = 7; j > 0; j--) rep_dists[j] = rep_dists[j - 1];
            rep_dists[0] = dist;

            if (res.size() + len > tam_orig) len = tam_orig - (u32)res.size();
            if (dist == 0 || dist > res.size()) return {};

            u32 p   = (u32)res.size();
            u32 src = p - dist;
            res.resize(p + len);
            for (u32 i = 0; i < len; i++)
                res[p + i] = res[src + i];
            est = 1 * 3 + (est / 3);
        }
    }

    return res;
}
