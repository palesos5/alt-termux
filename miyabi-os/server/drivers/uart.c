/* miyabi-os/server/drivers/uart.c — консоль-сервер (16550 на обеих машинах).
 * IP32: SI/O UART @ 0x1FFA0100 (8N1 9600, PROM уже инициализировал!)
 * Qube: 16550 @ 0x18000300. Протокол: Twrite=вывод символов, Tread=чтение строк;
 * RX по IRQ -> Tevent "char c" подписчикам. Никакого termios — только байты. */
#include "../../libc/washi.h"

#ifdef TARGET_IP32
#define UART_BASE 0x1FFA0100u
#endif
#ifdef TARGET_QUBE
#define UART_BASE 0x18000300u
#endif

static volatile u8 *reg;       /* THR/RBR/LSR в одном окне, 16550 layout */
#define RBR 0
#define THR 0
#define LCR 3
#define LSR 5

static void uart_init(void)
{
    reg = iowin(UART_BASE, 8);
    reg[LCR] = 0x83;            /* DLAB on, 8n1 */
    reg[1] = 0x0C;              /* baud 9600 divisor @1.8432MHz — значение из CVSB README */
    reg[LCR] = 0x03;            /* DLAB off */
    reg[2] = 0xC7;              /* FIFO on, trigger 14 */
}

int main(void)
{
    uart_init();
    fid srv = chan_open(DEV_UART,"console");
    Msg in;
    for(;;){
        if(chan_read(srv,&in)<0) continue;
        if(in.type==Twrite){
            for(int i=0;i<in.count;i++){
                while(!(reg[LSR]&0x20)) sched_yield();   /* TX ready — спин с yield */
                reg[THR]=in.data[i];
            }
            Msg o={0};o.type=Rwrite;o.tag=in.tag;o.fid=in.fid;chan_write(&o);
        } else if(in.type==Tread){                       /* неблокирующий опрос RX */
            Msg o={0};o.type=Rread;o.tag=in.tag;o.fid=in.fid;
            if(reg[LSR]&1){ o.count=1; o.data[0]=reg[RBR]; }
            chan_write(&o);
        } else if(in.type==Tirg){                        /* RX IRQ от ядра: broadcast char */
            if(reg[LSR]&1){
                Msg e={0};e.type=Tevent;e.count=3;
                e.data[0]='c'; e.data[1]=' '; e.data[2]=reg[RBR];
                /* подписку не ведём — консольный ввод читается через Tread поллингом;
                 * событие шлём всем открытым fid'ам DEV_UART — ядро само разберёт tag=0 */
                e.fid=in.fid; chan_write(&e);
            }
        }
    }
    return 0;
}
