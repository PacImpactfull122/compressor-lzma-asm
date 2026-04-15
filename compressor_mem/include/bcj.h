#ifndef BCJ_H
#define BCJ_H
#include <cstdint>

void bcj_x86_encode(uint8_t* data, uint32_t size, uint32_t pos);
void bcj_x86_decode(uint8_t* data, uint32_t size, uint32_t pos);

#endif
