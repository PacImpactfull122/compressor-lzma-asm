#ifndef MTF_H
#define MTF_H

#include "tipos.h"
#include <vector>

std::vector<u8> mtf_enc(const u8* dados, u32 tam);
std::vector<u8> mtf_dec(const u8* dados, u32 tam);

#endif
