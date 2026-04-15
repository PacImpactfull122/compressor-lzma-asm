#include "../../include/analise.h"
#include "../../include/tipos.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    u32 tam = (u32)ftell(f);
    fseek(f, 0, SEEK_SET);
    u8* dados = (u8*)malloc(tam);
    fread(dados, 1, tam, f);
    fclose(f);

    u32 pos = 0;
    int bloco = 0;
    while (pos < tam) {
        u32 btam = std::min(4194304u, tam - pos);
        info_bloco info;
        analisar_bloco(&dados[pos], btam, &info);
        u32 met = escolher_metodo(&info, &dados[pos], btam);
        printf("bloco %d: tam=%u metodo=%u executavel=%d entropia=%.3f\n",
               bloco++, btam, met, (int)info.executavel, info.entropia);
        pos += btam;
    }
    free(dados);
    return 0;
}
