section .init
global _init
section .text
_init:
    push ebp
    mov ebp, esp
    ; Code from crtbegin.o's .init section would be placed here.

section .fini
global _fini
section .text
_fini:
    push ebp
    mov ebp, esp
    ; Code from crtbegin.o's .fini section would be placed here.
