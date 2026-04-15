#include "../../include/rle.h"
#include <algorithm>

// * formato: byte nao-zero e literal, 0x00 e escape seguido de valor e contagem
// * isso significa que 0x00 literal nunca pode aparecer sozinho no stream

std::vector<u8> rle_enc(const u8* dados, u32 tam) {
    std::vector<u8> saida;
    if (tam == 0) return saida;

    u32 i = 0;
    while (i < tam) {
        u8 b = dados[i];
        u32 run = 1;
        while (i + run < tam && dados[i + run] == b && run < 255)
            run++;

        // ! qualquer sequencia com byte zero usa o escape, mesmo run=1
        // * isso garante que 0x00 nunca aparece como literal no stream
        if (b == 0 || run >= 4) {
            saida.push_back(0x00);
            saida.push_back(b);
            saida.push_back((u8)run);
        } else {
            for (u32 j = 0; j < run; j++)
                saida.push_back(b);
        }

        i += run;
    }

    return saida;
}

std::vector<u8> rle_dec(const u8* dados, u32 tam) {
    std::vector<u8> saida;
    u32 i = 0;

    while (i < tam) {
        u8 b = dados[i++];

        if (b == 0) {
            if (i + 1 >= tam) break;
            u8 val = dados[i++];
            u8 cnt = dados[i++];
            for (u32 j = 0; j < cnt; j++)
                saida.push_back(val);
        } else {
            saida.push_back(b);
        }
    }

    return saida;
}
