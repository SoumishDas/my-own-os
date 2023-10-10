section .init
    ; Code from crtend.o's .init section would be placed here.
    pop ebp
    ret

section .fini
    ; Code from crtend.o's .fini section would be placed here.
    pop ebp
    ret
