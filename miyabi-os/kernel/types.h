/* miyabi-os/kernel/types.h — базовые типы, только MIPS32 big-endian (2004) */
#ifndef _MIYABI_TYPES_H
#define _MIYABI_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;

#define NULL ((void*)0)
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))

/* syscall номера (stable ABI, менять нельзя — бинари 2004 года вечно) */
#define SYS_EXIT        1
#define SYS_SPAWN       2       /* spawn(path, argv_seg) -> pid */
#define SYS_SCHED       3
#define SYS_SEG_CREATE  4       /* create_segment(size) -> vaddr */
#define SYS_SEG_FREE    5
#define SYS_IOWIN       6       /* iowin(phys,size) -> vaddr (driver only) */
#define SYS_CHAN_OPEN   7       /* chan_open(dev, path) -> fid */
#define SYS_CHAN_READ   8       /* chan_read(q, &msg) */
#define SYS_CHAN_WRITE  9       /* chan_write(&msg) */
#define SYS_CHAN_WAIT   10      /* chan_wait(fid, evmask) */
#define SYS_CLUNK       11
#define SYS_IRQ_ON      12      /* подписка на прерывание линии */
#define SYS_COPYIN      13      /* debug/harness */
#define SYS_COPYOUT     14

/* каналы серверов (dev-номера, таблица зашита в ядре = фасад) */
#define DEV_NULL   0
#define DEV_KIFU   1    /* файловый сервер */
#define DEV_GUI    2    /* kanzashi */
#define DEV_FB     3    /* fb-драйвер */
#define DEV_UART   4    /* console */
#define DEV_IDE    5
#define DEV_CLOCK  6
#define DEV_PROC   7    /* /proc-like для отладки */

#endif
