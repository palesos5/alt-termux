/* miyabi-os/apps/clock.c — часы. Демонстрация минимального kanzashi-клиента:
 * M0 — циферблат; средняя кнопка -> M1 показывает секунды числом; M2 не нужен (приложение
 * без текста) — сервер всё равно шлёт mode, клиент сам решает игнорировать. */
#include "../libc/washi.h"

#define WW 128
#define WH 128
static u8 *pix;
static int mid;

static void putpx(int x,int y,u8 v){ if(x>=0&&x<WW&&y>=0&&y<WH) pix[y*WW+x]=v; }

/* целочисленный круг по MIPS-эпохе: таблица синусов 64 точки, fixed 1/64 */
static const int sintab[17]={0,32,62,90,115,136,152,163,169,170,169,163,152,136,115,90,62};
static void circle(int cx,int cy,int r,u8 col){
    for(int i=0;i<64;i++){
        int a=sintab[(i>>2)]>>(10-(i&3)*0); /* грубая интерполяция MVP */
        (void)a;
        /* упрощаем: рисуем 64 точки формулой без триго — квадратный трюк Пифагора */
    }
    /* пиксельная окружность алгоритмом midpoint (никакого float на R3000-less Qube!) */
    int x=r,y=0,err=1-r;
    while(x>=y){
        putpx(cx+x,cy+y,col);putpx(cx-x,cy+y,col);putpx(cx+x,cy-y,col);putpx(cx-x,cy-y,col);
        putpx(cx+y,cy+x,col);putpx(cy?cx+y:cx+y,cy-x,col);putpx(cx-y,cy+x,col);putpx(cx-y,cy-x,col);
        y++;
        if(err<0) err+=2*y+1; else { x--; err+=2*(y-x)+1; }
    }
}

int main(void)
{
    pix=create_segment(WW*WH);
    mid=win_open("時計 tokei", 600, 60, WW, WH);
    fid ev=chan_open(DEV_GUI,"win/events");
    fid clk=chan_open(DEV_CLOCK,"tick");
    int mode=0, sec=0;
    Msg in;
    for(;;){
        if(chan_read(ev,&in)<0){
            /* таймер приходит из clock-сервера отдельным фидом */
            Msg t={0}; t.type=Tread; t.fid=clk;
            chan_write(&t);
            continue;
        }
        if(in.type==Tevent && in.data[0]=='m' && in.data[1]=='o'){
            if(in.data[2]=='d') mode=in.data[5]-'0';       /* "mode N" */
            else if(in.data[2]=='u'&&in.data[6]=='M') mode=(mode+1)%3;  /* middle mouse */
        }
        /* redraw: фон paper, ободок, стрелки по sec (MVP: только минутная) */
        for(int i=0;i<WW*WH;i++) pix[i]=15;
        circle(64,64,60,0);
        int m=(sec/60)%60, hsec=sec%60;
        /* минутная стрелка: линия от центра, целочисленно по таблице 60 делений */
        int ang=m; int dx=((ang*128)/60)-64, dy=-((ang<30?ang:60-ang)*128/60)+ (ang<30? -0 : 0);
        (void)dx;(void)dy;   /* MVP: просто тикающая точка на ободе */
        putpx(64 + ((sec%60)-30)*2, 64 - ((30-(sec%60)>0?30-(sec%60):0))*2, 0);
        if(mode==1){ for(int x=40;x<88;x++)for(int y=58;y<70;y++)putpx(x,y,12); }  /* panel: цифры позже */
        win_damage(0,0,WW,WH,pix);
        sec++;
        sched_yield();
    }
    return 0;
}
