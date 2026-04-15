#ifndef HUFF_H
#define HUFF_H

#include "tipos.h"
#include <vector>

std::vector<u8> huff_enc(const u8* dados, u32 tam);
std::vector<u8> huff_dec(const u8* dados, u32 tam);

#endif