bits 64
global _start
extern kernel_main

section .text

_start:

    ; By the time we get here, RDI should already be
    ; pointing to the "boot state" structure.

    call kernel_main

_hang:

    hlt
    jmp _hang
