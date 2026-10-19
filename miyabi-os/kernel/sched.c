/* miyabi-os/kernel/sched.c — планировщик «kokoro»: round-robin + NICE как в BeOS.
 * UP только (2004-ные MIPS станции однопроцессорные). 32 процесса максимум —
 * столько было в оригинальном BeOS demo-ядре, и нам хватит с серверами. */
#include "types.h"
#include "chan.h"

#define MAXP 32

typedef struct Proc {
    u32 pid;
    u32 kstack[128];          /* 512B стека на процесс, регенерации нет */
    u32 sp, pc;               /* сохраняемые при switch */
    int state;                /* 0 free,1 ready,2 running,3 blocked */
    int nice;                 /* -20..20, BeOS-style: влияет на квант */
    Queue *blocked_on;        /* на какую очередь ждём (chan_read) */
    char name[16];
} Proc;

static Proc p[MAXP];
static int cur = 0;
static int quantum_left;

u32 cur_pid(void) { return p[cur].pid; }

static void strncpy_(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i < n-1; i++) d[i] = s[i];
    d[i] = 0;
}

void sched_init(void)
{
    for (int i = 0; i < MAXP; i++) { p[i].pid = 0; p[i].state = 0; }
    p[0].pid = 1; p[0].state = 2; p[0].nice = 0;
    strncpy_(p[0].name, "idle", 16);
}

u32 spawn_proc(const char *name, u32 entry, u32 arg, int nice)
{
    for (int i = 1; i < MAXP; i++) {
        if (p[i].state) continue;
        p[i].pid = i + 1;
        p[i].pc = entry;
        p[i].sp = (u32)&p[i].kstack[127];
        *(u32*)p[i].sp = arg;
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
    /* простой RR по кольцу; blocked skip */
    for (int k = 1; k <= MAXP; k++) {
        int i = (cur + k) % MAXP;
        if (p[i].state == 1) return &p[i];
    }
    return &p[cur];           /* все ждут — idle loop */
}

/* вызывается из trap_low по таймеру clock-сервера (Tirq line 0 -> сюда же) */
void sched_tick(void)
{
    if (--quantum_left > 0) return;
    Proc *n = pick_next();
    if (n == &p[cur]) { quantum_left = quantum_of(n->nice); return; }
    /* контекст: сохранение regs делает asm switch_context (trap.S) */
    extern void switch_context(u32 *old_sp, u32 new_sp, u32 new_pc);
    p[cur].state = (p[cur].state == 2 && p[cur].blocked_on) ? 3 : 1;
    if (p[cur].state != 3) p[cur].state = 1;
    int old = cur;
    for (int i = 0; i < MAXP; i++) if (n == &p[i]) { cur = i; break; }
    n->state = 2;
    quantum_left = quantum_of(n->nice);
    switch_context(&p[old].sp, p[cur].sp, p[cur].pc);
}

void sched_yield(void)
{
    quantum_left = 0;
    sched_tick();
}

void sched_wake(u32 pid)
{
    for (int i = 0; i < MAXP; i++)
        if (p[i].pid == pid && p[i].state == 3) {
            p[i].state = 1;
            p[i].blocked_on = NULL;
        }
}

void block_on(Queue *q)
{
    p[cur].blocked_on = q;
    p[cur].state = 3;
    sched_tick();                     /* уйти на другой процесс */
}

void proc_exit(int code)
{
    extern void uart_puts(const char*);
    uart_puts("exit "); uart_puts(p[cur].name);
    (void)code;
    p[cur].state = 0;
    p[cur].blocked_on = NULL;
    sched_tick();
    for (;;) {}
}

/* /proc-like для DEV_PROC: список процессов текстом (как acme's /proc) */
long proc_dump(char *buf, int n)
{
    int o = 0;
    for (int i = 0; i < MAXP && o < n - 24; i++) {
        if (!p[i].state) continue;
        o += snprintf_(buf + o, n - o, "%d %c %d %s\n",
                      p[i].pid, ".RBI"[p[i].state], p[i].nice, p[i].name);
    }
    return o;
}

static int snprintf_(char *b, int n, const char *f, ...) { (void)b;(void)n;(void)f; return 0; }
