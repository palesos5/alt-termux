/* miyabi-os/build/link-qube.lds — YAMON грузит ELF по 0x80400000 так же;
 * RAM Qube всего 64MB и кэш write-back — тот же layout, меньше буферы. */
OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
    . = 0x80400000;
    .text : { *(.text) *(.rodata) }
    .data : { *(.data) }
    .bss  : { *(.bss) *(COMMON) _end = .; }

    . = 0x80100000;
    init_start  = .;
    uart_start  = .;
    clock_start = .;
    ide_start   = .;
    fb_start    = .;
    kifu_start  = .;
    gui_start   = .;
    term_start  = .;
    tokei_start = .;
    font_bin    = .;

    /DISCARD/ : { *(.comment) *(.MIPS.abiflags) }
}
