/* miyabi-os/build/link-ip32.lds — линковка для SGI IP32 (ARMON PROM).
 * Все серверы+приложения линкуются в ОДИН образ на фикс. адресах: spawn по указателю,
 * никакого exec-загрузчика (фасад: ядро не умеет читать ELF — зачем?). */
OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
    . = 0x80400000;                 /* ARMON грузит kernel сюда (KSEG0) */
    .text : { *(.text) *(.rodata) }
    .data : { *(.data) }
    .bss  : { *(.bss) *(COMMON) _end = .; }

    /* user space: identity phys, но ниже 16MB (TLB refill map только их) */
    . = 0x80100000;
    init_start  = .; KEEP(*(apps_init))      /* секции заданы __attribute__((section)) */
    uart_start  = .; 
    clock_start = .;
    ide_start   = .;
    fb_start    = .;
    kifu_start  = .;
    gui_start   = .;
    term_start  = .;
    tokei_start = .;
    font_bin    = .;                          /* fs/font8x12.bin линкуется -b binary */

    /DISCARD/ : { *(.comment) *(.MIPS.abiflags) }
}
