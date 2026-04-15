#ifndef RANS_LZ_H
#define RANS_LZ_H

#include "tipos.h"
#include <vector>

// * range coder bit-a-bit com modelo de contexto para stream LZ
// * literais: 8 modelos por bit, condicionados no byte anterior (256 contextos)
// * match flag, comprimento, distancia: modelos por posicao de bit
// * modelo adaptativo: prob[bit] = freq1 / (freq0 + freq1), rescale quando total > PROB_MAX

struct rans_lz_enc;
struct rans_lz_dec;

std::vector<u8> rans_lz_comp(const u8* dados, u32 tam, u32 nivel);
std::vector<u8> rans_lz_decomp(const u8* dados, u32 tam, u32 tam_orig);

#endif
