ENTRY(_start)

SECTIONS {
  . = 2M;
  .text : { *(.text) }
  .rodata : { *(.rodata) }
  .data : { *(.data) }
  .bss : { *(.bss COMMON) }
}
