/* miyabi-os/x86/kernel/chan.h — IPC-канал = единственный механизм общения.
 * Фиксированный пакет 256B, побайтово совместим с MIPS-версией (x86 little-endian:
 * поля выровнены естественно, размер тот же — проверено sizeof == 256). */
#ifndef _MIYABI_CHAN_H
#define _MIYABI_CHAN_H
#include "types.h"

/* типы сообщений T* (запросы) и R* (ответы), как в урезанном 9P */
enum {
    Topen = 1, Ropen,
    Tread,    Rread,
    Twrite,   Rwrite,
    Tclunk,   Rclunk,
    Tctl,     Rctl,      /* управление: geom/mode/label/ctl строки */
    Tevent,              /* сервер -> клиент, асинхронно (mouse/key/expose/mode) */
    Tirg,                /* ядро -> драйвер: прерывание линии data[0] */
    Tflush,
};

typedef struct Msg {
    u8  type;
    u8  tag;      /* запрос/ответ пары; события имеют tag=0 */
    u16 dev;
    u32 fid;
    u32 off;
    u16 count;    /* длина payload в data */
    u16 mode;
    u8  data[240];
} Msg;            /* == 256 байт */

typedef struct Queue Queue;   /* FIFO из Msg, кольцевой буфер ядра */

/* --- ядерная сторона --- */
Queue *q_alloc(int n);
int    q_put(Queue *q, Msg *m);       /* -1 если переполнен */
int    q_get(Queue *q, Msg *m);       /* 0 если пусто */
u32    chan_open_dev(u32 pid, int dev, const char *path);
void   chan_init(void);

/* --- syscall обработчики (из trap.S) --- */
long sys_chan_open(u32 a0, u32 a1);   /* a0=dev, a1=user path ptr */
long sys_chan_read(u32 a0, u32 a1);   /* a0=fid, a1=&user Msg */
long sys_chan_write(u32 a0);          /* a0=&user Msg */
long sys_chan_wait(u32 a0, u32 a1);   /* a0=fid, a1=evmask */
long sys_clunk(u32 a0);

#endif
