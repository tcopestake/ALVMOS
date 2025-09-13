org 0x7C00
bits 16

; At boot, there should be a ~29KiB block of free memory
; beginning at 0x0500. This should be enough space for 
; a small stack, the page tables and the BIOS memory map.

STACK_OFFSET equ 0x0500
PAGE_MAPPING_OFFSET equ 0x1000
BIOS_MEMORY_MAP_OFFSET equ 0x5000

; There should be a ~480KiB block of free memory
; beginning at 0x7E00 - so we'll load the skeleton kernel 
; (and associated metadata) there.

KERNEL_LOAD_OFFSET equ 0x7E00

; (This "boot state" structure will be defined below - and
; used to pass info to the C kernel.)

struc BootState
    .KernelEntryOffset resw 1
    .KernelSize resw 1
    .MemoryMapOffset resw 1
    .MemoryMapSize resw 1
    .PageMappingOffset resw 1
endstruc

; In a valid ALVM install, the first 4 bytes of the
; second sector on the disk should be this "kernel header".

struc ALVMKernelHeader
    .Signature resw 1
    .KernelSectorCount resw 1
endstruc

struc DiskAddressPacket
    .Size resb 1
    .Null resb 1
    .SectorCount resw 1
    .DestinationOffset resw 1
    .DestinationSegment resw 1
    .LBAAddress resq 1
endstruc

_entry:

    ; Init some segment & stack stuff.

    xor dx, dx
    mov ds, dx
    mov es, dx
    mov fs, ax
    mov gs, ax
    mov ss, dx

    mov sp, STACK_OFFSET

    ; etc.

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

    ; _disk_address_packet should already be pre-configured with
    ; the appropriate offset, LBA address, etc.

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

    mov bx, [_disk_address_packet + DiskAddressPacket.DestinationOffset]
    mov ax, [bx + ALVMKernelHeader.Signature]

    cmp al, 'A'
    jne _invalid_alvm_header
    
    cmp ah, 'L'
    je _load_skeleton_kernel

_invalid_alvm_header:

    mov ah, 0x0E
    mov al, 0x65
    int 0x10
    ret

_load_skeleton_kernel:

    ; @todo Don't reload the first sector.

    ; bx should still be pointing to the kernel header.
    ; We just need to update the DAP with the sector count from the header.
    
    mov cx, [bx + ALVMKernelHeader.KernelSectorCount]
    mov [_disk_address_packet + DiskAddressPacket.SectorCount], cx

    mov ah, 0x42
    mov dl, 0x80
    mov si, _disk_address_packet
    int 0x13

    jnc _get_bios_memory_map

_load_skeleton_kernel_error:

    mov ah, 0x0E
    mov al, 0x66
    int 0x10
    ret

_get_bios_memory_map:

    ; @todo
    ; mov ax, 0xE820
    ; int 0x15

_setup_page_tables:

    ; (The OS dev wiki was very helpful here.)

    PAGING_PRESENT_BIT equ 0x01
    PAGING_WRITE_BIT equ (0x01 << 1)

    mov di, PAGE_MAPPING_OFFSET

    ; Zero out the whole structure.

    push di
    mov ecx, 0x1000
    xor eax, eax
    cld
    rep stosd
    pop di
    
    ; Build the page map L4.

    lea eax, [es:di + 0x1000]
    or eax, PAGING_PRESENT_BIT | PAGING_WRITE_BIT
    mov [es:di], eax

    ; Build the PDPT.

    lea eax, [es:di + 0x2000]
    or eax, PAGING_PRESENT_BIT | PAGING_WRITE_BIT
    mov [es:di + 0x1000], eax

    ; Build the page directory.

    lea eax, [es:di + 0x3000]
    or eax, PAGING_PRESENT_BIT | PAGING_WRITE_BIT
    mov [es:di + 0x2000], eax

    ; Save the current value in E/DI for later use.

    push di

    ; Map the first 2MiB.
    
    lea di, [di + 0x3000]
    mov eax, PAGING_PRESENT_BIT | PAGING_WRITE_BIT

    _set_next_page_table_entry:

        mov [es:di], eax
        add eax, 0x1000
        add di, 8
        cmp eax, 0x200000
        jb _set_next_page_table_entry

_setup_gdt:

    IRQ_MASK_ALL equ 0xFF
    
    ; Disable interrupts & mask all IRQs.

    cli

    mov al, IRQ_MASK_ALL
    out 0xA1, al
    out 0x21, al

    ; Load temporary IDT & GDT.
    
    lgdt [_temporary_gdt.pointer]

    lidt [_temporary_idt]

_enter_long_mode:
    
    IA32_EFER_MSR equ 0xC0000080
    IA32_EFER_MSR_LME_BIT equ (0x01 << 8)

    CR4_PAE_BIT equ (0x01 << 5)
    CR4_PGE_BIT equ (0x01 << 7)

    CR0_PROTECTED_MODE_BIT equ 0x01
    CR0_PAGING_BIT equ (0x01 << 31)

    ; Reset E/DI.

    pop di
    
    ; Point CR3 to the page tables.

    mov edx, edi
    mov cr3, edx

    ; Ensure the PAE and PGE bits are set in CR4.
    
    mov eax, cr4
    or eax, CR4_PAE_BIT | CR4_PGE_BIT
    mov cr4, eax

    ; Ensure the LME bit is set in the EFER MSR.

    mov ecx, IA32_EFER_MSR
    rdmsr
    or eax, IA32_EFER_MSR_LME_BIT
    wrmsr

    ; Ensure the protected mode and paging bits are set in CR0.

    mov ebx, cr0
    or ebx, CR0_PROTECTED_MODE_BIT | CR0_PAGING_BIT
    mov cr0, ebx

_far_jump:

    jmp 0x08:_enter_skeleton_kernel

_boot_state:

    istruc BootState
        at BootState.KernelEntryOffset, dw 0x0000
        at BootState.KernelSize, dw 0x0000
        at BootState.MemoryMapOffset, dw 0x0000
        at BootState.MemoryMapSize, dw 0x0000
        at BootState.PageMappingOffset, dw 0x0000
    iend

_temporary_idt:

    dw 0x00
    dq 0x00

_temporary_gdt:

    GDT_EXECUTABLE_BIT equ (0x01 << 43)
    GDT_TYPE_BIT equ (0x01 << 44)
    GDT_PRESENT_BIT equ (0x01 << 47)
    GDT_LONG_MODE_FLAG_BIT equ (0x01 << 53)

    ; The null entry.

    dq 0x00

    ; The "code segment" (for 64-bit operation).

    .code:

        dq GDT_EXECUTABLE_BIT | GDT_TYPE_BIT | GDT_PRESENT_BIT | GDT_LONG_MODE_FLAG_BIT

    ; The lgdt instruction expects a pointer to a pointer to the GDT.

    .pointer:

        dw $ - _temporary_gdt - 1
        dq _temporary_gdt

_disk_address_packet:

    istruc DiskAddressPacket
        at DiskAddressPacket.Size, db 0x10
        at DiskAddressPacket.Null, db 0x00
        at DiskAddressPacket.SectorCount, dw 0x0001
        at DiskAddressPacket.DestinationOffset, dw KERNEL_LOAD_OFFSET
        at DiskAddressPacket.DestinationSegment, dw 0x0000
        at DiskAddressPacket.LBAAddress, dq 0x0000000000000001
    iend

bits 64

_enter_skeleton_kernel:

    ; Jump to the C kernel entrypoint in long mode.

    mov rdx, (KERNEL_LOAD_OFFSET + ALVMKernelHeader_size)
    jmp rdx

_mbr_padding:

    times (510 - ($-$$)) db 0x00

_boot_signature:

    dw 0xAA55
