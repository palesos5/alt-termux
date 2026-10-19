/* miyabi-os/server/kifu.c — файловый сервер «kifu» (記録).
 * Одна ФС, плоская, 512B блоки, индекс в блоке 0. Читает образ через DEV_IDE.
 * Никаких прав/дат/ссылок — файл = имя + байты. Всё остальное политика клиентов. */
#include "../libc/washi.h"

#define NFILE 32
#define BLK   512
typedef struct F { char name[24]; u32 blk; u32 size; int alive; } F;
static F files[NFILE];
static fid idef;

/* примитивный текстовый разбор числа из строки "blk %d size %d" */
static int pnum(char *s)
{
    int v=0; while(*s && (*s<'0'||*s>'9')) s++;
    while(*s>='0'&&*s<='9') v=v*10+(*s++-'0');
    return v;
}

int main(void)
{
    idef = chan_open(DEV_IDE,"/");
    fid srv = chan_open(DEV_KIFU,"/");

    /* суперблок: список файлов текстом "name blk size\n..." — читаем блок 0 */
    Msg r={0}; r.type=Tread; r.fid=idef; r.count=BLK;
    Msg *resp = rpc(idef,&r);
    if(resp){
        char *p=(char*)resp->data, *e=p+resp->count;
        while(p<e){
            F *f=NULL;
            for(int i=0;i<NFILE;i++) if(!files[i].alive){ f=&files[i]; break; }
            if(!f) break;
            int n=0; while(p[n]&&p[n]!=' ')n++;
            for(int i=0;i<n&&i<23;i++) f->name[i]=p[i]; f->name[n<24?n:23]=0;
            p+=n+1; f->blk=pnum(p); p=pnum(p)?strchr(p,' ')+1:p; /* грубо: дальше size */
            f->size=pnum(p);
            while(p<e&&*p!='\n')p++; p++;
            f->alive=1;
        }
    }

    Msg in;
    for(;;){
        if(chan_read(srv,&in)<0) continue;
        switch(in.type){
        case Topen: {
            char *path=(char*)in.data;
            F *hit=NULL;
            for(int i=0;i<NFILE;i++) if(files[i].alive&&scmp(files[i].name,path)==0) hit=&files[i];
            Msg o={0}; o.type=Ropen; o.tag=in.tag;
            if(hit){ o.fid=in.fid; o.mode=hit->size;      /* размер в mode: урезанный протокол */
                     o.count=sfmt((char*)o.data,240,"%d %d",hit->blk,hit->size,0); }
            else o.count=0;                                /* ENOENT пустым ответом */
            chan_write(&o);
            break;
        }
        case Tread: {                                       /* off -> IDE блок */
            F *f=NULL; /* fid клиента -> ищем по имени кэшированно: MVP: off>>9==blk base */
            (void)f;
            Msg q={0}; q.type=Tread; q.fid=idef; q.off=in.off/BLK; q.count=BLK;
            Msg *ir=rpc(idef,&q);
            Msg o={0}; o.type=Rread; o.tag=in.tag; o.fid=in.fid;
            if(ir){ o.count=BLK; for(int i=0;i<BLK&&i<240;i++) o.data[i]=ir->data[i]; }
            chan_write(&o);
            break;
        }
        case Tclunk: { Msg o={0};o.type=Rclunk;o.tag=in.tag;chan_write(&o); break; }
        }
    }
    return 0;
}
