#include "ssa/campaign_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"story screen line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void word(FILE *f,unsigned n,unsigned bytes){while(bytes--){fputc(n&255,f);n>>=8;}}
static int bitmap(const char *prefix,unsigned level,const uint16_t *pixels)
{
    char path[512];FILE *f;unsigned i;
    if(snprintf(path,sizeof(path),"%s-%u.bmp",prefix,level)>=(int)sizeof(path) || !(f=fopen(path,"wb")))return 0;
    fputs("BM",f);word(f,54+38400*4,4);word(f,0,4);word(f,54,4);word(f,40,4);word(f,240,4);
    word(f,(unsigned)-160,4);word(f,1,2);word(f,32,2);word(f,0,4);word(f,38400*4,4);
    word(f,0,4);word(f,0,4);word(f,0,4);word(f,0,4);
    for(i=0;i<38400;++i){unsigned c=pixels[i];
        word(f,(((c&31)*255/31)<<16)|((((c>>5)&31)*255/31)<<8)|(((c>>10)&31)*255/31),4);}
    return fclose(f)==0;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);SsaCampaignScreen *s=calloc(1,sizeof(*s));
    uint16_t pixels[38400];unsigned language,level,mode,frames=0,commands=0,fades=0;FILE *f;
    CHECK(argc>=2 && argc<=3 && rom && s && (f=fopen(argv[1],"rb")));
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(language=0;language<3;++language)for(level=0;level<=12;level+=6)for(mode=0;mode<3;++mode){
        unsigned tick;
        memset(s,0,sizeof(*s));CHECK(ssa_campaign_screen_init(s,rom,0x800000,level,language,1)==1);
        CHECK(s->command_count==1 && s->commands[0]==11);
        for(tick=0;tick<10000 && !s->done;++tick){uint32_t input=mode==1?(tick==160?0x10000:0):mode==2?0x80:0;
            int result=ssa_campaign_screen_step(s,input);CHECK(result>=0 && !s->updating);
            CHECK(result==(s->done?0:1));
            commands+=s->command_count;fades+=s->fade_count;++frames;
            if(tick%64==0 || tick==160)CHECK(ssa_campaign_screen_draw(s,pixels,38400)==1);
            if(argc==3 && language==0 && mode==0 && tick==160)CHECK(bitmap(argv[2],level,pixels));
        }
        CHECK(s->done && s->story.mode==2 && !s->fading && s->fade_volume==0xf00);
        CHECK(ssa_campaign_screen_step(s,0)==0);
    }
    printf("{\"result\":\"pass\",\"checks\":%u,\"screens\":27,\"frames\":%u,\"commands\":%u,\"fades\":%u}\n",checks,frames,commands,fades);
    free(s);free(rom);return 0;
}
