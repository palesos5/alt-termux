/* miyabi-os/x86/kernel/main.c — ядро «kokoro» x86: init + syscall dispatch.
 * Фасад: после запуска /bin/init ядро не делает НИЧЕГО полезного само. */
#include "types.h"
#include "chan.h"

extern void paging_init(void);
extern void idt_init(void);
extern void vm_init(u32 memtop);
extern void sched_init(void);
extern u32  spawn_proc(const char*, u32 entry, u32 arg, int nice);
extern void sched_tick(void);
extern void vga_early_init(void);       /* debug-вывод в VGA text mode до драйвера */
extern void vga_puts(const char *s);
extern void chan_irq(int line);
extern long proc_dump(char *buf, int n);
extern void pic_init(void);             /* 8259A remap + маска: только IRQ0/1/12/14 */

/* --- syscall dispatch: nr,a0..a3 с user стека (см. boot.S t_syscall) --- */
long syscall_dispatch(long nr, u32 a0, u32 a1, u32 a2, u32 a3)
{
    switch (nr) {
    case SYS_EXIT:        extern void proc_exit(int); proc_exit((int)a0); return 0;
    case SYS_SPAWN:       return (long)spawn_proc("user", a0, a1, (s8)a2);
    case SYS_SCHED:       sched_tick(); return 0;
    case SYS_SEG_CREATE:  extern long sys_seg_create(u32);  return sys_seg_create(a0);
    case SYS_SEG_FREE:    extern long sys_seg_free(u32);    return sys_seg_free(a0);
    case SYS_IOWIN:       extern long sys_iowin(u32,u32);   return sys_iowin(a0, a1);
    case SYS_CHAN_OPEN:   return sys_chan_open(a0, a1);
    case SYS_CHAN_READ:   return sys_chan_read(a0, a1);
    case SYS_CHAN_WRITE:  return sys_chan_write(a0);
    case SYS_CHAN_WAIT:   return sys_chan_wait(a0, a1);
    case SYS_CLUNK:       return sys_clunk(a0);
    case SYS_IRQ_ON:      extern long sys_irq_on(u32);      return sys_irq_on(a0);
    default:              return -1;     /* ENOSYS честно */
    }
}

/* прерывания из boot.S irq_common: линия параметром */
void irq_dispatch(int line)
{
    extern void pic_eoi(int line);
    if (line == 0) { sched_tick(); pic_eoi(0); return; }  /* PIT -> планировщик+clock */
    chan_irq(line);                       /* остальное -> каналы драйверов */
    pic_eoi(line);
}

#define INIT_ENTRY 0x100100u     /* /bin/init на фикс. адресе таблицы линковки */

void kmain(u32 memsize)
{
    vga_early_init();
    vga_puts("\nMIYABI OS 雅び 0.1 (kokoro) i386-BIOS\n");
    vm_init(memsize);
    chan_init();
    pic_init();
    sched_init();
    /* первый и единственный процесс которого знает ядро — init. Дальше фасад. */
    spawn_proc("init", INIT_ENTRY, 0, -10);
    vga_puts("handing over to /bin/init...\n");
    for (;;) sched_tick();       /* idle: крутим RR при включённых прерываниях */
}
