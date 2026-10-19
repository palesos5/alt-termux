/* miyabi-os/apps/init.c — /bin/init: первый user-процесс. Рождает ВСЮ систему
 * спавном серверов по фикс. адресам линковки. Фасад: ядро не знает ни одного имени;
 * здесь единственное место где порядок загрузки зашит (как inittab, но честнее). */
#include "../libc/washi.h"

/* адреса точек входа — из build/link.<target>.lds (статика, без exec!) */
extern void _start_fb(void);      extern void _start_uart(void);
extern void _start_clock(void);   extern void _start_ide(void);
extern void _start_kifu(void);    extern void _start_kanzashi(void);
extern void _start_terminal(void);extern void _start_clockapp(void);

static u32 go(void (*f)(void), int nice){ return spawn((u32)f, 0, nice); }

int main(void)
{
    /* приоритеты BeOS-style: драйверы выше GUI, GUI выше приложений */
    go(_start_uart,     -15);   /* console раньше всех — иначе негде кричать об ошибках */
    go(_start_clock,    -18);   /* таймер: от него живут все sleep/quantum */
    go(_start_ide,      -12);
    go(_start_fb,       -10);
    u32 kifu = go(_start_kifu,   -8);
    (void)kifu;
    sched_yield(); sched_yield();          /* дать серверам открыться (очереди ядра примут
                                              сообщения даже если сервер ещё не в цикле) */
    go(_start_kanzashi,  -5);
    sched_yield(); sched_yield();
    go(_start_terminal,   0);
    go(_start_clockapp,  10);              /* часы низкий приоритет: decorative */
    /* init сам становится watchdog'ом: раз в секунду проверяем что kanzashi жив,
     * иначе перезапускаем GUI (Plan 9-style "если сервер упал — systemd нет, есть init") */
    fid p = chan_open(DEV_PROC,"list");
    for(;;){
        Msg m={0}; m.type=Tread; m.fid=p;
        rpc(p,&m);                          /* ответ содержит список процессов текстом */
        for(int i=0;i<64;i++) sched_yield();/* ~1 сек при кванте 4 и RR */
    }
    return 0;
}
