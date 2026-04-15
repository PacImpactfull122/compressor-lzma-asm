#ifndef CODEC_H
#define CODEC_H

#include "tipos.h"
#include <vector>

u32 crc32(const u8* dados, u32 tam);
std::vector<u8> codec_comp(const u8* dados, u32 tam, u32 nivel);
std::vector<u8> codec_decomp(const u8* dados, u32 tam, u32 tam_orig);

#endif
