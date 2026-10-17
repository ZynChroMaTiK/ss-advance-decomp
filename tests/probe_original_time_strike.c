/* Test-only hardware-refresh timing trace; never linked to the native game. */
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <stdio.h>
#include <stdlib.h>

static void log_errors(struct mLogger *logger,int category,enum mLogLevel level,const char *format,va_list args)
{(void)logger;(void)category;if(level&(mLOG_FATAL|mLOG_ERROR)){vfprintf(stderr,format,args);fputc('\n',stderr);}}
int main(int argc,char **argv)
{
    struct mCore *c;struct mLogger logger={log_errors,NULL};color_t *pixels=calloc(256*256,sizeof(*pixels));
    unsigned i,booted=0,started=0,changes=0,last_state=0;uint32_t effect=0,last_z=0;
    if(argc!=2 || !pixels)return 2;
    mLogSetDefaultLogger(&logger);c=mCoreFind(argv[1]);if(!c || !c->init(c))return 2;
    mCoreConfigInit(&c->config,"ssa-time-strike");c->opts.useBios=false;c->opts.skipBios=true;
    c->opts.audioSync=false;c->opts.videoSync=false;c->setVideoBuffer(c,pixels,256);
    if(!mCoreLoadFile(c,argv[1]))return 2;
    c->reset(c);
    for(i=0;i<7200;++i){
        unsigned state=c->busRead16(c,0x03004304);
        c->setKeys(c,booted?0:(i%120<30?9:0));c->runFrame(c);
        if(state==1){uint32_t game=c->busRead32(c,0x03004010);
            if(!booted)booted=i;
            if(i-booted<180)continue;
            if(!started){
                uint32_t base=c->busRead32(c,0x03004644),player=c->busRead32(c,0x03004648);
                uint32_t object=c->busRead32(c,player+44);unsigned j;
                /* Replace one already linked pickup in place, retaining its
                 * original XY/floor/list links. No ROM instruction is patched.
                 * Use the actual weapon9 artwork, with a CD-height520-unit drop. */
                for(j=0;j<100;++j){uint32_t e=base+j*116;
                    if(c->busRead16(c,e+60)==0xe1){effect=e;break;}}
                if(!effect)return 3;
                c->busWrite32(c,player+40,9);c->busWrite16(c,player+120,9);
                c->busWrite32(c,player+48,0);c->busWrite16(c,player+52,0x3ff);
                c->busWrite32(c,object+16,0x5000);c->busWrite16(c,object+32,8);
                c->busWrite16(c,object+34,0);c->busWrite8(c,object+36,3);
                c->busWrite8(c,object+37,0);c->busWrite8(c,object+38,0);
                c->busWrite16(c,effect+60,0xe3);c->busWrite8(c,effect+62,0);
                c->busWrite16(c,effect+48,0x20);c->busWrite32(c,effect+44,0);
                c->busWrite32(c,effect+40,128*65536);c->busWrite32(c,effect+32,0);
                c->busWrite8(c,effect+84,0);c->busWrite8(c,effect+85,9);
                c->busWrite32(c,effect+12,0);c->busWrite32(c,effect+16,0);c->busWrite32(c,effect+20,0);
                c->busWrite32(c,effect+8,(uint32_t)((int16_t)c->busRead16(c,effect+114)+520)*65536u);
                c->busWrite16(c,effect+88,0x60);c->busWrite16(c,effect+92,0);
                c->busWrite8(c,effect+94,5);c->busWrite8(c,effect+95,0);
                started=i;last_z=c->busRead32(c,effect+8);
                printf("{\"vblank\":0,\"frame\":%u,\"ticks\":%u,\"z\":%d,\"vz\":0,\"state\":0,\"type\":227}\n",
                    c->busRead32(c,game),c->busRead8(c,game+7),(int32_t)last_z);
            }else{
                uint32_t z=c->busRead32(c,effect+8);unsigned phase=c->busRead8(c,effect+62),type=c->busRead16(c,effect+60);
                if(z!=last_z || phase!=last_state || type!=0xe3){
                    printf("{\"vblank\":%u,\"frame\":%u,\"ticks\":%u,\"z\":%d,\"vz\":%d,\"state\":%u,\"type\":%u}\n",
                        i-started,c->busRead32(c,game),c->busRead8(c,game+7),(int32_t)z,(int32_t)c->busRead32(c,effect+20),phase,type);
                    last_z=z;last_state=phase;++changes;
                }
                if(type!=0xe3)break;
                if(i-started>300)return 4;
            }
        }
    }
    mCoreConfigDeinit(&c->config);c->deinit(c);free(pixels);return started && changes>2?0:1;
}
