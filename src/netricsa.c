#include "ssa/netricsa.h"
#include "ssa/map_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned u16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static const uint8_t *data(const SsaNetricsa *s,uint32_t address,size_t bytes)
{
    size_t off;if(address<0x08000000)return NULL;off=address-0x08000000;
    return off<=s->rom_size && bytes<=s->rom_size-off?s->rom+off:NULL;
}
const char *ssa_netricsa_text(const SsaNetricsa *s,unsigned id)
{
    const uint8_t *table,*p;
    if(!s || s->loaded!=1 || s->language>=3 || id>=252)return NULL;
    table=data(s,u32(s->rom+0x44cf34+s->language*4),252*4);
    if(!table || !(p=data(s,u32(table+id*4),1)) || !memchr(p,0,s->rom_size-(size_t)(p-s->rom)))return NULL;
    return (const char *)p;
}
static int decode(SsaNetricsa *s,uint32_t address,uint8_t *out,size_t bytes)
{
    const uint8_t *p=data(s,address,4);
    return p && u32(p)>>8==bytes && ssa_lz77_decode(p,s->rom_size-(size_t)(p-s->rom),out,bytes,NULL);
}
static void command(SsaNetricsa *s,unsigned id){if(s->command_count<8)s->commands[s->command_count++]=id;}
static unsigned lines(const char *p){unsigned n=1;while(*p)if(*p++==10)++n;return n;}
static int32_t signed32(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)(UINT32_MAX-n);}
static int format(SsaNetricsa *s,unsigned panel,unsigned id,const uint32_t *args,unsigned count,const char *word)
{
    const char *p=ssa_netricsa_text(s,id);size_t used=0;unsigned at=0;
    if(!p)return -1;
    while(*p){
        char value[32];const char *next=value;size_t n;
        if(*p!='%'){value[0]=*p++;value[1]=0;}
        else{unsigned pad=0;++p;
            if(*p=='%'){value[0]='%';value[1]=0;++p;}
            else if(*p=='s' && word){next=word;++p;}
            else{if(p[0]=='0' && p[1]=='2'){pad=1;p+=2;}
                if(*p!='d' || at>=count)return -1;
                (void)snprintf(value,sizeof(value),pad?"%02d":"%d",(int)signed32(args[at++]));++p;
            }
        }
        n=strlen(next);if(n>=sizeof(s->formatted[panel])-used)return -1;
        memcpy(s->formatted[panel]+used,next,n);used+=n;
    }
    s->formatted[panel][used]=0;s->text[panel]=s->formatted[panel];return at==count?1:-1;
}
static int records(SsaNetricsa *s)
{
    uint32_t a[7];unsigned i,w=s->weapon;const SsaStatisticsTotals *t=&s->data.totals;
    s->image=22;
    if(s->category==0){
        s->title[0]=130;s->title[1]=132;
        a[0]=t->elapsed/3600;a[1]=t->elapsed/60%60;a[2]=t->spawned;a[3]=t->kills;
        a[4]=t->hits;a[5]=t->weapon_hits;a[6]=t->pickups;
        if(format(s,0,131,a,7,NULL)!=1)return -1;
        a[0]=t->score;a[1]=(uint32_t)t->health;a[2]=(uint32_t)t->armor6;a[3]=(uint32_t)t->armor4;a[4]=s->data.lives;
        if(format(s,1,133,a,5,NULL)!=1)return -1;
        s->text_id[0]=s->text_id[1]=0;
    }else if(s->category==1){
        const SsaEnemyStatistics *e=&s->data.rows.enemies[s->enemy];
        s->image=s->enemy+10;s->title[0]=s->enemy+102;s->title[1]=128;
        s->text_id[0]=s->enemy+116;s->text[0]=ssa_netricsa_text(s,s->text_id[0]);
        a[0]=e->spawned;a[1]=e->hits;a[2]=e->kills;s->text_id[1]=0;
        /* German129 has a duplicated attacks row and four directives, while
         * the original caller supplies only three arguments. Fill that row
         * from the same attacks counter rather than reading past arguments. */
        if(s->language==1){a[2]=e->hits;a[3]=e->kills;}
        if(format(s,1,129,a,s->language==1?4:3,NULL)!=1)return -1;
    }else if(s->category==2){
        const SsaWeaponProgressStats *v=&s->data.rows.weapons[w];
        unsigned accuracy=v->shots?(uint32_t)(v->hits*100u)/v->shots:0;
        const char *yes=ssa_netricsa_text(s,v->owned==1?95:96);
        s->image=w;s->title[0]=w+74;s->title[1]=94;
        s->text_id[0]=w+84;s->text[0]=ssa_netricsa_text(s,s->text_id[0]);s->text_id[1]=0;
        if(!w){a[0]=v->hits;if(format(s,1,99,a,1,NULL)!=1)return -1;}
        else if(w==1){a[0]=v->shots;a[1]=v->hits;a[2]=accuracy;if(format(s,1,100,a,3,yes)!=1)return -1;}
        else if(w==7){a[0]=(uint32_t)v->ammo;a[1]=v->shots;a[2]=v->hits;if(format(s,1,101,a,3,yes)!=1)return -1;}
        else{a[0]=(uint32_t)v->ammo;a[1]=v->shots;a[2]=v->hits;a[3]=accuracy;if(format(s,1,98,a,4,yes)!=1)return -1;}
    }else{
        s->title[0]=135;s->title[1]=s->category==3?226:134;
        s->text_id[0]=(s->category==3?214:202)+s->data.level;
        s->text_id[1]=s->category==3?227+s->data.random%10:0;
        s->text[0]=ssa_netricsa_text(s,s->text_id[0]);
        if(s->category==3)s->text[1]=ssa_netricsa_text(s,s->text_id[1]);
        else{const char *empty=ssa_netricsa_text(s,136);
            if(!empty || snprintf(s->formatted[1],sizeof(s->formatted[1]),"%s\n",empty)>=(int)sizeof(s->formatted[1]))return -1;
            s->text[1]=s->formatted[1];}
    }
    for(i=0;i<2;++i)if(!s->text[i])return -1;
    return 1;
}
int ssa_netricsa_init(SsaNetricsa *out,const uint8_t *rom,size_t size,unsigned language,const SsaNetricsaData *snapshot)
{
    SsaNetricsa *s;uint8_t map[1200];unsigned i;uint32_t count=0;int result=-1;
    if(!out || out->loaded || !rom || size!=0x800000 || language>=3 || !snapshot || snapshot->level>=12 || snapshot->show_all>1)return -1;
    s=calloc(1,sizeof(*s));if(!s)return -1;
    s->rom=rom;s->rom_size=size;s->language=language;s->data=*snapshot;s->loaded=1;
    if(!decode(s,0x080a8368,s->tiles,sizeof(s->tiles)) || !decode(s,0x080aaf38,s->frame_tiles,sizeof(s->frame_tiles)) ||
       !decode(s,0x080b4c20,map,sizeof(map)) || !decode(s,0x080b7a60,s->fonts,sizeof(s->fonts)))goto done;
    for(i=0;i<23;++i)if(!decode(s,u32(rom+0x44cf48+i*4),s->images[i],4096))goto done;
    for(i=0;i<256;++i)s->palette[i]=(uint16_t)u16(rom+0xa8168+i*2);
    for(i=0;i<600;++i){s->frame_map[i]=(uint16_t)u16(map+i*2);if((s->frame_map[i]&1023)>=54)goto done;}
    s->palette[60]=0x1fe0;for(i=61;i<64;++i)s->palette[i]=0x1280;
    for(i=0;i<12;++i)count+=snapshot->rows.enemies[i].spawned;
    s->has_enemies=snapshot->show_all || count!=0;
    for(i=0;i<12;++i)if(snapshot->rows.enemies[i].spawned || snapshot->show_all){s->enemy=i;break;}
    for(i=0;i<10;++i)if(snapshot->rows.weapons[i].owned){s->weapon=i;break;}
    if(i==10)goto done; /* Avoid the original infinite unavailable-weapon scan. */
    s->category=4;s->focus=1;s->fade_volume=0xf00;s->fade_step=-64;
    if(records(s)!=1)goto done;
    command(s,7);*out=*s;
    /* Repair pointers to embedded formatted text after the transactional copy. */
    for(i=0;i<2;++i)if(s->text[i]==s->formatted[i])out->text[i]=out->formatted[i];
    result=1;
done:free(s);return result;
}
static void change_record(SsaNetricsa *s,int direction)
{
    unsigned i,limit=s->category==1?12:10,*index=s->category==1?&s->enemy:&s->weapon;
    if(s->category!=1 && s->category!=2)return;
    for(i=0;i<limit;++i){*index=(*index+limit+(direction<0?-1:1))%limit;
        if(s->category==1?(s->data.show_all || s->data.rows.enemies[*index].spawned):s->data.rows.weapons[*index].owned)break;
    }
    s->scroll[0]=s->scroll[1]=0;command(s,12);
}
int ssa_netricsa_step(SsaNetricsa *s,uint32_t input)
{
    unsigned i,changed=0;
    if(!s || s->loaded!=1 || s->category>4 || s->focus>2 || s->weapon>=10 || s->enemy>=12 || !s->text[0] || !s->text[1])return -1;
    s->command_count=0;if(s->done)return 0;
    if(s->fade_step){s->fade_volume+=s->fade_step;
        if(s->fade_volume<0){s->fade_volume=0;s->fade_step=0;}
        else if(s->fade_volume>0xf00){s->fade_volume=0xf00;s->fade_step=0;s->done=s->closing;}
    }
    if(!s->closing){
        if(input&SSA_NET_CLOSE){s->closing=1;s->fade_volume=0;s->fade_step=64;command(s,0x10c);input=0;}
        if(input&SSA_NET_FOCUS){s->focus=(s->focus+1)%3;s->delta[0]=s->delta[1]=0;command(s,0x10b);}
        if(!s->focus && (input&(SSA_NET_UP|SSA_NET_DOWN))){
            if(input&SSA_NET_UP){if(s->category<4)++s->category;if(s->category==1 && !s->has_enemies)++s->category;}
            else{if(s->category)--s->category;if(s->category==1 && !s->has_enemies)--s->category;}
            s->scroll[0]=s->scroll[1]=0;changed=1;command(s,12);
            /*1A2C4 refreshes records here and again in its common postlude.
             * Simultaneous Left/Right therefore visits the record twice. */
            if(input&SSA_NET_PREVIOUS)change_record(s,-1);
            if(input&SSA_NET_NEXT)change_record(s,1);
        }else if(s->focus){
            unsigned panel=s->focus-1;
            s->delta[panel]=input&SSA_NET_SCROLL_UP?-1:input&SSA_NET_SCROLL_DOWN?1:0;
        }
        if(input&SSA_NET_PREVIOUS){change_record(s,-1);changed=1;}
        if(input&SSA_NET_NEXT){change_record(s,1);changed=1;}
    }
    if(changed && records(s)!=1)return -1;
    for(i=0;i<2;++i){int maximum=((int)lines(s->text[i])-(i?9:7))*8;
        if(maximum<0)maximum=0;
        s->scroll[i]+=s->delta[i];
        if(s->scroll[i]>maximum)s->scroll[i]=maximum;
        if(s->scroll[i]<0){s->scroll[i]=0;s->delta[i]=-s->delta[i];}
    }
    if(!(s->counter&7))s->background_scroll=(s->background_scroll+64)&511;
    ++s->counter;return s->done?0:1;
}
int ssa_netricsa_pc_step(SsaNetricsa *s,uint32_t input)
{
    int result,volume;unsigned closing;
    if(!s || s->fade_volume<0 || s->fade_volume>0xf00)return -1;
    volume=s->fade_volume;closing=s->closing;
    if(s->fade_step)s->fade_step=s->fade_step<0?-256:256;
    result=ssa_netricsa_step(s,input);if(result<0)return result;
    if(!closing && s->closing){s->fade_volume=volume+256;s->fade_step=256;}
    if(s->fade_volume>=0xf00 && s->closing){s->fade_volume=0xf00;s->fade_step=0;s->done=1;}
    else if(!s->fade_volume)s->fade_step=0;
    return s->done?0:1;
}
int ssa_netricsa_row(const SsaNetricsa *s,const char *p,unsigned row,unsigned width,uint8_t *out,size_t bytes)
{
    unsigned line=0,palette=0,x=2,y,n=0;uint8_t pixels[8][256]={{0}};
    if(!s || s->loaded!=1 || !p || !out || !width || width>32 || bytes<width*64 || row>65535)return -1;
    for(;;){
        if(*p=='*'){if(p[1]<'0' || p[1]>'9')return -1;palette=(unsigned)(p[1]-'0');p+=2;}
        if(line==row)break;
        while(*p && *p!=10)++p;
        if(!*p){memset(out,0,width*64);return 1;}++p;++line;
    }
    while(*p && *p!=10){unsigned glyph,origin,w;
        if(*p=='*'){if(p[1]<'0' || p[1]>'9')return -1;palette=(unsigned)(p[1]-'0');p+=2;continue;}
        /*18D70 copies at most31 bytes into its32-byte line buffer. */
        if(n++>=31)break;
        glyph=s->rom[0x44c264+(uint8_t)*p++];
        if(glyph){
            if(glyph>=126)return -1;
            origin=s->rom[0xb7ea8+2*glyph];w=s->rom[0xb7ea9+2*glyph];if(w>8)w=1;
            if(origin+w>8)return -1;
            for(y=0;y<8;++y){unsigned k;for(k=0;k<w && x+k<256;++k){
                unsigned sx=origin+k,b=s->fonts[glyph*32+y*4+sx/2],c=(b>>((sx&1)*4))&15;
                if(c)pixels[y][x+k]=(uint8_t)(c|(palette<<4));
            }}
        }else w=4;
        x+=w+1;
    }
    for(y=0;y<8;++y){unsigned k;for(k=0;k<width*8;++k)out[(k/8)*64+y*8+(k&7)]=pixels[y][k];}
    return 1;
}
static int record_available(const SsaNetricsa *s,unsigned index)
{
    return s->category==1?(s->data.show_all || s->data.rows.enemies[index].spawned!=0):s->data.rows.weapons[index].owned!=0;
}
enum { SELECTOR_LEFT=90,SELECTOR_STRIDE=8,SELECTOR_TOP=17,SKIP_LEFT=134,SKIP_TOP=9 };
static int skip_width(const SsaNetricsa *s)
{
    const char *p=ssa_netricsa_text(s,64);int width=0;
    if(!p)return 0;
    while(*p){unsigned c=(uint8_t)*p++,glyph=s->rom[0x44c264+c];
        if(c==32)width+=4;
        else if(glyph<126)width+=s->rom[0xb7ea9+2*glyph]+1;
    }
    return width;
}
int ssa_netricsa_pointer(SsaNetricsa *s,int x,int y,unsigned click)
{
    unsigned region=0,i,before;
    if(!s || s->loaded!=1 || s->closing)return 0;
    /* Keep the blinking prompt clickable during its invisible half too. */
    if(x>=SKIP_LEFT && x<240 && x<SKIP_LEFT+skip_width(s) && y>=SKIP_TOP && y<SKIP_TOP+8){
        s->hover=0;return click?SSA_NET_POINTER_CLOSE:0;
    }
    if(x>=9 && x<85)for(i=0;i<5;++i)if(y>=73-(int)i*12 && y<81-(int)i*12){region=i+1;break;}
    if(x>=91 && x<235 && y>=33 && y<81)region=6;
    if(x>=115 && x<235 && y>=97 && y<160)region=7;
    if((s->category==1 || s->category==2) && x>=24 && x<88 && y>=89 && y<153)region=x<56?8:9;
    if((s->category==1 || s->category==2) && y>=SELECTOR_TOP && y<SELECTOR_TOP+8 && x>=SELECTOR_LEFT){
        unsigned count=s->category==1?12:10,index=(unsigned)(x-SELECTOR_LEFT)/SELECTOR_STRIDE;
        if(index<count && record_available(s,index))region=10+index;
    }
    if(region==2 && !s->has_enemies)region=0;
    before=s->hover;s->hover=region;
    if(!region)return 0;
    if(region>=10){unsigned *index=s->category==1?&s->enemy:&s->weapon;
        if(!click || *index==region-10)return 0;
        *index=region-10;s->scroll[0]=s->scroll[1]=s->delta[0]=s->delta[1]=0;
        return records(s)==1?1:-1;
    }
    if(region>=8){if(!click)return 0;change_record(s,region==8?-1:1);return records(s)==1?1:-1;}
    if(click){s->focus=region<=5?0:region-6+1;s->delta[0]=s->delta[1]=0;}
    if(region<=5 && region!=before && s->category!=region-1){s->category=region-1;s->scroll[0]=s->scroll[1]=0;
        s->delta[0]=s->delta[1]=0;
        if(records(s)!=1)return -1;
        return 1;}
    return 0;
}
int ssa_netricsa_wheel(SsaNetricsa *s,int steps)
{
    unsigned panel;int64_t value;int maximum;
    if(!s || s->loaded!=1 || s->closing || !steps)return 0;
    if(s->hover>=8 && (s->category==1 || s->category==2)){
        int64_t count=steps<0?-(int64_t)steps:steps;if(count>32)count=32;
        while(count--)change_record(s,steps>0?-1:1);
        return records(s)==1?1:-1;
    }
    panel=s->hover>=6 && s->hover<=7?s->hover-6:s->focus?s->focus-1:0;
    maximum=((int)lines(s->text[panel])-(panel?9:7))*8;if(maximum<0)maximum=0;
    value=(int64_t)s->scroll[panel]-(int64_t)steps*24;
    s->scroll[panel]=value<0?0:value>maximum?maximum:(int)value;s->focus=panel+1;s->delta[0]=s->delta[1]=0;return 1;
}
static void text_label(const SsaNetricsa *s,uint16_t *out,uint8_t *mask,const char *p,int x,int y,unsigned palette,unsigned shade)
{
    int start=x;if(!p)return;
    while(*p){unsigned c=(uint8_t)*p++,glyph=s->rom[0x44c264+c],w,k,dy,origin;
        if(c==10){x=start;y+=8;continue;}if(c==32){x+=4;continue;}
        if(glyph>=126)continue;
        origin=s->rom[0xb7ea8+2*glyph];w=s->rom[0xb7ea9+2*glyph];
        for(dy=0;dy<8;++dy)for(k=0;k<8;++k){int dx=x+(int)k-(int)origin,sy=y+(int)dy;
            unsigned color=(s->fonts[glyph*32+dy*4+k/2]>>((k&1)*4))&15;
            if(color && dx>=0 && dx<240 && sy>=0 && sy<160){unsigned at=(unsigned)sy*240+(unsigned)dx;
                unsigned c=s->palette[palette*16+color];
                out[at]=(uint16_t)(((c&31)*shade/16)|((((c>>5)&31)*shade/16)<<5)|((((c>>10)&31)*shade/16)<<10));
                if(mask)mask[at]=255;}
        }x+=(int)w+1;
    }
}
static void label(const SsaNetricsa *s,uint16_t *out,uint8_t *mask,unsigned id,int x,int y,unsigned palette)
{text_label(s,out,mask,ssa_netricsa_text(s,id),x,y,palette,16);}
static void selectors(const SsaNetricsa *s,uint16_t *out,uint8_t *mask)
{
    unsigned count,i,current;
    if(s->category!=1 && s->category!=2)return;
    count=s->category==1?12:10;current=s->category==1?s->enemy:s->weapon;
    for(i=0;i<count;++i){char number[12];
        unsigned palette=i==current?0:s->hover==10+i?1:2;
        if(!record_available(s,i))continue;
        (void)snprintf(number,sizeof(number),"%u",i+1);
        text_label(s,out,mask,number,SELECTOR_LEFT+(int)(i*SELECTOR_STRIDE),SELECTOR_TOP,palette,16);
    }
}
static int draw(const SsaNetricsa *s,uint16_t *out,size_t pixels,uint8_t *mask)
{
    unsigned x,y,i;uint8_t row[18*64];
    if(!s || s->loaded!=1 || !out || pixels<38400 || s->image>=23 || s->fade_volume<0 || s->fade_volume>0xf00)return -1;
    for(y=0;y<160;++y)for(x=0;x<240;++x){
        unsigned sx=(x+s->background_scroll)&511,tile=(64-(y/64)*8+((y/8)&7)*64+sx/8)&511;
        unsigned color=s->tiles[tile*64+(y&7)*8+(sx&7)],map=s->frame_map[(y/8)*30+x/8];
        unsigned fx=map&1024?7-(x&7):x&7,fy=map&2048?7-(y&7):y&7;
        unsigned frame=s->frame_tiles[(map&1023)*64+fy*8+fx];
        if(mask)mask[y*240+x]=frame?255:0;
        color=frame?frame:mask?0:color;
        out[y*240+x]=color>=60 && color<64?(color==60+s->focus?0x1fe0:0x1280):s->palette[color];
    }
    for(i=0;i<2;++i){unsigned width=i?15:18,top=i?97:33,left=i?115:91,height=i?63:48;
        unsigned previous=UINT32_MAX;
        for(y=0;y<height;++y){unsigned sy=(unsigned)s->scroll[i]+y,r=sy/8;
            if(r!=previous){if(ssa_netricsa_row(s,s->text[i],r,width,row,sizeof(row))!=1)return -1;previous=r;}
            for(x=0;x<width*8;++x){unsigned color=row[(x/8)*64+(sy&7)*8+(x&7)];
                if(color){unsigned at=(top+y)*240+left+x;out[at]=s->palette[color];if(mask)mask[at]=255;}}
        }
    }
    for(y=0;y<64;++y)for(x=0;x<64;++x){unsigned color=s->images[s->image][((y/8)*8+x/8)*64+(y&7)*8+(x&7)];
        if(color){unsigned at=(89+y)*240+24+x;out[at]=s->palette[color];if(mask)mask[at]=255;}}
    for(i=0;i<5;++i){
        if(i==1 && !s->has_enemies)text_label(s,out,mask,ssa_netricsa_text(s,68),9,61,1,4);
        else label(s,out,mask,67+i,9,73-(int)i*12,s->category==i?1:0);
    }
    label(s,out,mask,s->title[0],91,25,1);label(s,out,mask,s->title[1],115,89,1);
    selectors(s,out,mask);
    if(s->counter&32)label(s,out,mask,64,SKIP_LEFT,SKIP_TOP,1);
    for(i=0;i<38400;++i){unsigned c=out[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31,d=(unsigned)s->fade_volume>>8;
        if(mask)mask[i]=(uint8_t)(mask[i]*(0xf00-(unsigned)s->fade_volume)/0xf00);
        else out[i]=(uint16_t)((r-r*d/16)|((g-g*d/16)<<5)|((b-b*d/16)<<10));}
    return 1;
}
int ssa_netricsa_draw(const SsaNetricsa *s,uint16_t *out,size_t pixels)
{return draw(s,out,pixels,NULL);}
int ssa_netricsa_overlay(const SsaNetricsa *s,const uint32_t *background,const uint32_t *with_hud,unsigned w,unsigned h,uint32_t *out)
{
    uint16_t layer[38400];uint8_t mask[38400];unsigned width,height,left,top,x,y,fade;
    size_t count,i;
    if(!background || !out || !w || !h || (uint64_t)w*h>SIZE_MAX/sizeof(*out) || draw(s,layer,38400,mask)!=1)return -1;
    count=(size_t)w*h;fade=(0xf00-(unsigned)s->fade_volume)*255/0xf00;
    if(!with_hud)with_hud=background;
    for(i=0;i<count;++i){unsigned c=background[i],hud=with_hud[i];
        unsigned luma=(77*((c>>16)&255)+150*((c>>8)&255)+29*(c&255)+128)>>8;
        unsigned r=((((hud>>16)&255)*(255-fade))+(luma*18/100)*fade+127)/255;
        unsigned g=((((hud>>8)&255)*(255-fade))+(luma*48/100)*fade+127)/255;
        unsigned b=(((hud&255)*(255-fade))+(luma*25/100)*fade+127)/255;
        out[i]=(r<<16)|(g<<8)|b;
    }
    if((uint64_t)w*160<=(uint64_t)h*240){width=w;height=(unsigned)((uint64_t)w*160/240);if(!height)height=1;}
    else{height=h;width=(unsigned)((uint64_t)h*240/160);if(!width)width=1;}
    left=(w-width)/2;top=(h-height)/2;
    for(y=0;y<height;++y)for(x=0;x<width;++x){
        unsigned p=(unsigned)((uint64_t)y*160/height)*240+(unsigned)((uint64_t)x*240/width),a=mask[p];
        size_t at=(size_t)(top+y)*w+left+x;unsigned c=layer[p],bg=out[at],r,g,b;
        if(!a)continue;
        r=(((c&31)*255/31)*a+((bg>>16)&255)*(255-a)+127)/255;
        g=((((c>>5)&31)*255/31)*a+((bg>>8)&255)*(255-a)+127)/255;
        b=((((c>>10)&31)*255/31)*a+(bg&255)*(255-a)+127)/255;
        out[at]=(r<<16)|(g<<8)|b;
    }
    return 1;
}
