OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x000000, LENGTH = 1M
    RAM (rwx) : ORIGIN = 0x100000, LENGTH = 0xF000  /* Leave top 4KB for BIOS */
}

SECTIONS
{
    .text :
    {
        /* crt0.o MUST be first - contains vector table at address 0 */
        */crt0.o(.text)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } > ROM

    .data :
    {
        __data_start = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > ROM

    __data_load = LOADADDR(.data);

    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
}
