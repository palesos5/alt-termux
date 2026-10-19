/* miyabi-os/server/drivers/ide.c — IDE-сервер: PIO-чтение блоков 512B.
 * IP32: MIO ASIC (IDE на борту, регистры через 0x1FD80000 window)
 * Qube: CS5530 @ 0x100001F0 (классика CvSB). Только чтение: ОС read-only по идеологии
 * (образ прошивается dd'ом — менять файлы значит пересобирать ROM как в 2004). */
#include "../../libc/washi.h"

#ifdef TARGET_IP32
#define IDE_DATA 0x1FD80000u   /* alt0 data reg через MIO decode */
#endif
#ifdef TARGET_QUBE
#define IDE_DATA 0x100001F0u
#endif

static volatile u16 *data;     /* 16-bit PIO port */
static volatile u8  *ctl;      /* sector count/cyl/drive в том же окне, offset'ы 1..7 */

static void ide_init(void)
{
    data = iowin(IDE_DATA, 16);
    ctl  = (volatile u8*)data + 1;
}

/* LBA28 -> command 0xA0|drive, затем READ SECTORS, ждать BSY off+DRQ, PIO 256 words */
static int ide_read(u32 lba, u8 *buf)
{
    int t;
    ctl[4]=(u8)lba; ctl[5]=(u8)(lba>>8); ctl[6]=(u8)(lba>>16);
    ctl[7]=0xE0|(lba>>24)&0xF;           /* master, LBA bit24-27 */
    ctl[3]=1; ctl[2]=0;                  /* count=1 block */
    ctl[1]=0x20;                          /* READ SECTORS */
    for(t=0;t<100000;t++) if(!(ctl[7]&0x88) && (ctl[7]&0x40)) break;
    if(t>=100000) return -1;              /* EIO честно без ретраев — сервер решит */
    volatile u16 *d=data;
    for(int i=0;i<256;i++){ u16 w=d[0]; buf[i*2]=w>>8; buf[i*2+1]=w&0xFF; } /* big-endian PIO! */
    return 0;
}

int main(void)
{
    ide_init();
    fid srv = chan_open(DEV_IDE,"/");
    Msg in;
    static u8 blk[512];
    for(;;){
        if(chan_read(srv,&in)<0) continue;
        if(in.type==Tread){
            Msg o={0};o.type=Rread;o.tag=in.tag;o.fid=in.fid;
            if(ide_read(in.off,blk)==0){
                o.count=240; for(int i=0;i<240;i++) o.data[i]=blk[i];
                /* остаток 272B клиент добирает вторым Tread с off+240 — MVP упрощение:
                 * kifu читает блоки целиком двумя rpc, см. server/kifu.c */
            }
            chan_write(&o);
        } else if(in.type==Tclunk){
            Msg o={0};o.type=Rclunk;o.tag=in.tag;chan_write(&o);
        }
    }
    return 0;
}
