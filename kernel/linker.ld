ENTRY(_start)

SECTIONS
{
    /* Kernel loaded at 1 MB by GRUB */
    . = 0x100000;

    /* Multiboot2 header MUST be within first 32KB */
    .multiboot2 : { *(.multiboot2) }

    .text :
    {
        /* entry.o (_start) must come first */
        KEEP(*entry.o(.text))
        *(.text .text.*)
    }

    .rodata : { *(.rodata .rodata.*) }

    .data   : { *(.data .data.*) }

    .bss :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.rel*)
        *(.rela*)
    }
}
