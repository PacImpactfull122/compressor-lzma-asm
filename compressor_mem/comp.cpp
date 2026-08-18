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
    long tam_arq = ftell(f);
    fseek(f, 0, SEEK_SET);

    // ! formato nao suporta arquivos maiores que quatro gigabytes, acima disso truncaria
    if (tam_arq < 0 || tam_arq > (long)0xFFFFFF00) {
        printf("arquivo grande demais: %ld bytes\n", tam_arq);
        fclose(f);
        return 1;
    }
    u32 tam = (u32)tam_arq;

    u8* dados = (u8*)malloc(tam);
    if (!dados) { printf("sem memoria\n"); fclose(f); return 1; }

    if (tam > 0 && fread(dados, 1, tam, f) != tam) {
        printf("erro ao ler %s\n", argv[1]);
        free(dados);
        fclose(f);
        return 1;
    }
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

    // ! arquivo vazio gera stream sem dados, o descompressor trata normalmente
    if (fwrite(&h, sizeof(h), 1, out) != 1 ||
        (comp.size() > 0 && fwrite(comp.data(), 1, comp.size(), out) != comp.size())) {
        printf("erro ao escrever %s\n", argv[2]);
        fclose(out);
        return 1;
    }
    fclose(out);

    if (tam > 0)
        printf("comprimido: %u -> %zu bytes (%.1f%%)\n",
               tam, comp.size(), 100.0 * comp.size() / tam);
    else
        printf("comprimido: %u -> %zu bytes\n", tam, comp.size());
    return 0;
}
