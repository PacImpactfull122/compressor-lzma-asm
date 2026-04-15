#ifndef DELTA_H
#define DELTA_H

#include "tipos.h"
#include <vector>

void delta_enc(u8* dados, u32 tam, u32 stride);
void delta_dec(u8* dados, u32 tam, u32 stride);

std::vector<u8> delta_audio_enc(const u8* dados, u32 tam);
std::vector<u8> delta_audio_dec(const u8* dados, u32 tam);

std::vector<u8> delta_2d_enc(const u8* dados, u32 tam, u32 largura);
std::vector<u8> delta_2d_dec(const u8* dados, u32 tam, u32 largura);

#endif