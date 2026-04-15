
section .text
global calc_hash_asm
global compara_bytes_asm

calc_hash_asm:
    push rbx
    mov  rax, rdi
    movzx ebx, byte [rsi + rax]
    shl  ebx, 16
    movzx ecx, byte [rsi + rax + 1]
    shl  ecx, 8
    or   ebx, ecx
    movzx ecx, byte [rsi + rax + 2]
    or   ebx, ecx
    ; * multiplicacao de Knuth: distribui bits uniformemente para hash de 3 bytes
    imul ebx, ebx, 2654435761
    shr  ebx, 16
    mov  eax, ebx
    pop  rbx
    ret

; * compara em blocos de 8 bytes via qword, bsf localiza o byte divergente
compara_bytes_asm:
    push rbx
    push rcx

    xor  eax, eax
    mov  ecx, edx

.bloco8:
    cmp  ecx, 8
    jl   .byte1

    mov  rbx, qword [rdi + rax]
    cmp  rbx, qword [rsi + rax]
    jne  .diff8

    add  eax, 8
    sub  ecx, 8
    jmp  .bloco8

.byte1:
    test ecx, ecx
    jz   .fim

    mov  bl, byte [rdi + rax]
    cmp  bl, byte [rsi + rax]
    jne  .fim

    inc  eax
    dec  ecx
    jmp  .byte1

.diff8:
    ; ! xor expoe o primeiro bit diferente, bsf localiza esse bit, shr converte para offset de byte
    mov  rbx, qword [rdi + rax]
    xor  rbx, qword [rsi + rax]
    bsf  rbx, rbx
    shr  rbx, 3
    add  eax, ebx

.fim:
    pop  rcx
    pop  rbx
    ret
