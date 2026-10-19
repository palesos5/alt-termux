/* miyabi-os/libc/washi.c — «washi» (和紙) libc: единственная библиотека, статическая.
 * Нет malloc (есть create_segment), нет stdio (пишем в каналы текстом как в rio),
 * нет errno — возвращаем коды Msg.count=-n. ~300 строк на всё. */
#include "washi.h"

/* --- syscall trampoline (MIPS o32: v0=nr, a0..a3 args, instr syscall) ---
 * Конвенция kokoro: аргументы в $a0..$a3, номер в $v0, ответ в $v0 (<0 = ошибка). */
long _sys(long nr, long a0, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile(
        "move $v0, %1\n\t"
        "move $a0, %2\n\t"
        "move $a1, %3\n\t"
        "move $a2, %4\n\t"
        "move $a3, %5\n\t"
        "syscall\n\t"
        "move %0, $v0\n\t"
        : "=r"(ret)
        : "r"(nr), "r"(a0), "r"(a1), "r"(a2), "r"(a3)
        : "$v0", "$a0", "$a1", "$a2", "$a3", "memory");
    return ret;
}
#define SYSCALL(n,a,b,c,d) _sys((n),(long)(a),(long)(b),(long)(c),(long)(d))

int exit(int code)                 { return (int)SYSCALL(SYS_EXIT, code,0,0,0); }
u32 spawn(u32 entry, u32 arg, int nice){ return (u32)SYSCALL(SYS_SPAWN, entry,arg,nice,0); }
void sched_yield(void)             { SYSCALL(SYS_SCHED,0,0,0,0); }
void* create_segment(u32 size)     { return (void*)(long)SYSCALL(SYS_SEG_CREATE,size,0,0,0); }
void free_segment(void *p)         { SYSCALL(SYS_SEG_FREE,p,0,0,0); }
void* iowin(u32 phys, u32 size)    { return (void*)(long)SYSCALL(SYS_IOWIN,phys,size,0,0); }

/* --- каналы --- */
fid chan_open(int dev, const char *path)
{
    return (fid)SYSCALL(SYS_CHAN_OPEN, dev, path, 0, 0);
}

int chan_write(Msg *m)
{
    return (int)SYSCALL(SYS_CHAN_WRITE, m, 0, 0, 0);
}

/* chan_read блокирующий: ядро само sleep'ит (см. kernel/chan.c) */
int chan_read(fid f, Msg *m)
{
    return (int)SYSCALL(SYS_CHAN_READ, f, m, 0, 0);
}

int chan_wait(fid f, u32 evmask)
{
    return (int)SYSCALL(SYS_CHAN_WAIT, f, evmask, 0, 0);
}

void clunk(fid f) { SYSCALL(SYS_CLUNK, f, 0, 0, 0); }

/* отправка запроса серверу + ожидание ответа с тем же tag (9P-стиль) */
Msg *rpc(fid f, Msg *req)
{
    static Msg rspbuf;
    static u8 tagctr;
    req->tag = ++tagctr;
    while (chan_write(req) == -2) sched_yield();   /* EFULL retry: решение клиента */
    Msg in;
    for (;;) {
        if (chan_read(f, &in) < 0) return NULL;
        if (in.type >= Ropen && in.tag == req->tag) { rspbuf = in; return &rspbuf; }
        /* чужое событие: бросить обратно нельзя (q уже съела) — приложения
         * читают события отдельным фидом /dev/win/<id>/events. Тут молча дропаем
         * только если type==Tevent и клиент не подписан. */
    }
}

/* --- мини-string (никакого glibc, compile time 2004 позволяет) --- */
int slen(const char *s){ int n=0; while(s[n])n++; return n; }
char *scpy(char *d,const char *s){ int i=0; do d[i]=s[i]; while(s[i++]); return d; }
int scmp(const char*a,const char*b){ while(*a&&*a==*b){a++;b++;} return (u8)*a-(u8)*b; }

int sfmt(char *b, int n, const char *f, int a, int c)
{
    int o = 0, i = 0;
    while (f[i] && o < n-1) {
        if (f[i]=='%'){
            int v = (f[i+1]=='d')?a:c;
            if (f[i+1]=='d'||f[i+1]=='x'){
                char tmp[12]; int t=0;
                unsigned uv=(unsigned)v;
                if (f[i+1]=='x'){ do tmp[t]="0123456789abcdef"[uv&15],uv>>=4;while(uv); }
                else { do tmp[t++]='0'+uv%10,uv/=10;while(uv); }
                while(t&&o<n-1) b[o++]=tmp[--t];
                i+=2; continue;
            }
        }
        b[o++]=f[i++];
    }
    b[o]=0;
    return o;
}

/* --- GUI helpers поверх kanzashi (политика модалей живёт ЗДЕСЬ, не в сервере) --- */
static fid win_f;              /* главный фид окна */
static char win_id[8];

int win_open(const char *label, int x,int y,int w,int h)
{
    Msg m = {0};
    m.type=Topen; m.dev=DEV_GUI;
    sfmt((char*)m.data,240,"win/%d", (x<<16)|(y<<8)|w);   /* id выдаст сервер в Ropen.fid */
    Msg *r = rpc(win_f ? win_f : (win_f=chan_open(DEV_GUI,"win/new")), &m);
    if(!r) return -1;
    *(u32*)win_id = r->fid;
    Msg c = {0}; c.type=Tctl; c.fid=r->fid;
    c.count=sfmt((char*)c.data,240,"%d %d %d %d 0",x,y,w,h)+1;
    rpc(win_f,&c);                    /* geom */
    c.type=Tctl; c.mode=1;            /* label */
    c.count=slen(label)+1; scpy((char*)c.data,label);
    rpc(win_f,&c);
    return (int)r->fid;
}

void win_damage(int x,int y,int w,int h,u8 *pix)
{
    Msg m={0}; m.type=Twrite; m.fid=*(u32*)win_id; m.off=(x)|(y<<8)|((w&0xFF)<<16)|((h&0xFF)<<24);
    m.count = w*h > 240 ? 240 : w*h;  /* большие damage — сегментами; MVP: ≤240px за раз */
    for(int i=0;i<m.count;i++) m.data[i]=pix[i];
    chan_write(&m);                   /* fire-and-forget: композитинг асинхронный */
}

/* большая модальность: M0 direct / M1 panel / M2 command-line */
void win_setmode(int mid, int mode)
{
    Msg m={0}; m.type=Tctl; m.fid=mid; m.mode=2;
    m.count=sfmt((char*)m.data,240,"modal %d",mode)+1;
    rpc(win_f,&m);
}

/* событие из events-фида: сервер шлёт текст "mouse x y B down|up|drag" */
int win_events(int mid, char *buf, int n)
{
    Msg m={0}; m.type=Tread; m.fid=mid; m.count=n>240?240:n;
    Msg *r=rpc(mid? (fid)mid:0,&m);   /* events читается своим фидом /win/<id>/events */
    if(!r) return -1;
    for(int i=0;i<r->count&&i<n-1;i++) buf[i]=r->data[i];
    buf[r->count]=0;
    return r->count;
}
