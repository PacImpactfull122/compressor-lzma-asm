#include "include/codec.h"
#include "include/tipos.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: %s <entrada> <saida>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("erro ao abrir %s\n", argv[1]); return 1; }

    cab h;
    if (fread(&h, sizeof(h), 1, f) != 1) {
        printf("header invalido\n");
        fclose(f);
        return 1;
    }

    // ! magic incorreto indica arquivo nao gerado por este compressor
    if (h.magic[0] != 'L' || h.magic[1] != 'Z' || h.magic[2] != 'J') {
        printf("magic invalido\n");
        fclose(f);
        return 1;
    }

    u8* comp = (u8*)malloc(h.tam_comp);
    if (!comp) { printf("sem memoria\n"); fclose(f); return 1; }

    fread(comp, 1, h.tam_comp, f);
    fclose(f);

    // ! crc cobre apenas os dados comprimidos, nao o header
    u16 crc_calc = (u16)(crc32(comp, h.tam_comp) & 0xFFFF);
    if (crc_calc != h.crc) {
        printf("crc invalido: esperado %04x, calculado %04x\n", h.crc, crc_calc);
        free(comp);
        return 1;
    }

    auto dec = codec_decomp(comp, h.tam_comp, h.tam_orig);
    free(comp);

    FILE* out = fopen(argv[2], "wb");
    if (!out) { printf("erro ao criar %s\n", argv[2]); return 1; }

    fwrite(dec.data(), 1, dec.size(), out);
    fclose(out);

    printf("descomprimido: %u bytes\n", h.tam_orig);
    return 0;
}
