/* miyabi-os/x86/kernel/types.h — базовые типы i386 (little-endian, 2004).
 * Тот же ABI каналов, что и в MIPS-версии: номера syscall и Msg не меняются —
 * «бинари 2004 года вечно» работает только при стабильном протоколе. */
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

/* syscall номера (stable ABI, идентичны MIPS-версии) */
#define SYS_EXIT        1
#define SYS_SPAWN       2       /* spawn(entry, arg, nice) -> pid */
#define SYS_SCHED       3
#define SYS_SEG_CREATE  4       /* create_segment(size) -> vaddr */
#define SYS_SEG_FREE    5
#define SYS_IOWIN       6       /* iowin(phys,size) -> vaddr (driver only) */
#define SYS_CHAN_OPEN   7       /* chan_open(dev, path) -> fid */
#define SYS_CHAN_READ   8
#define SYS_CHAN_WRITE  9
#define SYS_CHAN_WAIT   10
#define SYS_CLUNK       11
#define SYS_IRQ_ON      12      /* подписка на IRQ линии PIC */
#define SYS_COPYIN      13
#define SYS_COPYOUT     14

/* каналы серверов (dev-номера, таблица зашита в ядре = фасад) */
#define DEV_NULL   0
#define DEV_KIFU   1    /* файловый сервер */
#define DEV_GUI    2    /* kanzashi */
#define DEV_FB     3    /* fb-драйвер (VESA LFB / VGA planar) */
#define DEV_UART   4    /* console (VGA text + PS/2 keyboard) */
#define DEV_IDE    5    /* PIIX4/ICH compact flash */
#define DEV_CLOCK  6    /* PIT channel 0 */
#define DEV_PROC   7    /* /proc-like для отладки */

#endif
