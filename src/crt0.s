global _start
extern main
; global main

section .text
_start:
	call main
    mov ebx, eax
	mov eax, 19
	int 0x30
    jmp  $