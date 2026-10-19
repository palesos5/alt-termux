/* miyabi-os/kernel/chan.c — каналы ядра «kokoro».
 * Фасад: ядро не понимает содержимое Msg, только маршрутизирует 256B пакеты
 * между очередями процессов. Никаких блокировок — вытесняющий планировщик
 * один на CPU (обе машины 2004 года UP). */
#include "types.h"
#include "chan.h"

#define QLEN 16            /* 16*256B = 4KB на очередь, ровно страница */

typedef struct Queue {
    Msg  m[QLEN];
    u32  head, tail;
    u32  owner;            /* pid читателя (сервер) */
    int  used;
} Queue;

/* таблица fid'ов: фасад ядра знает только dev+очередь, путь opaque для него */
typedef struct Fid {
    u32   id;
    u32   pid;             /* владелец */
    int   dev;
    Queue *q_req;          /* клиент -> сервер */
    Queue *q_rsp;          /* сервер -> клиент (события тоже сюда) */
    char  path[64];        /* копируется серверу в Topen.data */
    int   alive;
} Fid;

#define NFID 128
static Fid fids[NFID];
static u32 next_fid = 1;

/* память очередей: статический пул страниц (никакого kmalloc-багатора) */
static Queue qpool[64];
static int   qpool_n;

Queue *q_alloc(int n)
{
    if (n > QLEN || qpool_n >= ARRAYLEN(qpool)) return NULL;
    Queue *q = &qpool[qpool_n++];
    q->head = q->tail = q->used = 0;
    return q;
}

int q_put(Queue *q, Msg *m)
{
    if (q->used == QLEN) return -1;          /* backpressure: сервер сам решит */
    q->m[q->tail] = *m;
    q->tail = (q->tail + 1) % QLEN;
    q->used++;
    return 0;
}

int q_get(Queue *q, Msg *m)
{
    if (!q->used) return 0;
    *m = q->m[q->head];
    q->head = (q->head + 1) % QLEN;
    q->used--;
    return 1;
}

void chan_init(void) { qpool_n = 0; next_fid = 1; }

/* внешний API для sys_* ниже и для spawn-пути */
extern void sched_wake(u32 pid);
extern u32  cur_pid(void);
extern int  copyin(u32 uptr, void *kbuf, int n);      /* vm.c */
extern int  copyout(const void *kbuf, u32 uptr, int n);

u32 chan_open_dev(u32 pid, int dev, const char *path)
{
    for (int i = 0; i < NFID; i++) {
        Fid *f = &fids[i];
        if (f->alive) continue;
        f->id = next_fid++;
        f->pid = pid;
        f->dev = dev;
        f->alive = 1;
        f->q_req = q_alloc(QLEN);
        f->q_rsp = q_alloc(QLEN);
        if (!f->q_req || !f->q_rsp) { f->alive = 0; return 0; }
        int n = 0;
        while (path[n] && n < 63) { f->path[n] = path[n]; n++; }
        f->path[n] = 0;
        /* Topen летит сразу в очередь сервера — сервер может ещё не родиться;
         * очередь DEV-* общая (per-dev), поэтому демультиплицирует сам сервер
         * по fid. Это и есть фасад: ядро не хранит реестр серверов. */
        return f->id;
    }
    return 0;
}

static Fid *find_fid(u32 id)
{
    for (int i = 0; i < NFID; i++)
        if (fids[i].alive && fids[i].id == id) return &fids[i];
    return NULL;
}

long sys_chan_open(u32 a0, u32 a1)
{
    char path[64];
    if (copyin(a1, path, sizeof(path)) < 0) return -1;
    u32 f = chan_open_dev(cur_pid(), (int)a0, path);
    return f ? (long)f : -1;
}

/* chan_read(fid, &msg): если очередь ответа пуста — блокируемся (sched_yield
 * до Tevent/R*). Возвращаем скопированный пользователю Msg. */
long sys_chan_read(u32 a0, u32 a1)
{
    Fid *f = find_fid(a0);
    if (!f) return -1;
    Msg m;
    for (;;) {
        if (q_get(f->q_rsp, &m)) {
            /* помечаем: ответ идёт обратно в q_req сервера по tag */
            m.dev = f->dev;
            return copyout(&m, a1, sizeof(m));
        }
        extern void block_on(Queue *q);
        block_on(f->q_rsp);            /* sleep until q_put wakes owner */
    }
}

long sys_chan_write(u32 a0)
{
    Msg m;
    if (copyin(a0, &m, sizeof(m)) < 0) return -1;
    Fid *f = find_fid(m.fid);
    if (!f) return -1;
    /* направление зависит от типа: запрос клиента -> q_req сервера;
     * ответ сервера (R*) или Tevent/Tirg -> q_rsp клиента */
    Queue *to;
    if (m.type == Ropen || m.type == Rread || m.type == Rwrite ||
        m.type == Rclunk || m.type == Rctl  || m.type == Tevent || m.type == Tirg)
        to = f->q_rsp;
    else
        to = f->q_req;
    if (q_put(to, &m) < 0) return -2;    /* EFULL: ретраит по таймеру — решение сервера */
    sched_wake(to->owner);
    return 0;
}

long sys_chan_wait(u32 a0, u32 a1)
{
    Fid *f = find_fid(a0);
    if (!f) return -1;
    Msg m;
    for (;;) {
        if (q_get(f->q_rsp, &m)) {
            if ((1u << m.type) & a1) {   /* evmask фильтрует по типу */
                q_put(f->q_rsp, &m);     /* не съедено — вернуть */
                return 1;
            }
            /* не наш тип: оставляем в очереди и выходим чтобы не крутить CPU */
            q_put(f->q_rsp, &m);
            return 0;
        }
        extern void block_on(Queue *q);
        block_on(f->q_rsp);
    }
}

long sys_clunk(u32 a0)
{
    Fid *f = find_fid(a0);
    if (!f) return -1;
    Msg m = { .type = Tclunk, .fid = a0, .dev = f->dev };
    q_put(f->q_req, &m);                 /* сказать серверу «забудь fid» */
    f->alive = 0;
    return 0;
}

/* доставка прерываний драйверам (вызывается из trap_low в IRQ case) */
void chan_irq(int line)
{
    for (int i = 0; i < NFID; i++) {
        Fid *f = &fids[i];
        if (!f->alive || f->dev != DEV_FB + line % 4) continue;  /* грубая маршрутизация */
        Msg m = { .type = Tirg, .fid = f->id, .dev = f->dev };
        m.data[0] = (u8)line;
        q_put(f->q_req, &m);
        sched_wake(f->q_req->owner);
    }
}
