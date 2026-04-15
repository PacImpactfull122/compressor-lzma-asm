#ifndef CONFIG_H
#define CONFIG_H

// ! win deve ser potencia de 2 e igual ao max dist suportado por FX_DIST em codec.cpp
constexpr unsigned WIN = 4194304;
constexpr unsigned HASH_BITS = 22;
constexpr unsigned HASH_TAM = 1 << HASH_BITS;
constexpr unsigned MIN_MATCH = 3;
// ! tabelas FX_LEN em codec.cpp suportam no maximo 258, aumentar aqui sem expandir as tabelas corrompe o stream
constexpr unsigned MAX_MATCH = 258;
constexpr unsigned RANS_M = 65536;
constexpr unsigned BLOCO_TAM = 4194304;
constexpr unsigned CTX_BITS = 8;
constexpr unsigned CTX_TAM = 1 << CTX_BITS;

#endif
