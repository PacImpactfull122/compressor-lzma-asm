#ifndef BITPLANE_H
#define BITPLANE_H

#include "tipos.h"
#include <vector>

std::vector<u8> bp_enc(const u8* dados, u32 tam);
std::vector<u8> bp_dec(const u8* ent, u32 tam_ent);

#endif