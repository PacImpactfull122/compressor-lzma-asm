#ifndef TIPOS_H
#define TIPOS_H
#include <cstdint>

using u8=uint8_t;
using u16=uint16_t;
using u32=uint32_t;
using u64=uint64_t;

struct cab {
    u8 magic[3];
    u8 flags;
    u32 tam_orig;
    u32 tam_comp;
    u16 crc;
} __attribute__((packed));

enum flags_jenga {
    FLAG_COMPRESSED = 0x80,
    FLAG_TRANSFORM = 0x40,
    TRANSFORM_BCJ = 0x00,
    TRANSFORM_DELTA_AUDIO = 0x10,
    TRANSFORM_DELTA_2D = 0x20,
    TRANSFORM_DELTA_STRIDE = 0x30,
    FLAG_RANS = 0x00,
    FLAG_HUFFMAN = 0x08,
    FLAG_CONTEXT = 0x04,
    BLOCK_64KB = 0x00,
    BLOCK_256KB = 0x01,
    BLOCK_1MB = 0x02,
    BLOCK_4MB = 0x03
};

#endif