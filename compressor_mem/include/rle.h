#ifndef RLE_H
#define RLE_H

#include "tipos.h"
#include <vector>

std::vector<u8> rle_enc(const u8* dados, u32 tam);
std::vector<u8> rle_dec(const u8* dados, u32 tam);

#endif
