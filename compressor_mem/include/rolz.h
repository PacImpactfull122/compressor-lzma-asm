#ifndef ROLZ_H
#define ROLZ_H

#include "tipos.h"
#include <vector>

struct rolz_match {
    u32 len, idx;
};

std::vector<u8> rolz_enc(const u8* dados, u32 tam);
std::vector<u8> rolz_dec(const u8* dados, u32 tam);

#endif
