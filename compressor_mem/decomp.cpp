#include "include/codec.h"
#include "include/tipos.h"
#include "include/config.h"
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

    fseek(f, 0, SEEK_END);
    long tam_arquivo = ftell(f);
    // * posiciona depois do header, o fread dos dados parte daqui
    fseek(f, (long)sizeof(cab), SEEK_SET);

    // ! campos do header vem do arquivo, validar antes de alocar
    if (tam_arquivo < (long)sizeof(cab)) {
        printf("arquivo truncado\n");
        fclose(f);
        return 1;
    }
    if (h.tam_comp > (u32)(tam_arquivo - (long)sizeof(cab))) {
        printf("tam_comp invalido: %u\n", h.tam_comp);
        fclose(f);
        return 1;
    }
    // ! bloco comprimido expande no maximo a bloco_tam, limite folgado contra tam_orig corrompido
    if ((u64)h.tam_orig > (u64)h.tam_comp * BLOCO_TAM + 1024 * 1024) {
        printf("tam_orig invalido: %u\n", h.tam_orig);
        fclose(f);
        return 1;
    }

    u8* comp = (u8*)malloc(h.tam_comp);
    if (!comp) { printf("sem memoria\n"); fclose(f); return 1; }

    if (fread(comp, 1, h.tam_comp, f) != h.tam_comp) {
        printf("arquivo truncado\n");
        free(comp);
        fclose(f);
        return 1;
    }
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

    // ! saida menor que o prometido indica bloco corrompido
    if (dec.size() != h.tam_orig) {
        printf("dados corrompidos: esperado %u bytes, obtido %zu\n", h.tam_orig, dec.size());
        return 1;
    }

    FILE* out = fopen(argv[2], "wb");
    if (!out) { printf("erro ao criar %s\n", argv[2]); return 1; }

    fwrite(dec.data(), 1, dec.size(), out);
    fclose(out);

    printf("descomprimido: %u bytes\n", h.tam_orig);
    return 0;
}
