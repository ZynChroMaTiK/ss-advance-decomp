/* Test-only original-ROM timing oracle; never linked to the native port. */
#include <mgba/core/core.h>
#include <mgba/core/timing.h>
#include <mgba/core/log.h>
#include <stdio.h>
#include <stdlib.h>

static void log_errors(struct mLogger *logger,int category,enum mLogLevel level,const char *format,va_list args){
    (void)logger;(void)category;if(level&(mLOG_FATAL|mLOG_ERROR)){vfprintf(stderr,format,args);fputc('\n',stderr);}}
int main(int argc,char **argv){
    struct mCore *c;unsigned i,state,previous=~0u,frames=0,last=0,started=0;
    unsigned weapon=argc>2?(unsigned)atoi(argv[2]):4,ammo=300;uint32_t player=0;struct mLogger logger={log_errors,NULL};
    color_t *pixels=calloc(256*256,sizeof(*pixels));
    if(argc<2 || (weapon!=1 && weapon!=4 && weapon!=5) || !pixels)return 2;
    mLogSetDefaultLogger(&logger);
    c=mCoreFind(argv[1]);if(!c || !c->init(c))return 2;
    mCoreConfigInit(&c->config,"ssa-timing");
    c->opts.useBios=false;c->opts.skipBios=true;c->opts.audioSync=false;c->opts.videoSync=false;
    c->setVideoBuffer(c,pixels,256);
    if(!mCoreLoadFile(c,argv[1]))return 2;
    c->reset(c);
    for(i=0;i<7200;++i){
        state=c->busRead16(c,0x03004304);
        if(state!=previous){fprintf(stderr,"vblank%u state%u\n",i,state);previous=state;}
        c->setKeys(c,started?1:(i%120<30?9:0));
        c->runFrame(c);
        if(state==1){uint32_t game=c->busRead32(c,0x03004010);unsigned n=c->busRead32(c,game);
            if(!started){uint32_t object;started=i;last=n;player=c->busRead32(c,0x03004648);
                object=c->busRead32(c,player+44);
                c->busWrite32(c,player+40,weapon);c->busWrite16(c,player+120,(uint16_t)weapon);
                c->busWrite32(c,player+48,0);c->busWrite16(c,player+52,0x3ff);
                c->busWrite16(c,player+54+2*(weapon-1),300);
                c->busWrite32(c,object+16,0x5000);c->busWrite16(c,object+32,(uint16_t)(weapon-1));
                c->busWrite16(c,object+34,0);c->busWrite8(c,object+36,3);c->busWrite8(c,object+37,0);c->busWrite8(c,object+38,0);
                fprintf(stderr,"game start%u game%x player%x object%x weapon%u\n",i,game,player,object,weapon);}
            if(n!=last){printf("{\"vblank\":%u,\"frame\":%u,\"ticks\":%u,\"ammo\":%u,\"shots\":%u}\n",
                i-started,n,c->busRead8(c,game+7),c->busRead16(c,player+54+2*(weapon-1)),c->busRead32(c,game+0x98+16*(weapon-1)));last=n;++frames;}
            if(c->busRead16(c,player+54+2*(weapon-1))!=ammo){ammo=c->busRead16(c,player+54+2*(weapon-1));
                fprintf(stderr,"shot vblank%u ammo%u\n",i-started,ammo);}
            if(i-started>=600)break;
        }
    }
    fprintf(stderr,"observed%u frames in%u vblanks; hardware clock%dHz/%dcycles\n",
        frames,i-started,c->frequency(c),c->frameCycles(c));
    mCoreConfigDeinit(&c->config);c->deinit(c);free(pixels);return started?0:1;
}
