ENTRY(_start)

SECTIONS {
  .text.start (0x7E04) : {
    .out/kernel-entry.o( .text )
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
  }

  .bss : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }
}
