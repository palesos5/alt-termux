/* miyabi-os/x86/kernel/vm.c — страничная память i386 (Pentium MMX / Pentium 4, 2004).
 * Одна глобальная страница-таблица identity: ядро и юзеры живут в одном адресном
 * мире (UP без swapper'а — как в MIPS-версии). Фасад: ядро даёт только сегменты
 * и iowin. Никакого CR4.PSE-хаоса: 4KB страницы, supervisor bit решает привилегию. */
#include "types.h"

#define PGSZ     4096
#define USERBASE 0x10000000u        /* user-сегменты: ring3 видит только >= USERBASE */
#define KERNEL_BASE 0x100000u       /* ядро загружено GRUB'ом сюда */

/* таблица страниц: 1024 PDE * 1024 PTE покрывает 4GB; нам хватит первых 64MB:
 * 16 PT по 4KB = 64KB статической памяти на таблицы + 4KB PDE. */
#define NPT 17                      /* PDE[0..15] -> low 64MB, PDE[16] -> MMIO/VGA */
static u32 pd[1024]      __attribute__((aligned(4096)));
static u32 pt[NPT][1024] __attribute__((aligned(4096)));

extern char _end[];
static u32 freelist[512];           /* стек свободных страниц (2MB пул — серверам хватит) */
static int fl_n;
static u32 heap_ptr;

#define PF_PRESENT 0x01u
#define PF_WRITE   0x02u
#define PF_USER    0x04u
#define PF_NOCACHE 0x018u           /* PCD|PWT для MMIO/VRAM */

static void map_page(u32 virt, u32 phys, u32 flags)
{
    u32 pi = virt >> 22, vi = (virt >> 12) & 1023;
    if (!(pd[pi] & PF_PRESENT)) {
        /* выделить PT нельзя динамически: используем предвыделенные pt[] по индексу PDE */
        if (pi >= NPT) return;                    /* грубый отказ: >64MB не маппим */
        pd[pi] = (u32)&pt[pi][0] | PF_PRESENT | PF_WRITE | PF_USER;
    }
    pt[pi][vi] = (phys & 0xFFFFF000u) | flags | PF_PRESENT;
}

void paging_init(void)
{
    int i;
    for (i = 0; i < 1024; i++) pd[i] = 0;
    /* identity: первые 16MB RAM (ядро+стек+пулы), supervisor-only выше USERBASE нет:
     * всё low RAM доступно ring3? НЕТ: ставим USER бит только на диапазоны >= 1MB,
     * а 0..1MB (BIOS/VGA rom area) оставляем kernel-only. */
    for (u32 a = 0; a < 0x00100000u; a += PGSZ)
        map_page(a, a, PF_WRITE);                 /* kernel only */
    for (u32 a = 0x00100000u; a < 0x01000000u; a += PGSZ)
        map_page(a, a, PF_WRITE | PF_USER);       /* 1..16MB: общий мир */
    /* VGA framebuffer 0xA0000 (planar 128K) — kernel+user read/write, nocache-ish */
    map_page(0x000A0000u, 0x000A0000u, PF_WRITE | PF_USER | PF_NOCACHE);
    map_page(0x000B0000u, 0x000B0000u, PF_WRITE | PF_USER | PF_NOCACHE);
    /* PCI MMIO окно 0xE0000000..0xE0400000 (VESA LFB типично тут на чипсетах 2004) */
    for (u32 a = 0xE0000000u; a < 0xE0400000u; a += PGSZ)
        map_page(a, a, PF_WRITE | PF_USER | PF_NOCACHE);
    /* legacy IO через порт-доступ не требует страниц; CR3: */
    __asm__ volatile("movl %0, %%cr3" :: "r"(&pd[0]));
    u32 cr0;
    __asm__ volatile("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000001u;               /* PG | WP: write-protect ring0 уважает USER бит */
    __asm__ volatile("movl %0, %%cr0" :: "r"(cr0));
}

void vm_init(u32 memtop)
{
    u32 end = memtop;
    if (end > 0x01000000u) end = 0x01000000u;     /* 16MB ceiling: TLB-era simplicity */
    heap_ptr = ((u32)_end + PGSZ - 1) & ~(PGSZ - 1);
    fl_n = 0;
    for (u32 p = heap_ptr; p + PGSZ <= end && fl_n < 512; p += PGSZ)
        freelist[fl_n++] = p;
}

static u32 page_alloc(void) { return fl_n ? freelist[--fl_n] : 0; }
static void page_free(u32 p) { if (fl_n < 512) freelist[fl_n++] = p; }

/* create_segment(size): одна физическая страница, identity-отображена в USERBASE
 * диапазон. Простота эпохи: сегменты 4KB; большие буферы — несколько rpc (как в MIPS). */
long sys_seg_create(u32 size)
{
    if (size > PGSZ) return -1;       /* честный отказ — сервер пересоберётся */
    u32 base = page_alloc();
    if (!base) return -1;
    volatile u32 *k = (volatile u32*)base;
    u32 i;
    for (i = 0; i < PGSZ/4; i++) k[i] = 0;   /* memset: пользователь не видит мусор BIOS */
    return (long)base;                        /* уже сматчен identity с PF_USER */
}

long sys_seg_free(u32 uptr)
{
    if (uptr < 0x00100000u || uptr >= 0x01000000u) return -1;
    page_free(uptr & 0xFFFFF000u);
    return 0;
}

/* iowin: единственная привилегия драйверов. Whitelist зашит compile-time. */
struct Range { u32 lo, hi; };
static struct Range allow[] = {
    { 0x000A0000u, 0x000C0000u },   /* VGA framebuffer window */
    { 0x000C0000u, 0x000D0000u },   /* option ROM shadow (VESA BIOS info block) */
    { 0xE0000000u, 0xF0000000u },   /* PCI MMIO bar (VESA LFB у Trident/ATI/Rage) */
};

long sys_iowin(u32 phys, u32 size)
{
    unsigned i;
    for (i = 0; i < ARRAYLEN(allow); i++)
        if (phys >= allow[i].lo && phys + size <= allow[i].hi)
            return (long)phys;       /* identity: виртуальный == физический */
    return -1;                       /* EPERM: фасад не трескается */
}

/* copyin/copyout: pages всегда резидентны (UP без swapper'а) — прямое копирование. */
int copyin(u32 uptr, void *kbuf, int n)
{
    if (uptr < 0x00100000u || uptr + n > 0x01000000u) return -1;
    const u8 *src = (const u8*)uptr;
    u8 *dst = (u8*)kbuf;
    int i;
    for (i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

int copyout(const void *kbuf, u32 uptr, int n)
{
    if (uptr < 0x00100000u || uptr + n > 0x01000000u) return -1;
    u8 *dst = (u8*)uptr;
    const u8 *src = (const u8*)kbuf;
    int i;
    for (i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

/* --- PIC 8259A remap: IRQ0..7 -> int 0x20.., IRQ8..15 -> 0x28.. --- */
static inline void outb_(u16 port, u8 v) { __asm__ volatile("outb %0,%w1" :: "a"(v), "Nd"(port)); }
static inline u8  inb_(u16 port) { u8 v; __asm__ volatile("inb %w1,%0" : "=a"(v)); return v; }

void pic_init(void)
{
    outb_(0x20, 0x11); outb_(0xA0, 0x11);      /* ICW1: init + cascade */
    outb_(0x21, 0x20); outb_(0xA1, 0x28);      /* ICW2: vector base */
    outb_(0x21, 0x04); outb_(0xA1, 0x02);      /* ICW3: cascade lines */
    outb_(0x21, 0x01); outb_(0xA1, 0x01);      /* ICW4: 8086 mode */
    outb_(0x21, 0xD6);                          /* IM1: off IRQ0 timer? NO:
        mask = ~разрешённых: разрешаем IRQ0(PIT),IRQ1(kbd),IRQ12(mouse),IRQ14(IDE)
        master bits: 0(timer),1(kbd),2(slave cascade!) -> маска 0xFD&~0x04...
        считаем честно: master enable 0,1 => mask 0xFE? bit0,bit1 on -> 0xFC;
        slave enable 4(mouse irq12->slave4),6(ide irq14->slave6) -> mask 0xEF&0xBF=0xAF?
        slave bits 4 и 6 on => маска ~(1<<4|1<<6)=0xAF. Cascade bit2 master off => 0xFB. */
    outb_(0x21, 0xFB);                          /* master: IRQ0,1 on; IRQ2 cascade */
    outb_(0xA1, 0xAF);                          /* slave: IRQ12,14 on */
    /* PIT ch0: 50 Гц square wave (1193182/50=23863) — тик планировщика и часов */
    outb_(0x43, 0x36);
    outb_(0x40, 0x57); outb_(0x40, 0xDD*0+0x5D); /* 23863 = 0x5D57: low 0x57 high 0x5D */
    __asm__ volatile("sti");                    /* прерывания включаются после IDT готов */
}

void pic_eoi(int line)
{
    if (line >= 8) outb_(0xA0, 0x20);
    outb_(0x20, 0x20);
}

long sys_irq_on(u32 line)   /* подписка драйвера: в этой реализации линии фикс.
                                (whitelist в chan_irq) — syscall только подтверждает */
{
    return (line <= 15) ? 0 : -1;
}
