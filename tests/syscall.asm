section .rodata
msg:    db "Hello world PIE", 10 ; "Hello world\n"
msg_len equ $ - msg

section .text
global _start

_start:
    mov rax, 1          ; syscall write
    mov rdi, 1          ; fd = stdout
    mov rsi, msg		; lea rsi, [rel msg]  ; adresse de la string
    mov rdx, msg_len    ; longueur
    syscall

    mov rax, 60         ; syscall exit
    xor rdi, rdi        ; status = 0
    syscall
