/* miyabi-os/x86/kernel/idt.c — IDT: exception stubs + IRQ gates + syscall 0x40.
 * Векторы PIC remapped: 0x20..0x27 master, 0x28..0x2F slave (см. pic_init). */
#include "types.h"

typedef struct Gate { u16 lo, sel; u8 zero, attr; u16 hi; } Gate;
static Gate idt[256] __attribute__((aligned(8)));
static struct { u16 lim; u32 base; } idtr;

extern void t_syscall(void);
extern void t_irq0(void), t_irq1(void), t_irq5(void), t_irq6(void);
extern void t_irq7(void), t_irq12(void), t_irq14(void);
extern char tss[];              /* boot.S: .space 104 */
extern u32 gdt[];               /* boot.S: таблица дескрипторов */

extern void vga_puts(const char*);
static void die_halt(const char *msg)
{
    vga_puts(msg);
    for (;;) __asm__ volatile("cli;hlt");
}

/* generic exception entry: номер параметром -> exc_c */
void exc_c(int n)
{
    char b[24]; int i = 0;
    const char *s = "exception ";
    while (*s) b[i++] = *s++;
    if (n >= 10) b[i++] = '0' + n/10;
    b[i++] = '0' + n%10; b[i] = 0;
    die_halt(b);
}

#define STUB(n) \
    __asm__(".text\n.align 4\n.globl exc" #n "\nexc" #n ":\n" \
            "pushl $" #n "\ncall exc_c\naddl $4,%esp\niretd\n")

STUB(0) STUB(1) STUB(2) STUB(3) STUB(4) STUB(5) STUB(6) STUB(7)
STUB(8) STUB(9) STUB(10) STUB(11) STUB(12) STUB(13) STUB(14) STUB(15) STUB(16)

/* spurious IRQ: EOI и выход — чтобы stray с PIC не убил систему */
void irq_spurious_entry(void)
{
    extern void pic_eoi(int);
    pic_eoi(7);   /* классика: spurious на slave masquerades как IRQ7 */
}

void idt_init(void)
{
    int i;
    /* все векторы -> spurious, затем точечная замена: никакой stray не уйдёт
       в BIOS-процедуры (классическая причина зависания машин 2004 года) */
    for (i = 0; i < 256; i++) {
        u32 a = (u32)&irq_spurious_entry;
        idt[i].lo = a & 0xFFFF; idt[i].hi = a >> 16;
        idt[i].sel = 0x08; idt[i].zero = 0; idt[i].attr = 0x8E;
    }
    #define SETGATE(vec, fn, dpl) do { \
        u32 a_ = (u32)(fn); idt[vec].lo = a_ & 0xFFFF; idt[vec].hi = a_ >> 16; \
        idt[vec].sel = 0x08; idt[vec].zero = 0; idt[vec].attr = 0x8E | ((dpl) << 5); \
    } while (0)
    SETGATE(0, exc0, 0);  SETGATE(1, exc1, 0);  SETGATE(2, exc2, 0);  SETGATE(3, exc3, 3);
    SETGATE(4, exc4, 0);  SETGATE(5, exc5, 0);  SETGATE(6, exc6, 0);  SETGATE(7, exc7, 0);
    SETGATE(8, exc8, 0);  SETGATE(9, exc9, 0);  SETGATE(10, exc10, 0); SETGATE(11, exc11, 0);
    SETGATE(12, exc12, 0); SETGATE(13, exc13, 0); SETGATE(14, exc14, 0); SETGATE(15, exc15, 0);
    SETGATE(16, exc16, 0);
    SETGATE(0x40, t_syscall, 3);            /* syscall шлюз: ring3 может int 0x40 */
    SETGATE(0x20, t_irq0, 0);   SETGATE(0x21, t_irq1, 0);
    SETGATE(0x25, t_irq5, 0);   SETGATE(0x26, t_irq6, 0);
    SETGATE(0x27, t_irq7, 0);
    SETGATE(0x2C, t_irq12, 0);  SETGATE(0x2E, t_irq14, 0);

    /* TSS descriptor в GDT slot 0x28 + ltr: esp0 выставляет sched при входах из ring3 */
    {
        u32 base = (u32)tss, lim = 103;
        u32 *desc = &gdt[0x28 / 4];          /* 8-байтный дескриптор = 2 слова */
        desc[0] = (lim << 16) | (base & 0xFFFF);
        /* word1: base23:16(bits16-23) | type 0x9<<8 | P<<15 | DPL0 | limit19:16<<16 | base31:24<<24 */
        desc[1] = ((base >> 16) & 0xFFu) << 16
                | 0x00000900u                 /* present|type busy-386TSS */
                | 0x00008000u                 /* P bit */
                | ((lim >> 16) & 0xFu) << 16
                | (base & 0xFF000000u);
        u16 sel = 0x28;
        __asm__ volatile("ltr %0" :: "r"(sel));
    }

    idtr.lim = sizeof(idt) - 1;
    idtr.base = (u32)idt;
    __asm__ volatile("lidt %0" :: "m"(idtr));
}
