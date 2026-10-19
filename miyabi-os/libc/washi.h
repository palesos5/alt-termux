/* miyabi-os/libc/washi.h — публичный API единственной libc.
 * Включается одним заголовком во все приложения — как в добрые времена. */
#ifndef _WASHI_H
#define _WASHI_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed int     s32;
typedef int            fid;

#define SYS_EXIT 1
#define SYS_SPAWN 2
#define SYS_SCHED 3
#define SYS_SEG_CREATE 4
#define SYS_SEG_FREE 5
#define SYS_IOWIN 6
#define SYS_CHAN_OPEN 7
#define SYS_CHAN_READ 8
#define SYS_CHAN_WRITE 9
#define SYS_CHAN_WAIT 10
#define SYS_CLUNK 11
#define SYS_IRQ_ON 12

#define DEV_KIFU 1
#define DEV_GUI  2
#define DEV_FB   3
#define DEV_UART 4
#define DEV_IDE  5
#define DEV_CLOCK 6
#define DEV_PROC 7

enum { Topen=1,Ropen,Tread,Rread,Twrite,Rwrite,Tclunk,Rclunk,Tctl,Rctl,Tevent,Tirg,Tflush };

typedef struct Msg {
    u8  type; u8 tag; u16 dev; u32 fid; u32 off; u16 count; u16 mode; u8 data[240];
} Msg;

#define NULL ((void*)0)

/* ядро */
int   _exit(int code);
#define exit(c) _exit(c)
u32   spawn(u32 entry, u32 arg, int nice);
void  sched_yield(void);
void* create_segment(u32 size);
void  free_segment(void *p);
void* iowin(u32 phys, u32 size);

/* каналы */
fid   chan_open(int dev, const char *path);
int   chan_write(Msg *m);
int   chan_read(fid f, Msg *m);
int   chan_wait(fid f, u32 evmask);
void  clunk(fid f);
Msg*  rpc(fid f, Msg *req);

/* string-минимум */
int   slen(const char *s);
char* scpy(char *d, const char *s);
int   scmp(const char *a, const char *b);
int   sfmt(char *b, int n, const char *f, int a, int c);

/* GUI (политика модалей — здесь!) */
int   win_open(const char *label, int x,int y,int w,int h);
void  win_damage(int x,int y,int w,int h,u8 *pix);
void  win_setmode(int mid, int mode);      /* 0 direct / 1 panel / 2 command */
int   win_events(int mid, char *buf, int n);

#endif
