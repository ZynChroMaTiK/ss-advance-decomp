#include "ssa/netricsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"NETRICSA line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void word(FILE *f,unsigned n,unsigned bytes){while(bytes--){fputc(n&255,f);n>>=8;}}
static int bitmap(const char *path,const uint16_t *pixels){
    FILE *f=fopen(path,"wb");unsigned i;if(!f)return 0;
    fputs("BM",f);word(f,54+38400*4,4);word(f,0,4);word(f,54,4);word(f,40,4);word(f,240,4);word(f,(unsigned)-160,4);
    word(f,1,2);word(f,32,2);word(f,0,4);word(f,38400*4,4);word(f,0,4);word(f,0,4);word(f,0,4);word(f,0,4);
    for(i=0;i<38400;++i){unsigned c=pixels[i];word(f,(((c&31)*255/31)<<16)|((((c>>5)&31)*255/31)<<8)|(((c>>10)&31)*255/31),4);}
    return fclose(f)==0;
}
static int selector_style(SsaNetricsa *s,uint16_t *pixels,const char *preview)
{
    unsigned state,y,k,x,n=0,glyph=s->rom[0x44c264+'1'],origin=s->rom[0xb7ea8+2*glyph],category=s->category;
    uint16_t background[38400];
    static const unsigned palettes[]={2,1,0};
    CHECK(ssa_netricsa_pointer(s,102,21,1)>=0 && s->weapon==1);
    CHECK(ssa_netricsa_pointer(s,-1,-1,0)==0);
    s->category=4;CHECK(ssa_netricsa_draw(s,background,38400)==1);s->category=category;
    for(state=0;state<3;++state){
        if(state)CHECK(ssa_netricsa_pointer(s,94,21,state==2)>=0);
        CHECK(ssa_netricsa_draw(s,pixels,38400)==1);
        for(y=0;y<8;++y)for(k=0;k<8;++k){unsigned color=(s->fonts[glyph*32+y*4+k/2]>>((k&1)*4))&15;
            if(color){CHECK(pixels[(17+y)*240+90+k-origin]==s->palette[palettes[state]*16+color]);++n;}}
        for(y=17;y<25;++y){
            for(x=0;x<90;++x)CHECK(pixels[y*240+x]==background[y*240+x]);
            for(x=170;x<240;++x)CHECK(pixels[y*240+x]==background[y*240+x]);
            for(x=90;x<98;++x){unsigned k=x-90+origin,color=k<8?(s->fonts[glyph*32+(y-17)*4+k/2]>>((k&1)*4))&15:0;
                if(!color)CHECK(pixels[y*240+x]==background[y*240+x]);}
        }
        if(state==1 && preview){char path[2048];
            CHECK(snprintf(path,sizeof(path),"%s.selectors.bmp",preview)<(int)sizeof(path));
            CHECK(bitmap(path,pixels));}
    }
    CHECK(n>0 && s->weapon==0);return 0;
}
static int disabled_records(SsaNetricsa *s,uint16_t *pixels,const char *preview)
{
    uint16_t background[38400];unsigned y,x,k,glyph,origin,n=0;
    const char *label=ssa_netricsa_text(s,68);
    CHECK(!s->has_enemies && s->category==2 && s->weapon==1 && label);
    s->fade_volume=0;s->category=4;
    CHECK(ssa_netricsa_draw(s,background,38400)==1);s->category=2;
    CHECK(ssa_netricsa_draw(s,pixels,38400)==1);
    /* Only the owned pistol's cell may contain number pixels. Other cells
     * and the old left-side number area must be untouched background. */
    for(y=17;y<25;++y)for(x=0;x<240;++x)if(x<98 || x>=106)
        CHECK(pixels[y*240+x]==background[y*240+x]);
    glyph=s->rom[0x44c264+(uint8_t)*label];origin=s->rom[0xb7ea8+2*glyph];
    for(y=0;y<8;++y)for(k=0;k<8;++k){unsigned color=(s->fonts[glyph*32+y*4+k/2]>>((k&1)*4))&15;
        if(color){unsigned c=s->palette[16+color],dim=((c&31)/4)|((((c>>5)&31)/4)<<5)|((((c>>10)&31)/4)<<10);
            CHECK(pixels[(61+y)*240+9+k-origin]==dim);++n;}}
    CHECK(n>0 && ssa_netricsa_pointer(s,12,61,0)==0 && !s->hover && s->category==2);
    CHECK(ssa_netricsa_pointer(s,12,61,1)==0 && !s->hover && s->category==2);
    if(preview){char path[2048];CHECK(snprintf(path,sizeof(path),"%s.disabled.bmp",preview)<(int)sizeof(path));
        CHECK(bitmap(path,pixels));}
    return 0;
}
static int overlay(SsaNetricsa *s)
{
    unsigned w=854,h=480,i,changed=0,errors=0;size_t n=(size_t)w*h;
    uint32_t *back=malloc(n*4),*hud=malloc(n*4),*first=malloc(n*4),*second=malloc(n*4);
    CHECK(back && hud && first && second);
    for(i=0;i<n;++i){back[i]=(i*7919u)&0xffffff;hud[i]=back[i]^0xff0000;}
    back[0]=0x808080;hud[0]=0xff0000;back[w-1]=hud[w-1]=0xffffff; /* Weapon remains visible. */
    s->fade_volume=0xf00;
    CHECK(ssa_netricsa_overlay(s,back,hud,w,h,first)==1 && !memcmp(first,hud,n*4));
    s->fade_volume=0x780;
    CHECK(ssa_netricsa_overlay(s,back,hud,w,h,first)==1);
    CHECK((first[0]>>16)>23 && (first[0]>>16)<255 && ((first[0]>>8)&255)>0 && ((first[0]>>8)&255)<61);
    s->fade_volume=0;
    CHECK(ssa_netricsa_overlay(s,back,hud,w,h,first)==1);
    CHECK(first[0]==0x173d20 && first[w-1]==0x2d7a3f);
    CHECK(ssa_netricsa_overlay(s,back,NULL,w,h,second)==1 && !memcmp(first,second,n*4)); /* HUD fully gone. */
    for(i=1;i<n;++i){changed+=first[i]!=back[i];
        if(i!=w-1)errors+=back[i]!=((i*7919u)&0xffffff) || hud[i]!=(back[i]^0xff0000);}
    CHECK(changed>1000 && !errors);
    s->background_scroll=(s->background_scroll+64)&511;
    CHECK(ssa_netricsa_overlay(s,back,hud,w,h,second)==1 && !memcmp(first,second,n*4));
    memcpy(second,back,n*4);
    CHECK(ssa_netricsa_overlay(s,second,hud,w,h,second)==1 && !memcmp(first,second,n*4));
    memcpy(second,hud,n*4);
    CHECK(ssa_netricsa_overlay(s,back,second,w,h,second)==1 && !memcmp(first,second,n*4));
    CHECK(ssa_netricsa_overlay(s,back,hud,0,h,second)==-1);
    CHECK(ssa_netricsa_overlay(s,NULL,hud,w,h,second)==-1);
    /* Smooth opening/closing endpoints and early-close reversal. */
    s->closing=s->done=0;s->fade_volume=0xf00;s->fade_step=-64;
    for(i=0;i<15;++i){CHECK(ssa_netricsa_pc_step(s,0)==1);CHECK(s->fade_volume==(int)(14-i)*256);}
    CHECK(s->fade_step==0);
    CHECK(ssa_netricsa_pointer(s,136,12,1)==SSA_NET_POINTER_CLOSE && !s->closing);
    CHECK(ssa_netricsa_pc_step(s,SSA_NET_CLOSE)==1 && s->fade_volume==256);
    for(i=1;i<15;++i)CHECK(ssa_netricsa_pc_step(s,0)==(i==14?0:1));
    CHECK(s->done && s->fade_volume==0xf00);
    s->closing=s->done=0;s->fade_volume=0xf00;s->fade_step=-64;
    for(i=0;i<4;++i)CHECK(ssa_netricsa_pc_step(s,0)==1);
    CHECK(ssa_netricsa_pc_step(s,SSA_NET_CLOSE)==1 && s->fade_volume==0xc00);
    for(i=0;i<3;++i)CHECK(ssa_netricsa_pc_step(s,0)==(i==2?0:1));
    CHECK(s->done && s->fade_volume==0xf00);
    free(back);free(hud);free(first);free(second);return 0;
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000),row[1152];uint16_t pixels[38400];SsaNetricsa *s=calloc(1,sizeof(*s));
    SsaNetricsaData d={0};FILE *f;unsigned language,level,i,j,frames=0,runs=0;
    CHECK(argc>=2 && rom && s && (f=fopen(argv[1],"rb")));
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(i=0;i<10;++i){d.rows.weapons[i].owned=1;d.rows.weapons[i].ammo=(int32_t)i*10;
        d.rows.weapons[i].shots=100;d.rows.weapons[i].hits=i*3;}
    for(i=0;i<12;++i){d.rows.enemies[i].spawned=i+1;d.rows.enemies[i].hits=i*2;d.rows.enemies[i].kills=i;}
    d.totals.elapsed=125*60;d.totals.health=75;d.totals.score=123000;d.totals.spawned=78;d.totals.kills=43;
    d.lives=3;d.random=5;
    for(language=0;language<3;++language)for(level=0;level<12;++level){
        memset(s,0,sizeof(*s));d.level=level;
        CHECK(ssa_netricsa_init(s,rom,0x800000,language,&d)==1);
        CHECK(s->category==4 && s->focus==1 && s->image==22 && s->has_enemies);
        CHECK(!memcmp(&s->data,&d,sizeof(d)));
        for(i=0;i<61;++i){CHECK(ssa_netricsa_step(s,i>15?SSA_NET_SCROLL_DOWN:0)==1);++frames;}
        CHECK(s->scroll[0]>0 && s->fade_volume==0);
        for(i=0;i<5;++i){
            int y=73-(int)i*12;
            {int result=ssa_netricsa_pointer(s,12,y,0);
             if(result<0 || s->category!=i)fprintf(stderr,"language%u level%u category%u actual%u result%d\n",language,level,i,s->category,result);
             CHECK(result>=0 && s->category==i);}
            for(j=0;j<(i==1?12:i==2?10:1);++j){unsigned selected=i==1?s->enemy:s->weapon;
                CHECK(ssa_netricsa_step(s,0)==1);
                CHECK(ssa_netricsa_draw(s,pixels,38400)==1);
                if(i==1)CHECK(s->image==10+selected && s->title[0]==102+selected);
                if(i==2)CHECK(s->image==selected && s->title[0]==74+selected);
                CHECK(ssa_netricsa_step(s,SSA_NET_NEXT)==1);++frames;
            }
            if(i==1 || i==2){unsigned count=i==1?12:10,stride=8;
                for(j=0;j<count;++j){unsigned *selected=i==1?&s->enemy:&s->weapon;
                    s->scroll[0]=12;s->scroll[1]=16;s->delta[0]=1;s->delta[1]=-1;
                    CHECK(ssa_netricsa_pointer(s,90+(int)(j*stride+stride/2),21,0)==0);
                    CHECK(ssa_netricsa_pointer(s,90+(int)(j*stride+stride/2),21,1)>=0 && *selected==j);
                    if(j)CHECK(!s->scroll[0] && !s->scroll[1] && !s->delta[0] && !s->delta[1]);
                    CHECK(s->image==(i==1?10:0)+j);
                }
                CHECK(ssa_netricsa_pointer(s,90+(int)stride/2,21,1)>=0);
            }
            if(i==2){
                if(!language && !level)CHECK(selector_style(s,pixels,argc==3?argv[2]:NULL)==0);
                CHECK(ssa_netricsa_pointer(s,70,110,1)==1 && s->weapon==1);
                CHECK(ssa_netricsa_pointer(s,30,110,1)==1 && s->weapon==0);
                CHECK(ssa_netricsa_wheel(s,-2)==1 && s->weapon==2);
                CHECK(ssa_netricsa_wheel(s,2)==1 && s->weapon==0);
            }
            CHECK(ssa_netricsa_pointer(s,130,120,1)>=0 && s->focus==2);
            CHECK(ssa_netricsa_wheel(s,-100)==1);
            CHECK(ssa_netricsa_wheel(s,100)==1 && s->scroll[1]==0);
        }
        CHECK(ssa_netricsa_pointer(s,12,49,1)>=0);
        if(!language && !level && argc==3){CHECK(ssa_netricsa_draw(s,pixels,38400)==1);CHECK(bitmap(argv[2],pixels));}
        CHECK(ssa_netricsa_pointer(s,136,8,1)==0 && !s->closing);
        CHECK(ssa_netricsa_pointer(s,133,12,1)==0 && !s->closing);
        CHECK(ssa_netricsa_pointer(s,136,12,0)==0 && !s->closing);
        s->counter&=~32u;
        CHECK(ssa_netricsa_pointer(s,136,12,1)==SSA_NET_POINTER_CLOSE && !s->closing);
        s->counter|=32u;
        CHECK(ssa_netricsa_pointer(s,136,12,1)==SSA_NET_POINTER_CLOSE && !s->closing);
        CHECK(ssa_netricsa_step(s,SSA_NET_CLOSE)==1 && s->closing);
        CHECK(ssa_netricsa_pointer(s,136,12,1)==0);
        for(i=0;i<61 && !s->done;++i){CHECK(ssa_netricsa_step(s,0)>=0);++frames;}
        CHECK(s->done && s->fade_volume==0xf00);++runs;
    }
    memset(s,0,sizeof(*s));memset(&d,0,sizeof(d));d.rows.weapons[1].owned=1;
    CHECK(ssa_netricsa_init(s,rom,0x800000,0,&d)==1 && !s->has_enemies);
    CHECK(ssa_netricsa_pointer(s,12,61,1)==0 && s->category==4 && !s->hover);
    CHECK(ssa_netricsa_pointer(s,12,49,1)>=0 && s->category==2);
    CHECK(ssa_netricsa_step(s,SSA_NET_NEXT)==1 && s->weapon==1);
    CHECK(ssa_netricsa_pointer(s,92,20,1)==0 && s->weapon==1 && !s->hover); /* Unowned chainsaw. */
    CHECK(ssa_netricsa_pointer(s,100,20,1)==0 && s->weapon==1 && s->hover==11);
    CHECK(disabled_records(s,pixels,argc==3?argv[2]:NULL)==0);
    CHECK(strstr(s->text[1],"Accuracy - 0%")!=NULL);
    CHECK(ssa_netricsa_row(s,"*1ABC\nXYZ",1,18,row,sizeof(row))==1);
    CHECK(ssa_netricsa_row(s,"ABC",4,18,row,sizeof(row))==1);
    for(i=0;i<sizeof(row);++i)CHECK(!row[i]);
    CHECK(ssa_netricsa_row(s,"ABC",0,33,row,sizeof(row))==-1);
    CHECK(ssa_netricsa_row(s,"*X",0,18,row,sizeof(row))==-1);
    CHECK(ssa_netricsa_row(s,"ABC",0,18,row,1)==-1);
    CHECK(ssa_netricsa_pointer(s,-10,-10,1)==0);
    memset(s,0,sizeof(*s));d.rows.enemies[3].spawned=1;
    CHECK(ssa_netricsa_init(s,rom,0x800000,0,&d)==1);
    CHECK(ssa_netricsa_pointer(s,12,61,1)>=0 && s->enemy==3);
    CHECK(ssa_netricsa_pointer(s,92,20,1)==0 && s->enemy==3 && !s->hover);
    s->data.show_all=1;
    CHECK(ssa_netricsa_pointer(s,180,20,1)==1 && s->enemy==11);
    CHECK(overlay(s)==0);
    memset(s,0,sizeof(*s));memset(d.rows.enemies,0,sizeof(d.rows.enemies));d.rows.enemies[11].spawned=1;
    CHECK(ssa_netricsa_init(s,rom,0x800000,0,&d)==1 && s->has_enemies && s->enemy==11);
    CHECK(ssa_netricsa_pointer(s,12,61,1)==1 && s->category==1 && s->image==21);
    printf("{\"result\":\"pass\",\"checks\":%u,\"runs\":%u,\"frames\":%u,\"languages\":3,\"levels\":12}\n",checks,runs,frames);
    free(s);free(rom);return 0;
}
