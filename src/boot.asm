org 0x7C00
bits 16

_entry:

    mov ah, 0x0E
    mov al, 0x61
    int 0x10

    mov ah, 0x0E
    mov al, 0x62
    int 0x10

    mov ah, 0x0E
    mov al, 0x63
    int 0x10

_read_alvm_header:

    mov ah, 0x42
    mov dl, 0x80
    mov si, _disk_address_packet
    int 0x13

    jc _read_alvm_header_error
    jmp _check_alvm_header

_read_alvm_header_error:

    mov ah, 0x0E
    mov al, 0x64
    int 0x10
    ret

_check_alvm_header:

    xor dx, dx
    mov ds, dx
    mov al, [0x0500]
    mov bl, 'A'
    cmp al, bl
    je _load_skeleton_kernel

    mov ah, 0x0E
    mov al, 0x65
    int 0x10
    ret

_load_skeleton_kernel:

    ; @todo Don't reload the first sector.
    
    mov cx, [0x0502]
    mov [_disk_address_packet + 2], cx
    
    mov ah, 0x42
    mov dl, 0x80
    mov si, _disk_address_packet
    int 0x13

    jc _load_skeleton_kernel_error
    jmp _boot_skeleton_kernel

_load_skeleton_kernel_error:

    mov ah, 0x0E
    mov al, 0x66
    int 0x10
    ret

_boot_skeleton_kernel:

    jmp 0x0504

_disk_address_packet:

    db 0x10
    db 0x00
    dw 0x0001
    dw 0x0500
    dw 0x0000
    db 0x01
    times 7 db 0x00

_mbr_padding:

    times (510 - ($-$$)) db 0x00

_boot_signature:

    dw 0xAA55

_alvm_header:

    db 'A'
    db 'L'

_dummy_skeleton_kernel_size:

    dw 0x0001

_dummy_skeleton_kernel:

    mov ah, 0x0E
    mov al, 0x31
    int 0x10

    mov ah, 0x0E
    mov al, 0x32
    int 0x10

    _l:
        jmp _l

_disk_padding:

    times (10 * 1024 * 1024 - ($-$$)) db 0x00
