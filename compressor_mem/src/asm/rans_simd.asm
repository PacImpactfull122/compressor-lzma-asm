; rans_simd.asm
; rANS encode/decode em assembly x86-64
;
; encode: normaliza estado emitindo bytes baixos ate caber, depois codifica
; decode: s = estado mod total, atualiza estado, renormaliza consumindo bytes do stream
;
; RANS_M = 65536

%define RANS_M 65536

section .text
global rans_enc_simd
global rans_dec_simd

; void rans_enc_simd(u32* estado, u8* saida_buf, u32 tam, u32* freq_tab, u32* cum_tab, u32* total_ptr)
; rdi=estado  rsi=saida_buf  rdx=tam  rcx=freq_tab  r8=cum_tab  r9=total_ptr
rans_enc_simd:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  r12, rdi
    mov  r13, rsi
    mov  r14d, edx
    mov  r15, rcx

    xor  r10d, r10d
    xor  r11d, r11d

.loop:
    cmp  r10d, r14d
    jge  .fim

    movzx eax, byte [r13 + r10]
    mov   ebx, dword [r15 + rax*4]
    test  ebx, ebx
    jz    .skip

    mov   ecx, dword [r8 + rax*4]
    mov   edx, dword [r9]

    ; ! total == 0 causaria divisao por zero
    test  edx, edx
    jz    .skip

    mov   eax, dword [r12]

    ; * max_val: limite de normalizacao, calculado a partir de RANS_M, freq e total
    push  rdx
    push  rcx
    push  rbx

    mov   ecx, RANS_M
    shr   ecx, 8
    xor   edx, edx
    imul  ecx, ebx
    mov   eax, ecx
    xor   edx, edx
    div   dword [r9]
    shl   eax, 8

    mov   r13d, eax
    mov   eax, dword [r12]

    pop   rbx
    pop   rcx
    pop   rdx

.norm:
    cmp   eax, r13d
    jl    .enc

    mov   byte [rsi + r11], al
    inc   r11d
    shr   eax, 8
    jmp   .norm

.enc:
    ; * codifica: divide estado por freq, multiplica por total, soma cum e resto
    xor   edx, edx
    div   ebx
    imul  eax, dword [r9]
    add   eax, ecx
    add   eax, edx
    mov   dword [r12], eax

.skip:
    inc   r10d
    jmp   .loop

.fim:
    mov   eax, r11d

    pop   r15
    pop   r14
    pop   r13
    pop   r12
    pop   rbx
    pop   rbp
    ret

; void rans_dec_simd(u32* estado, const u8* stream, u32 tam, u32* freq_tab, u32* cum_tab, u32* total_ptr, u8* saida, u32 n)
; interface simplificada: apenas renormalizacao em loop
; rdi=estado  rsi=stream  rdx=tam
rans_dec_simd:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14

    mov  r12, rdi           ; estado
    mov  r13, rsi           ; stream
    mov  r14d, edx          ; tam

    xor  r10d, r10d         ; pos no stream

.loop:
    cmp  r10d, r14d
    jge  .fim

    mov  eax, dword [r12]

.renorm:
    cmp  eax, RANS_M
    jge  .decodifica

    cmp  r10d, r14d
    jge  .decodifica

    shl  eax, 8
    movzx ebx, byte [r13 + r10]
    or   eax, ebx
    inc  r10d
    jmp  .renorm

.decodifica:
    mov  dword [r12], eax
    inc  r10d
    jmp  .loop

.fim:
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret
