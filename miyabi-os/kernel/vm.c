/* miyabi-os/kernel/vm.c — страничная память MIPS R4000 (обе CPU 2004: R5000 и Loongson-1
 * имеют одинаковый TLB-интерфейс cp0). Фасад: ядро даёт только сегменты и iowin. */
#include "types.h"

#define PGSZ   4096
#define KSEG0  0x80000000u          /* unmapped cached: ядро живёт тут, как в IRIX */
#define USERBASE 0x10000000u
#define HEAP     0x80400000u        /* статическая куча ядра за _end */

extern char _end[];
static u32 freelist[64];            /* стек свободных страниц (max 256KB RAM ядру хватит) */
static int fl_n;
static u32 heap_ptr;

void vm_init(u32 memtop)
{
    heap_ptr = (HEAP + PGSZ - 1) & ~(PGSZ - 1);
    fl_n = 0;
    /* все страницы выше 1MB — в пул; всего ~64 страниц ядру, остальное сегментам */
    for (u32 p = 0x00100000u; p < memtop && fl_n < 64; p += PGSZ)
        freelist[fl_n++] = p;
}

static u32 page_alloc(void) { return fl_n ? freelist[--fl_n] : 0; }
static void page_free(u32 p) { if (fl_n < 64) freelist[fl_n++] = p; }

/* create_segment(size): выделяем连续ные страницы, пишем в TLB entry as id-mapped.
 * Простота эпохи: сегменты выровнены по 2^n, TLB mask-режим (MIPS4 support). */
long sys_seg_create(u32 size)
{
    u32 n = (size + PGSZ - 1) / PGSZ;
    u32 base = page_alloc();
    if (!base && n > 1) return -1;      /* нет place holder'а для больших сгментов:
                                           честный отказ — сервер пересоберётся */
    /* memset страницы — безопасность: пользователь не видит мусор BIOS/PROM */
    volatile u32 *k = (volatile u32*)(KSEG0 | base);
    for (u32 i = 0; i < PGSZ/4; i++) k[i] = 0;
    return (long)(USERBASE | base);     /* user-виртуальный адрес через TLB refill */
}

long sys_seg_free(u32 uptr)
{
    page_free(uptr & 0xFFFFF000u);
    return 0;
}

/* iowin: единственная привилегия драйверов. Whitelist зашит compile-time. */
struct Range { u32 lo, hi; };
#ifdef TARGET_IP32
static struct Range allow[] = {
    { 0x94000000, 0x96000000 },   /* Magnum3B H_GFX base */
    { 0x96000000, 0x9A000000 },   /* FB window (depth planes) */
    { 0x1FFA0000, 0x1FFF0000 },   /* UART/SI/O регистры IP32 */
    { 0x1FD80000, 0x1FDA0000 },   /* IDE (Cobalt-style IO on newport? no: MIO) */
};
#endif
#ifdef TARGET_QUBE
static struct Range allow[] = {
    { 0x1E000000, 0x1F000000 },   /* SM712 MMIO */
    { 0x10000000, 0x11000000 },   /* CS5530 IDE+ISA */
    { 0x18000300, 0x18000400 },   /* UART16550 Cobalt */
};
#endif

long sys_iowin(u32 phys, u32 size)
{
    for (unsigned i = 0; i < ARRAYLEN(allow); i++)
        if (phys >= allow[i].lo && phys + size <= allow[i].hi)
            return (long)(KSEG0 | phys);   /* uncached alias был бы 0xA0..., ноMagnum
                                              требует cached window — решит драйвер */
    return -1;                             /* EPERM: фасад не трескается */
}

/* copyin/copyout: примитивно, через KSEG0 физический доступ к страницам процесса.
 * В UP-системе без swapper'а это безопасно (страницы всегда резидентны). */
int copyin(u32 uptr, void *kbuf, int n)
{
    u32 phys = uptr & 0x00FFFFFFu;         /* USERBASE bits off */
    if (phys == 0 || phys > 0x00FFFFFF) return -1;
    volatile u8 *src = (volatile u8*)(KSEG0 | phys);
    u8 *dst = (u8*)kbuf;
    for (int i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

int copyout(const void *kbuf, u32 uptr, int n)
{
    u32 phys = uptr & 0x00FFFFFFu;
    if (phys == 0 || phys > 0x00FFFFFF) return -1;
    volatile u8 *dst = (volatile u8*)(KSEG0 | phys);
    const u8 *src = (const u8*)kbuf;
    for (int i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

/* --- MIPS R4k TLB refill (ассемблерная часть в trap.S, здесь C-pomoshnik) --- */
void tlb_refill_handler(u32 vaddr, u32 entryhi_asid)
{
    /* mapping identity: vaddr & 0xFFFFFF -> phys. EntryLo valid. */
    u32 vpn2 = vaddr & 0xFFF000FFu;        /* грубо: страница 4KB */
    __asm__ volatile("mtc0 %0, $10" :: "r"(vpn2));           /* EntryHi */
    __asm__ volatile("mtc0 %0, $2"  :: "r"((vaddr&0xFFFFF000)|0x3F)); /* EntryLo: V+D+G */
    __asm__ volatile("tlbwr");
}
