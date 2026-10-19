/* miyabi-os/server/kanzashi.c — GUI-сервер «kanzashi» (簪).
 * Фасадная механика: окна, события, композитинг. НЕ знает что такое panel/command —
 * только хранит число mode у окна и пересылает строки. Политика в libc/apps. */
#include "../libc/washi.h"

#define MAXW 16
typedef struct Win {
    int   id;                 /* == fid на сервере */
    int   x,y,w,h,mode;
    char  label[32];
    u8   *buf;                /* offscreen w*h пикселей (палитра) */
    fid   evq[4];             /* подписчики events (клиенты могут слушать чужие окна —
                                 так работает модальность: панель M1 читает мышь окна) */
    int   nevq;
    int   alive;
} Win;

static Win wins[MAXW];
static int nwin;

/* вывод через fb-драйвер (он сам сервер DEV_FB): "blit x y w h" + пиксели.
 * Разрешение/глубина известны только драйверу — kanzashi не спрашивает! */
static fid fbf;
static void fb_blit(int x,int y,int w,int h,u8 *pix)
{
    Msg m={0}; m.type=Twrite; m.fid=fbf;
    m.off = x | (y<<9) | ((w&0x7FF)<<18) | ((h&0x3FF)<<27);
    int sent=0;
    while(sent<w*h){
        m.count = (w*h-sent)>240?240:(w*h-sent);
        for(int i=0;i<m.count;i++) m.data[i]=pix[sent+i];
        m.off = sent;                      /* драйвер помнит текущий blit-контекст
                                              по первому Twrite с geom в mode'е */
        chan_write(&m);
        sent+=m.count;
    }
}

static void ev_post(Win *wd, const char *text)
{
    Msg m={0}; m.type=Tevent; m.tag=0;
    int n=slen(text); if(n>240)n=240;
    m.count=n; for(int i=0;i<n;i++) m.data[i]=text[i];
    for(int i=0;i<wd->nevq;i++){ m.fid=wd->evq[i]; chan_write(&m); }
}

static Win *find_win(fid f){ for(int i=0;i<MAXW;i++) if(wins[i].alive&&wins[i].id==f) return &wins[i]; return NULL; }

static int sscanf_min(char*,int*,int*,int*,int*);   /* C89 forward — gcc -Wall -Werror */

int main(void)
{
    fbf = chan_open(DEV_FB,"screen");
    fid srv = chan_open(DEV_GUI,"/");      /* ядро демультиплицирует к нам по fid */
    u8 palette[16] = {1,1,2,3,3,4,5,6,6,7,8,9,10,11,12,15}; /* тёплая «миёби» палитра:
            ink #1a1a1a..paper #f5f0e6 индексы, зашита в драйвере как таблица ROM */

    Msg in;
    for(;;){
        if(chan_read(srv,&in)<0) continue;
        switch(in.type){
        case Topen: {
            /* пути: win/new -> окно; win/<id>/events -> подписка */
            char *p=(char*)in.data;
            if(p[0]=='w'&&p[1]=='i'&&p[2]=='n'&&p[3]=='/'&&p[4]=='n'){
                if(nwin==MAXW) break;
                Win *w=&wins[nwin++]; w->alive=1; w->id=in.fid+1000; w->mode=0;
                w->buf=create_segment(128*128);   /* MVP буфер 16KB, resize потом */
                Msg r={0}; r.type=Ropen; r.tag=in.tag; r.fid=w->id;
                chan_write(&r);
            } else { /* .../events */
                Win *w=find_win(in.fid);
                if(w&&w->nevq<4){ w->evq[w->nevq++]=in.fid;
                    Msg r={0};r.type=Ropen;r.tag=in.tag;r.fid=in.fid;chan_write(&r);}
            }
            break;
        }
        case Tctl: {
            Win *w=find_win(in.fid); if(!w) break;
            char *s=(char*)in.data;
            if(in.mode==0){                       /* geom "x y w h" */
                sscanf_min(s,&w->x,&w->y,&w->w,&w->h);
                ev_post(w,s);                     /* expose всем */
            } else if(in.mode==1){                /* label */
                int n=in.count>31?31:in.count; for(int i=0;i<n;i++)w->label[i]=s[i]; w->label[n]=0;
            } else if(in.mode==2){                /* modal N — ЕДИНСТВЕННАЯ мода сервера:
                                                     просто запоминаем число и шлём событие */
                w->mode=s[6]-'0';
                char t[16]; sfmt(t,16,"mode %d",w->mode,0); ev_post(w,t);
            }
            Msg r={0};r.type=Rctl;r.tag=in.tag;r.fid=in.fid;chan_write(&r);
            break;
        }
        case Twrite: {                            /* damage: x y w h в off, пиксели data */
            Win *w=find_win(in.fid); if(!w) break;
            int x=in.off&0xFF, y=(in.off>>8)&0xFF, ww=(in.off>>16)&0xFF, hh=(in.off>>24)&0xFF;
            for(int i=0;i<in.count;i++){
                int px=x+i%ww, py=y+i/ww;
                if(px<w->w&&py<w->h) w->buf[py*w->w+px]=palette[in.data[i]&15];
            }
            fb_blit(w->x+w->bw, w->y+12, ww, hh, w->buf+y*w->w+x);  /* +12 = titlebar рисует fb */
            break;
        }
        case Tread: {                             /* events poll: копия последнего хвоста
                                                    очередей делает ядро, тут no-op ack */
            Msg r={0};r.type=Rread;r.tag=in.tag;r.fid=in.fid;chan_write(&r);
            break;
        }
        case Tclunk: {
            Win *w=find_win(in.fid);
            if(w){ w->alive=0; nwin--; }
            Msg r={0};r.type=Rclunk;r.tag=in.tag;chan_write(&r);
            break;
        }
        }
    }
    return 0;
}

static int sscanf_min(char *s,int*a,int*b,int*c,int*d)
{
    int v[4]={0},ni=0;
    for(int i=0;s[i]&&ni<4;i++){
        if(s[i]>='0'&&s[i]<='9') v[ni]=v[ni]*10+s[i]-'0';
        else if(s[i]==' ') ni++;
    }
    *a=v[0];*b=v[1];*c=v[2];*d=v[3];return ni+1;
}
