#include "../../include/mtf.h"
#include <cstring>

static void mtf_lista_inicial(u8* lista) {
    for (u32 i = 0; i < 256; i++)
        lista[i] = (u8)i;
}

std::vector<u8> mtf_enc(const u8* dados, u32 tam) {
    std::vector<u8> saida(tam);
    u8 lista[256];
    mtf_lista_inicial(lista);

    for (u32 i = 0; i < tam; i++) {
        u8 byte = dados[i];
        u32 idx = 0;
        while (lista[idx] != byte && idx < 255)
            idx++;
        saida[i] = (u8)idx;
        for (u32 j = idx; j > 0; j--)
            lista[j] = lista[j - 1];
        lista[0] = byte;
    }
    return saida;
}

std::vector<u8> mtf_dec(const u8* dados, u32 tam) {
    std::vector<u8> saida(tam);
    u8 lista[256];
    mtf_lista_inicial(lista);

    for (u32 i = 0; i < tam; i++) {
        u8 idx = dados[i];
        u8 byte = lista[idx];
        saida[i] = byte;
        for (u32 j = idx; j > 0; j--)
            lista[j] = lista[j - 1];
        lista[0] = byte;
    }
    return saida;
}
