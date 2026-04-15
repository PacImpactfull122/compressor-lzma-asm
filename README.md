# compressor-lzma-asm

Compressor de dados com pipeline multi-estágio em C++ e assembly x86-64.

## Estrutura

```
compressor_mem/
├── comp.cpp              # entry point do compressor
├── decomp.cpp            # entry point do descompressor
├── Makefile
├── include/              # headers de todos os módulos
└── src/
    ├── core/             # implementações C++
    ├── asm/              # rotinas críticas em assembly (SSE2/AVX2)
    └── tools/            # utilitários de debug
```

## Build

```bash
cd compressor_mem
make
```

Gera `bin/comp` e `bin/decomp`. Requer `g++` com `-march=native` e `nasm >= 2.14`.

## Uso

```bash
./bin/comp   entrada.bin saida.lz [nivel]   # nivel 1-9, padrão 5
./bin/decomp saida.lz    saida_orig.bin
```

Veja `compressor_mem/README.md` para detalhes do pipeline e algoritmos.
