#include "include/codec.h"
#include "include/tipos.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: %s <entrada> <saida> [nivel]\n", argv[0]);
        return 1;
    }

    u32 nivel = 5;
    if (argc >= 4) {
        nivel = atoi(argv[3]);
        if (nivel < 1 || nivel > 9) {
            printf("nivel invalido, usando 5\n");
            nivel = 5;
        }
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("erro ao abrir %s\n", argv[1]); return 1; }

    fseek(f, 0, SEEK_END);
    u32 tam = (u32)ftell(f);
    fseek(f, 0, SEEK_SET);

    u8* dados = (u8*)malloc(tam);
    if (!dados) { printf("sem memoria\n"); fclose(f); return 1; }

    fread(dados, 1, tam, f);
    fclose(f);

    auto comp = codec_comp(dados, tam, nivel);
    free(dados);

    // * monta o header cab antes de escrever
    cab h;
    h.magic[0] = 'L';
    h.magic[1] = 'Z';
    h.magic[2] = 'J';
    h.flags    = FLAG_COMPRESSED;
    h.tam_orig = tam;
    h.tam_comp = (u32)comp.size();
    h.crc      = (u16)(crc32(comp.data(), h.tam_comp) & 0xFFFF);

    FILE* out = fopen(argv[2], "wb");
    if (!out) { printf("erro ao criar %s\n", argv[2]); return 1; }

    fwrite(&h, sizeof(h), 1, out);
    fwrite(comp.data(), 1, comp.size(), out);
    fclose(out);

    printf("comprimido: %u -> %zu bytes (%.1f%%)\n",
           tam, comp.size(), 100.0 * comp.size() / tam);
    return 0;
}
