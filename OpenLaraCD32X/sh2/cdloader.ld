OUTPUT_ARCH(sh)
ENTRY(pri_start)

/*
 * Correct CD-mode model, confirmed against a real disassembly of the
 * Master/Slave SH-2 boot ROMs:
 *   - .header: the 56-byte CD-specific boot table (module name, version,
 *     Destination, Length, Master/Slave entry+VBR - all absolute
 *     addresses). This is what the boot ROM actually reads, from a
 *     fixed frame-buffer offset - NOT any cartridge-style Mars header.
 *   - .text/.data: the real program, linked at VMA 0x06000000 (SDRAM,
 *     matching the table's Destination field) with LMA placed
 *     immediately after the header (0x38 bytes in), matching where the
 *     boot ROM's own copy loop expects program data to begin.
 */

MEMORY {
    hdr   (rx) : ORIGIN = 0x00000000, LENGTH = 0x00000038
    sdram (wx) : ORIGIN = 0x06000000, LENGTH = 0x00040000
}

SECTIONS {
    .header 0x00000000 : {
        __header_start = .;
        *(.header)
        __header_end = .;
    } > hdr

    __header_size = SIZEOF(.header);

    .text 0x06000000 : AT ( __header_size ) {
        __text_start = .;
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        __text_end = .;
    } > sdram

    .data : AT ( __header_size + SIZEOF(.text) ) {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        . = ALIGN(4);
        __data_end = .;
    } > sdram

    __code_size = SIZEOF(.text) + SIZEOF(.data);

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > sdram
}
