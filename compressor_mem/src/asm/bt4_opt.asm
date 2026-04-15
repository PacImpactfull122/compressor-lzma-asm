; * bt4_opt.asm
; * busca e insercao bt4 em assembly x86-64
;
; * layout de lz_ctx em memoria, offsets em bytes:
; *   head ocupa os primeiros 16mb, prev e chain vem logo depois, bt_left e bt_right fecham a estrutura
;
; * constantes espelhando config.h

%define OFF_HEAD     0
%define OFF_PREV     16777216
%define WIN_MASK     0xffff
%define HASH_MASK    0x3fffff
%define MAX_MATCH    258
%define MIN_MATCH    3

section .text
global bt4_busca_asm
global bt4_ins_asm

; * bt4_busca_asm recebe ctx, dados, tam, pos, nivel
bt4_busca_asm:
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
    mov  r15d, ecx

    xor  ebx, ebx
    xor  r11d, r11d

    ; ! pos mais MIN_MATCH maior que tam significa que nao ha bytes suficientes para match
    lea  eax, [r15d + MIN_MATCH]
    cmp  eax, r14d
    jg   .fim

    ; * hash de 4 bytes via multiplicacao de knuth
    mov  eax, dword [r13 + r15]
    imul eax, eax, 0x9e3779b1
    shr  eax, 10
    and  eax, HASH_MASK

    mov  r10d, dword [r12 + OFF_HEAD + rax*4]
    test r10d, r10d
    jz   .fim

    mov  r9d, r8d
    shl  r9d, 4

    mov  edx, r14d
    sub  edx, r15d
    cmp  edx, MAX_MATCH
    jle  .loop
    mov  edx, MAX_MATCH

.loop:
    test r9d, r9d
    jz   .fim

    cmp  r10d, r15d
    jge  .fim

    mov  eax, r15d
    sub  eax, r10d
    cmp  eax, WIN_MASK + 1
    ja   .fim

    lea  rdi, [r13 + r10]
    lea  rsi, [r13 + r15]
    mov  ecx, edx
    call compara_bytes_asm_local

    cmp  eax, ebx
    jle  .proximo

    mov  ebx, eax
    mov  r11d, r15d
    sub  r11d, r10d

    cmp  ebx, edx
    jge  .fim

.proximo:
    ; ! off_prev indexa por pos mascarado, nao por pos direto
    mov  eax, r10d
    and  eax, WIN_MASK
    mov  r10d, dword [r12 + OFF_PREV + rax*4]
    dec  r9d
    jmp  .loop

.fim:
    ; * empacota dist nos bits altos e len nos bits baixos do retorno
    mov  eax, r11d
    shl  rax, 32
    or   rax, rbx

    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; * comparacao inline, mesma logica do compara_bytes_asm global
compara_bytes_asm_local:
    push rbx
    push rcx
    xor  eax, eax

.bloco8:
    cmp  ecx, 8
    jl   .byte1

    mov  rbx, qword [rdi + rax]
    xor  rbx, qword [rsi + rax]
    jnz  .diff8

    add  eax, 8
    sub  ecx, 8
    jmp  .bloco8

.diff8:
    bsf  rbx, rbx
    shr  rbx, 3
    add  eax, ebx
    jmp  .ret

.byte1:
    test ecx, ecx
    jz   .ret

    movzx ebx, byte [rdi + rax]
    cmp   bl, byte [rsi + rax]
    jne   .ret

    inc  eax
    dec  ecx
    jmp  .byte1

.ret:
    pop  rcx
    pop  rbx
    ret

; * bt4_ins_asm recebe ctx, dados, pos, len, tam
bt4_ins_asm:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13

    mov  r12, rdi
    mov  r13, rsi

    lea  r9d, [edx + ecx]

.loop_ins:
    cmp  edx, r9d
    jge  .fim

    ; ! precisa de pelo menos 4 bytes a partir de pos para hash valido
    lea  eax, [edx + 3]
    cmp  eax, r8d
    jge  .next

    mov  eax, dword [r13 + rdx]
    imul eax, eax, 0x9e3779b1
    shr  eax, 10
    and  eax, HASH_MASK

    mov  ebx, edx
    and  ebx, WIN_MASK
    mov  ecx, dword [r12 + OFF_HEAD + rax*4]
    mov  dword [r12 + OFF_PREV + rbx*4], ecx

    mov  dword [r12 + OFF_HEAD + rax*4], edx

.next:
    inc  edx
    jmp  .loop_ins

.fim:
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret
