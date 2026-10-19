/* miyabi-os/server/drivers/fb.c — драйвер-сервер framebuffer.
 * Один файл на обе машины: compile-time TARGET выбирает Magnum3B (SGI IP32)
 * или SM712 (Cobalt Qube). Драйвер НЕ знает окон — только "blit x y w h + пиксели". */
#include "../../libc/washi.h"

#ifdef TARGET_IP32
/* SGI IP32 Magnum3B: H_GFX base 0x94000000, FB window 0x96000000, 8bpp indexed.
 * Палитра через CRAY registers; инициализация скопирована из заголовков IRIX 6.5
 * hgi32.h (регистрация по номеру, никаких devicetree!). */
#define HG_BASE   0x94000000u
#define FB_BASE   0x96000000u
#define W 1280
#define H 1024
static volatile u32 *hg;      /* iowin'нутые регистры */
static volatile u8  *fb;
static void fb_init(void)
{
    hg = iowin(HG_BASE, 0x2000);
    fb = iowin(FB_BASE, W*H);
    hg[0x10/4] = 0x1F;        /* turn on CRT, blank off — значения из newport lib 2004 */
    for(int i=0;i<16;i++)     /* тёплая палитра «миёби»: sumi ink -> washi paper */
        hg[0x100/4+i] = (i<2?0x1a1a1a : 0xf5f0e6 - i*0x020202);
}
static void fb_put(int x,int y,u8 v){ if(x<W&&y<H) fb[y*W+x]=v; }
#endif

#ifdef TARGET_QUBE
/* Cobalt Qube/Loonson: Cirrus Logic GD54xx-совместимый SM712 в MMIO 0x1E000000,
 * режим VGA textless, 800x600x4bpp grayscale (дешёвая панель Qube Micro). */
#define SM_BASE 0x1E000000u
#define FB_BASE 0x1F000000u
#define W 800
#define H 600
static volatile u32 *sm;
static volatile u8  *fb;
static void fb_init(void)
{
    sm = iowin(SM_BASE, 0x400);
    fb = iowin(FB_BASE, W*H/2);           /* 4bpp: 2 пикселя в байте */
    sm[0x3C0/4] = 0x67;                    /* sequencer clocking: graphics mode */
    sm[0x3CE/4] = 0x05|0x02;               /* cr: enable display */
}
static void fb_put(int x,int y,u8 v)
{
    if(x>=W||y>=H) return;
    u8 b = fb[(y*W+x)/2];
    if((x&1)) b=(b&0xF0)|(v>>4); else b=(b&0x0F)|(v<<4);
    fb[(y*W+x)/2]=b;
}
#endif

/* общий протокол: Twrite с geom в off (упаковано kanzashi), пиксели data[] */
int main(void)
{
    fb_init();
    fid srv = chan_open(DEV_FB,"screen");
    Msg in;
    int bx=0,by=0,bw=0,bh=0,sent=0;   /* текущий blit-контекст */
    for(;;){
        if(chan_read(srv,&in)<0) continue;
        if(in.type==Twrite){
            if(in.off & 0xFFFF0000u && sent==0){          /* первый пакет: geom */
                bx=in.off&0xFF; by=(in.off>>8)&0xFF;
                bw=(in.off>>16)&0xFF; bh=(in.off>>24)&0xFF;
            }
            for(int i=0;i<in.count;i++,sent++){
                int px=bx+sent%bw, py=by+sent/bw;
                fb_put(px,py,in.data[i]);
            }
            if(sent>=bw*bh) sent=0;                        /* blit завершён */
            Msg o={0};o.type=Rwrite;o.tag=in.tag;o.fid=in.fid;chan_write(&o);
        } else if(in.type==Tirg){                          /* vsync -> ничего: UP-сервер,
                                                              композитинг уже синхронный */
        } else if(in.type==Tclunk){
            Msg o={0};o.type=Rclunk;o.tag=in.tag;chan_write(&o);
        }
    }
    return 0;
}
