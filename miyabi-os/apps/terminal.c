/* miyabi-os/apps/terminal.c — терминал kanzashi-клиент с ТРЕМЯ модальностями.
 * M0: печатаешь — символы летят в shell через /dev/uart канал.
 * M1 (средняя кнопка): панель «cut|copy|paste|clear» над строкой ввода (BeOS-style).
 * M2 (Ctrl-E): командная строка внизу, acme-style: "edit 3:7", "send ls".
 * Никаких fork/exec терминов — приложение пишет текст, сервер делает остальное. */
#include "../libc/washi.h"

static int mid;                 /* мой window id */
static char line[240];          /* текущий ввод */
static int nl;
static int mode = 0;            /* локальная копия модальности (истина — у клиента!) */
static u8 *pix;                 /* offscreen буфер окна 320x200 палитра */

#define WW 320
#define WH 200

static void putpx(int x,int y,u8 v){ if(x<WW&&y<WH) pix[y*WW+x]=v; }

/* bitmap шрифт 8x12: только ASCII 0x20..0x7E, 96 глифов по 12 байт.
 * Сгенерирован вручную из ROM SGI mini-font (см. fs/font8x12.hex в образе),
 * тут — трюация первых букв для MVP; полный массив лежит в образе как файл. */
static const u8 *font;          /* грузится из kifu при старте */

static void text_out(int x,int y,const char *s)
{
    for(;*s;s++,x+=8){
        const u8 *g = font + (*s&0x7F)*12;
        for(int r=0;r<12;r++)
            for(int c=0;c<8;c++)
                if(g[r]&(0x80>>c)) putpx(x+c,y+r,0);   /* 0 = ink */
    }
}

static void draw(void)
{
    for(int i=0;i<WW*WH;i++) pix[i]=15;                /* paper */
    /* рамка заголовка рисует fb-сервер; мы рисуем контент + modality chrome */
    text_out(4, 4,  "sh$ ");
    text_out(28,4,  line);
    if(mode==1){                                       /* M1 panel поверх */
        for(int y=20;y<44;y++) for(int x=0;x<WW;x++) putpx(x,y,12);
        text_out(8, 24, "[ cut ] [ copy ] [ paste ] [ clear ]");
    }
    if(mode==2){                                       /* M2 command line снизу */
        for(int y=WH-16;y<WH;y++) for(int x=0;x<WW;x++) putpx(x,y,13);
        text_out(4, WH-14, "> ");
    }
    win_damage(0,0,WW,WH,pix);
}

int main(void)
{
    /* шрифт линкуется в образ утилитой mkfs как бинарный файл (ld -b binary — путь 2004) */
    extern char _binary_font8x12_start[];
    font=(const u8*)_binary_font8x12_start;

    pix = create_segment(WW*WH);
    mid = win_open("sh — яび terminal", 40, 40, WW, WH);
    fid ev = chan_open(DEV_GUI,"win/events");         /* подписка на свои события */
    fid con = chan_open(DEV_UART,"console");

    Msg in;
    for(;;){
        if(chan_read(ev,&in)<0) continue;
        if(in.type!=Tevent) continue;
        char *s=(char*)in.data;
        /* парсим текстовые события сервера (человекочитаемый протокол!) */
        if(s[0]=='m'&&s[1]=='o'&&s[2]=='d'&&s[3]=='e'){     /* "mode N" от другого подписчика */
            mode=s[5]-'0'; draw(); continue;
        }
        if(s[0]=='k'&&s[1]=='e'&&s[2]=='y'){                 /* key 'c' press */
            char c=s[5];
            if(c=='\x05'){                                    /* Ctrl-E -> M2 toggle */
                mode = (mode==2)?0:2;
                win_setmode(mid,mode);                        /* сказать серверу (и всем) */
                draw(); continue;
            }
            if(mode==2){                                      /* M2: команды"> ..." */
                if(c=='\r'){ /* exec command text as-is в консоль — фасад: shell разберёт */
                    Msg w={0};w.type=Twrite;w.fid=con;w.count=nl+1;
                    for(int i=0;i<nl;i++)w.data[i]=line[i]; w.data[nl]='\n';
                    chan_write(&w); nl=0; line[0]=0;
                    mode=0; win_setmode(mid,0);
                } else if(nl<239){ line[nl++]=c; line[nl]=0; }
            } else {                                          /* M0/M1: raw в shell */
                if(c=='\b'&&nl) nl--;
                else if(c=='\r'||c=='\n'){
                    Msg w={0};w.type=Twrite;w.fid=con;w.count=nl+1;
                    for(int i=0;i<nl;i++)w.data[i]=line[i]; w.data[nl]='\n';
                    chan_write(&w); nl=0;
                } else if(nl<239){ line[nl++]=c; line[nl]=0; }
            }
            draw(); continue;
        }
        if(s[0]=='m'&&s[1]=='o'&&s[2]=='u'&&s[3]=='s'&&s[5]=='e'){ /* mouse ... */
            /* "mouse x y M down" средняя кнопка = переключатель большой модальности */
            int mx=0,my=0,i=6; while(s[i]!=' ')mx=mx*10+s[i++]-'0'; i++;
            while(s[i]!=' ')my=my*10+s[i++]-'0'; i++;
            char btn=s[i];
            if(btn=='M'&&s[i+2]=='d'){                        /* middle down */
                mode=(mode+1)%3;                              /* M0->M1->M2->M0 cycle */
                win_setmode(mid,mode);
            }
            if(mode==1){                                      /* клики по панели M1 */
                if(my>=20&&my<44){
                    if(mx>8&&mx<52)   { /* cut */ }
                    if(mx>60&&mx<110) { /* copy */ }
                    if(mx>118&&mx<178){ /* paste */ }
                    if(mx>186&&mx<240){ nl=0; line[0]=0; }    /* clear */
                }
            }
            draw();
        }
    }
    return 0;
}
