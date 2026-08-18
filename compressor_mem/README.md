# compressor-lzma-asm

Compressor de dados com pipeline multi-estágio implementado em C++ e assembly x86-64.

## Arquitetura

O pipeline de compressão analisa o bloco de entrada e escolhe dinamicamente a combinação de transformações e codificadores mais eficiente para aquele dado.

```
entrada
  └── analise (entropia, tipo de dado)
        └── transformacao opcional
        │     ├── BCJ        (executaveis x86)
        │     ├── Delta      (audio, imagens, dados com stride)
        │     └── RLE        (sequencias repetidas)
        └── codificador LZ
        │     ├── LZ77 + BT4 (binary tree, busca em assembly)
        │     └── ROLZ       (rank-ordered, contexto de 2 bytes)
        └── codificador entropico
              ├── rANS        (range asymmetric numeral systems)
              ├── rANS + contexto de ordem 1
              ├── Huffman
              └── Bitplane
```

## Componentes

| Arquivo | Funcao |
|---|---|
| `src/core/lz.cpp` | LZ77 com binary tree BT4, busca otimizada em ASM |
| `src/core/rolz.cpp` | ROLZ com tabela de contexto de 2 bytes |
| `src/core/rans.cpp` | rANS simetrico, tabelas de encode/decode |
| `src/core/ctx_rans.cpp` | rANS com modelo de contexto de ordem 1 |
| `src/core/huff.cpp` | Huffman canonico |
| `src/core/bitplane.cpp` | Codificacao por planos de bits |
| `src/core/bcj.cpp` | Filtro BCJ para executaveis x86 |
| `src/core/delta.cpp` | Filtro delta com suporte a stride variavel |
| `src/core/rle.cpp` | Run-length encoding |
| `src/core/analise.cpp` | Analise de entropia e selecao de metodo |
| `src/core/codec.cpp` | Orquestrador do pipeline |
| `src/asm/bt4_search.asm` | Busca BT4 em SSE2/AVX2 |
| `src/asm/bt4_opt.asm` | Otimizacao de candidatos BT4 |
| `src/asm/hash_compare.asm` | Comparacao de hash vetorizada |
| `src/asm/rans_simd.asm` | rANS com SIMD |

## Formato do arquivo `.lz`

```
[magic: 3 bytes "LZJ"] [flags: 1 byte] [tam_orig: 4 bytes] [tam_comp: 4 bytes] [crc: 2 bytes]
[blocos...]
```

Cada bloco: `[metodo: 1 byte] [tam_orig_bloco: 4 bytes] [tam_comp_bloco: 4 bytes] [dados...]`

O metodo por bloco e escolhido dinamicamente pela analise de entropia.

O descompressor valida o header contra o tamanho real do arquivo e cada bloco antes de processar, arquivos corrompidos encerram com erro em vez de acessar memoria invalida.

## Build

Requer `g++`, `nasm`, e suporte a SSE2/AVX2.

```bash
make
```

Gera dois binarios: `comp` (compressor) e `decomp` (descompressor).

## Uso

```bash
./comp   entrada.bin saida.lz
./decomp saida.lz    saida_orig.bin
```

## Resultados

Medidos em Linux x86-64, nivel 5, binarios ELF e texto:

| Arquivo | Tipo | Original | Comprimido | Reducao |
|---|---|---|---|---|
| comp (este binario) | ELF 113KB | 113816 B | 49429 B | 56.6% |
| ls | ELF 162KB | 162472 B | 70255 B | 56.8% |
| find | ELF 200KB | 199872 B | 91236 B | 54.4% |
| gcc | ELF 1.4MB | 1459464 B | 473624 B | 67.5% |
| CHANGELOG.md | texto 300KB | 307866 B | 142730 B | 53.6% |
| dados aleatorios | binario 1MB | 1048576 B | 1048585 B | 0% (store) |

## Dependencias

- g++ com suporte a `-march=native`
- nasm >= 2.14
- Linux x86-64
