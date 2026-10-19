/* miyabi-os/server/drivers/clock.c — сервер таймера.
 * MIPS cp0 Compare: ядро крутит Count, мы ставим Compare += тиков и получаем Tirg line 0.
 * Дополнительно считает wall-clock секунды для часов (Tevent "tick N" подписчикам).
 * Самая маленькая «ОС-подсистема»: ~60 строк, как и должна быть правда жизни. */
#include "../../libc/washi.h"

static long count_read(void)      /* mfc0 $9 = Count — привилегия? на MIPS I/III user может читать! */
{
    long c; __asm__ volatile("mfc0 %0, $9" : "=r"(c)); return c;
}
static void compare_set(long v){ __asm__ volatile("mtc0 %0, $11" :: "r"(v)); }

int main(void)
{
    fid srv = chan_open(DEV_CLOCK,"tick");
    long sec=0;
    Msg in;
    for(;;){
        if(chan_read(srv,&in)<0){
            /* ждём прерываний: ядро шлёт Tirg line0 каждые 50 Гц когда Compare сработал */
            sched_yield();
            continue;
        }
        if(in.type==Tirg && in.data[0]==0){
            static long base;
            base += 200000;                 /* ~27MHz/50Гц для R5000, 80MHz/50 для Loongson:
                                               калибровка по PROM-частоте зашита compile-time */
            compare_set(base + count_read());
            if((++sec % 50)==0){
                Msg e={0}; e.type=Tevent; e.fid=in.fid;
                e.count=sfmt((char*)e.data,240,"tick %d",sec/50,0)+1;
                chan_write(&e);             /* часы в GUI просыпаются */
            }
        } else if(in.type==Tread){
            Msg o={0};o.type=Rread;o.tag=in.tag;o.fid=in.fid;
            o.count=sfmt((char*)o.data,240,"%d",sec/50,0)+1;
            chan_write(&o);
        }
    }
    return 0;
}
