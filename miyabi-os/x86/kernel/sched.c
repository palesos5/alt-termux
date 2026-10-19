/* miyabi-os/x86/kernel/sched.c — планировщик «kokoro» x86: round-robin + NICE как в BeOS.
 * UP только (рабочие станции 2004 однопроцессорные). 32 процесса максимум.
 *
 * Переключения через switch_context(old_ptr, new_esp) с ret-механикой (boot.S):
 *   - блокнувшийся процесс сохраняет esp на вершине кадра {edi,esi,ebx,ebp};
 *     над ним лежит ret-адрес вызова switch_context внутри sched_tick, поэтому
 *     resume продолжается после call -> return в block_on -> chan_read -> iretd;
 *   - первый вход: spawn_proc кладёт на дно kstack 4 junk-слоя и &proc_entry;
 *     pop'ы съедают junk, RET уходит в trampoline, тот делает iretd в ring3. */
#include "types.h"
#include "chan.h"

#define MAXP 32

typedef struct Proc {
    u32 pid;
    u32 kstack[192];          /* 768B стека ядра на процесс, регенерации нет */
    u32 esp;                  /* сохраняемый sp (вершина кадра switch'а) */
    u32 entry;                /* user eip для первого входа */
    int started;              /* 0 => вход через proc_entry */
    u32 uesp;                 /* user stack pointer (для iret в ring3) */
    int state;                /* 0 free,1 ready,2 running,3 blocked */
    int nice;                 /* -20..20, BeOS-style: влияет на квант */
    Queue *blocked_on;        /* на какую очередь ждём (chan_read) */
    char name[16];
} Proc;

static Proc p[MAXP];
static int cur = 0;
static int quantum_left;
extern char tss[];            /* esp0 по смещению 4 (TSS layout i386) */

u32 cur_pid(void) { return p[cur].pid; }

static void strncpy_(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i < n-1; i++) d[i] = s[i];
    d[i] = 0;
}

void sched_init(void)
{
    int i;
    for (i = 0; i < MAXP; i++) { p[i].pid = 0; p[i].state = 0; }
    p[0].pid = 1; p[0].state = 2; p[0].nice = 0;
    strncpy_(p[0].name, "idle", 16);
}

/* первый вход процесса: RET с нового стека приводит сюда; собираем iretd-фрейм
 * ring3 на user стеке и уходим в пользовательский код навсегда. */
void proc_entry(void)
{
    int i;
    u32 sp;
    __asm__ volatile("movl %%esp, %0" : "=r"(sp));
    for (i = 1; i < MAXP; i++)                    /* найти себя по стеку ядра */
        if (!p[i].started && sp >= (u32)p[i].kstack && sp < (u32)&p[i].kstack[192])
            break;
    if (i == MAXP) return;
    p[i].started = 1;
    /* фрейм iretd снизу вверх: eip, cs, eflags, uesp, ss; аргумент spawn уже
     * лежит на 4 байта ниже (записан в spawn_proc) */
    {
        u32 *us = (u32*)(p[i].uesp & ~3u);
        *(--us) = p[i].entry;                     /* eip */
        *(--us) = 0x1B;                           /* cs = code ring3 */
        *(--us) = 0x202;                          /* eflags: IF only, IOPL=0 честно */
        *(--us) = (u32)(us - 1);                  /* user esp после входа (указывает на arg) */
        *(--us) = 0x23;                           /* ss = data ring3 */
        ((u32*)tss)[1] = (u32)p[i].kstack + sizeof(p[i].kstack);  /* esp0 этого процесса */
        __asm__ volatile(
            "movl %0, %%esp\n\t"
            "movw $0x23, %%ax\n\t"
            "movw %%ax, %%ds\n\t"
            "movw %%ax, %%es\n\t"
            "movw %%ax, %%gs\n\t"
            "iretd"
            :: "r"((u32)us) : "memory");
    }
}

u32 spawn_proc(const char *name, u32 entry, u32 arg, int nice)
{
    int i;
    for (i = 1; i < MAXP; i++) {
        if (p[i].state) continue;
        p[i].pid = i + 1;
        p[i].entry = entry;
        p[i].started = 0;
        p[i].uesp = 0x00F00000u - i * 0x1000u;   /* user стеки: фикс. слоты ниже 16MB */
        *(u32*)p[i].uesp = arg;                   /* аргумент под future iret frame */
        /* кадр "как после switch_context": pop'ы читают [esp],[+4],[+8],[+12],
         * затем ret читает [esp+16]: junk-регистры + адрес возврата proc_entry */
        p[i].kstack[184] = 0;                    /* edi slot junk */
        p[i].kstack[185] = 0;                    /* esi junk */
        p[i].kstack[186] = 0;                    /* ebx junk */
        p[i].kstack[187] = 0;                    /* ebp junk */
        p[i].kstack[188] = (u32)&proc_entry;     /* ret адрес */
        p[i].esp = (u32)&p[i].kstack[184];       /* esp указывает на первый pop-слой */
        p[i].state = 1;
        p[i].nice = nice;
        p[i].blocked_on = NULL;
        strncpy_(p[i].name, name, 16);
        return p[i].pid;
    }
    return 0;
}

/* квант зависит от nice: BeOS high priority получает больше тиков */
static int quantum_of(int nice) { return 4 - nice / 8; }   /* nice -20 => ~6, +20 => 1 */

static Proc *pick_next(void)
{
    int k;
    for (k = 1; k <= MAXP; k++) {                 /* простой RR по кольцу; blocked skip */
        int i = (cur + k) % MAXP;
        if (p[i].state == 1) return &p[i];
    }
    return &p[cur];                               /* все ждут — idle loop */
}

extern void switch_context(u32 *old_esp, u32 new_esp);

/* вызывается из IRQ0 (PIT) через irq_dispatch и из sched_yield/idle */
void sched_tick(void)
{
    Proc *n;
    int old, i;
    if (--quantum_left > 0) return;
    n = pick_next();
    if (n == &p[cur]) { quantum_left = quantum_of(n->nice); return; }
    if (p[cur].state == 2 && !p[cur].blocked_on) p[cur].state = 1;  /* preempt -> ready */
    old = cur;
    for (i = 0; i < MAXP; i++) if (n == &p[i]) { cur = i; break; }
    n->state = 2;
    quantum_left = quantum_of(n->nice);
    ((u32*)tss)[1] = (u32)p[cur].kstack + sizeof(p[cur].kstack);  /* esp0 нового */
    switch_context(&p[old].esp, p[cur].esp);
    /* сюда возвращаемся ТОЛЬКО когда этот процесс снова станет текущим */
}

void sched_yield(void)
{
    quantum_left = 0;
    sched_tick();
}

void sched_wake(u32 pid)
{
    int i;
    for (i = 0; i < MAXP; i++)
        if (p[i].pid == pid && p[i].state == 3) {
            p[i].state = 1;
            p[i].blocked_on = NULL;
        }
}

void block_on(Queue *q)
{
    p[cur].blocked_on = q;
    p[cur].state = 3;
    quantum_left = 0;
    sched_tick();   /* RET-механика вернёт управление сюда после пробуждения */
}

void proc_exit(int code)
{
    extern void vga_puts(const char*);
    (void)code;
    vga_puts("exit "); vga_puts(p[cur].name);
    p[cur].state = 0;
    p[cur].blocked_on = NULL;
    quantum_left = 0;
    sched_tick();
    for (;;) {}
}

/* /proc-like для DEV_PROC: список процессов текстом (как acme's /proc) */
long proc_dump(char *buf, int n)
{
    int o = 0, i;
    for (i = 0; i < MAXP && o < n - 24; i++) {
        int pid; char st; int nice; const char *nm;
        if (!p[i].state) continue;
        pid = p[i].pid; st = ".RBI"[p[i].state]; nice = p[i].nice; nm = p[i].name;
        buf[o++] = '0' + (pid/10)%10; buf[o++] = '0' + pid%10;
        buf[o++] = ' '; buf[o++] = st; buf[o++] = ' ';
        if (nice < 0) { buf[o++]='-'; nice=-nice; } else buf[o++]='+';
        buf[o++] = '0' + (nice/10)%10; buf[o++] = '0' + nice%10;
        buf[o++] = ' ';
        while (*nm) buf[o++] = *nm++;
        buf[o++] = '\n';
    }
    return o;
}
