#include "../../include/delta.h"
#include "../../include/tipos.h"
#include <cstring>

using i16 = int16_t;

void delta_enc(u8* dados, u32 tam, u32 stride) {
    if (tam < stride) return;
    // ! iteracao decrescente para que dados[i-stride] ainda seja o valor original
    for (u32 i = tam - 1; i >= stride; i--)
        dados[i] -= dados[i - stride];
}

void delta_dec(u8* dados, u32 tam, u32 stride) {
    if (tam < stride) return;
    for (u32 i = stride; i < tam; i++)
        dados[i] += dados[i - stride];
}

std::vector<u8> delta_audio_enc(const u8* dados, u32 tam) {
    std::vector<u8> res(tam);
    memcpy(res.data(), dados, tam);

    // ! iteracao decrescente para que res[i-2] ainda seja o valor original
    if (tam >= 4) {
        u32 inicio = (tam % 2 == 0) ? tam - 2 : tam - 3;
        for (u32 i = inicio; i >= 2; i -= 2) {
            i16 cur  = *(i16*)&res[i];
            i16 prev = *(i16*)&res[i - 2];
            *(i16*)&res[i] = cur - prev;
        }
    }

    return res;
}

std::vector<u8> delta_audio_dec(const u8* dados, u32 tam) {
    std::vector<u8> res(tam);
    memcpy(res.data(), dados, tam);

    for (u32 i = 2; i + 1 < tam; i += 2) {
        i16 diff = *(i16*)&res[i];
        i16 prev = *(i16*)&res[i - 2];
        *(i16*)&res[i] = diff + prev;
    }

    return res;
}

std::vector<u8> delta_2d_enc(const u8* dados, u32 tam, u32 largura) {
    std::vector<u8> res(tam);
    memcpy(res.data(), dados, tam);

    if (largura == 0 || tam < largura) return res;

    u32 altura = tam / largura;

    // * vertical primeiro, depois horizontal
    // * decoder precisa reverter na ordem inversa: horizontal depois vertical
    for (u32 y = 1; y < altura; y++) {
        for (u32 x = 0; x < largura; x++) {
            u32 pos = y * largura + x;
            res[pos] -= res[(y - 1) * largura + x];
        }
    }

    for (u32 y = 0; y < altura; y++) {
        for (u32 x = 1; x < largura; x++) {
            u32 pos = y * largura + x;
            res[pos] -= res[y * largura + (x - 1)];
        }
    }

    return res;
}

std::vector<u8> delta_2d_dec(const u8* dados, u32 tam, u32 largura) {
    std::vector<u8> res(tam);
    memcpy(res.data(), dados, tam);

    if (largura == 0 || tam < largura) return res;

    u32 altura = tam / largura;

    // ! loop com u32 decrementando ate 0 causa underflow, usar int com cast
    for (u32 y = 0; y < altura; y++) {
        for (int x = (int)largura - 1; x >= 1; x--) {
            u32 pos = y * largura + (u32)x;
            res[pos] += res[y * largura + (u32)(x - 1)];
        }
    }

    for (int y = (int)altura - 1; y >= 1; y--) {
        for (u32 x = 0; x < largura; x++) {
            u32 pos = (u32)y * largura + x;
            res[pos] += res[(u32)(y - 1) * largura + x];
        }
    }

    return res;
}
