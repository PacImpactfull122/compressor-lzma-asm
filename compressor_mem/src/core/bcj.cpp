#include "../../include/bcj.h"
#include "../../include/tipos.h"

void bcj_x86_encode(u8* dados, u32 tam, u32 pos) {
    u32 prev_mask = 0;
    u32 prev_pos = (u32)-1;

    for (u32 i = 0; i < tam;) {
        // * jcc de 2 bytes: 0x0F 0x8x + 4 bytes de offset
        if (dados[i] == 0x0F && i + 1 < tam && (dados[i+1] & 0xF0) == 0x80) {
            if (i + 6 > tam) { i++; continue; }
            u32 src = dados[i+2] | (dados[i+3] << 8) | (dados[i+4] << 16) | (dados[i+5] << 24);
            u32 dest = src - (pos + i + 6);
            dados[i+2] = dest & 0xFF;
            dados[i+3] = (dest >> 8) & 0xFF;
            dados[i+4] = (dest >> 16) & 0xFF;
            dados[i+5] = (dest >> 24) & 0xFF;
            prev_mask = 0;
            prev_pos = i;
            i += 6;
            continue;
        }

        if (dados[i] != 0xE8 && dados[i] != 0xE9) {
            i++;
            continue;
        }

        // * a consulta da mascara le o ultimo byte do offset, o limite precisa vir antes
        if (i + 5 > tam) break;

        u32 offset = i - prev_pos;
        prev_pos = i;

        if (offset > 5) {
            prev_mask = 0;
        } else {
            prev_mask = (prev_mask << (offset - 1)) & 7;
            if (prev_mask != 0) {
                if ((prev_mask & 1) && dados[i + 4] == 0) {
                    prev_mask = (prev_mask << 1) | 1;
                    i++;
                    continue;
                }
                if ((prev_mask & 2) && dados[i + 4] == 0xFF) {
                    prev_mask = (prev_mask << 1) | 1;
                    i++;
                    continue;
                }
            }
        }

        u32 src = dados[i + 1] | (dados[i + 2] << 8) | (dados[i + 3] << 16) | (dados[i + 4] << 24);
        u32 dest = src - (pos + i + 5);

        dados[i + 1] = dest & 0xFF;
        dados[i + 2] = (dest >> 8) & 0xFF;
        dados[i + 3] = (dest >> 16) & 0xFF;
        dados[i + 4] = (dest >> 24) & 0xFF;

        prev_mask = (prev_mask << 1) | 1;
        i += 5;
    }
}

void bcj_x86_decode(u8* dados, u32 tam, u32 pos) {
    u32 prev_mask = 0;
    u32 prev_pos = (u32)-1;

    for (u32 i = 0; i < tam;) {
        if (dados[i] == 0x0F && i + 1 < tam && (dados[i+1] & 0xF0) == 0x80) {
            if (i + 6 > tam) { i++; continue; }
            u32 src = dados[i+2] | (dados[i+3] << 8) | (dados[i+4] << 16) | (dados[i+5] << 24);
            u32 dest = src + (pos + i + 6);
            dados[i+2] = dest & 0xFF;
            dados[i+3] = (dest >> 8) & 0xFF;
            dados[i+4] = (dest >> 16) & 0xFF;
            dados[i+5] = (dest >> 24) & 0xFF;
            prev_mask = 0;
            prev_pos = i;
            i += 6;
            continue;
        }

        if (dados[i] != 0xE8 && dados[i] != 0xE9) {
            i++;
            continue;
        }

        // * a consulta da mascara le o ultimo byte do offset, o limite precisa vir antes
        if (i + 5 > tam) break;

        u32 offset = i - prev_pos;
        prev_pos = i;

        if (offset > 5) {
            prev_mask = 0;
        } else {
            prev_mask = (prev_mask << (offset - 1)) & 7;
            if (prev_mask != 0) {
                if ((prev_mask & 1) && dados[i + 4] == 0) {
                    prev_mask = (prev_mask << 1) | 1;
                    i++;
                    continue;
                }
                if ((prev_mask & 2) && dados[i + 4] == 0xFF) {
                    prev_mask = (prev_mask << 1) | 1;
                    i++;
                    continue;
                }
            }
        }

        u32 src = dados[i + 1] | (dados[i + 2] << 8) | (dados[i + 3] << 16) | (dados[i + 4] << 24);
        u32 dest = src + (pos + i + 5);

        dados[i + 1] = dest & 0xFF;
        dados[i + 2] = (dest >> 8) & 0xFF;
        dados[i + 3] = (dest >> 16) & 0xFF;
        dados[i + 4] = (dest >> 24) & 0xFF;

        prev_mask = (prev_mask << 1) | 1;
        i += 5;
    }
}
