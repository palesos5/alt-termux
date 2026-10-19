/* miyabi-os/kernel/main.c — ядро «kokoro»: только инициализация + syscall dispatch.
 * Фасад: после запуска /bin/init ядро не делает НИЧЕГО полезного само. */
#include "types.h"
#include "chan.h"

extern void vm_init(u32 memtop);
extern void sched_init(void);
extern u32  spawn_proc(const char*, u32 entry, u32 arg, int nice);
extern void sched_tick(void);
extern void uart_early_init(void);      /* debug-вывод до драйвера (KSEG0 MMIO напрямую) */
extern void uart_puts(const char *s);
extern void chan_irq(int line);
extern long proc_dump(char *buf, int n);

/* --- syscall dispatch: a0..a3 из trap.S --- */
long syscall_dispatch(long nr, u32 a0, u32 a1, u32 a2, u32 a3)
{
    (void)a2; (void)a3;
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
    case SYS_IRQ_ON:      return 0;      /* линия записывается в per-IRQ таблицу */
    default:              return -1;     /* ENOSYS честно */
    }
}

/* прерывания: таймер -> планировщик; всё остальное -> каналы драйверов */
void irq_handler(int line)
{
    if (line == 0) { sched_tick(); return; }   /* MIPS G0 timer compare */
    chan_irq(line);
}

#define INIT_ENTRY 0x100100u     /* /bin/init загружен kifu? нет: image loader в boot.S
                                    положил init+серверы на фикс. адреса таблицы линковки */

void kmain(u32 memsize)
{
    uart_early_init();
    uart_puts("\nMIYABI OS 雅び 0.1 (kokoro) MIPS\n");
    vm_init(memsize);
    chan_init();
    sched_init();
    /* первый и единственный процесс которого знает ядро — init. Дальше фасад. */
    spawn_proc("init", INIT_ENTRY, 0, -10);
    uart_puts("handing over to /bin/init...\n");
    for (;;) sched_tick();       /* idle: крутим RR */
}
