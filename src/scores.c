#include "ssa/scores.h"
#include <string.h>
static uint32_t read32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t x){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(x>>(i*8));}
static uint32_t crc(const uint8_t *b,size_t n){uint32_t c=UINT32_MAX;size_t i;unsigned j;
    for(i=0;i<n;++i){c^=b[i];for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;
}
int ssa_scores_valid(const SsaScores *s){unsigned i,j;
    if(!s || s->name[12])return 0;
    for(i=0;i<18;++i){
        if(i && s->entries[i-1].score<s->entries[i].score)return 0;
        for(j=0;j<12;++j)if((unsigned char)s->entries[i].name[j]<32 || (unsigned char)s->entries[i].name[j]>126)return 0;
    }
    for(j=0;j<12;++j)if(!((s->name[j]>='A' && s->name[j]<='Z') || (s->name[j]>='0' && s->name[j]<='9') || s->name[j]=='_' || s->name[j]==' '))return 0;
    return 1;
}
int ssa_scores_init(SsaScores *s,const uint8_t *rom,size_t size){SsaScores next;unsigned i;
    if(!s || !rom || size!=0x800000)return -1;
    memset(&next,0,sizeof next);memset(next.name,'_',12);
    for(i=0;i<18;++i){uint32_t at=read32(rom+0x44c18c+i*4);
        if(at<0x08000000 || at-0x08000000>size-12)return -1;
        next.entries[i].score=read32(rom+0x44c144+i*4);
        memcpy(next.entries[i].name,rom+at-0x08000000,12);
    }
    if(!ssa_scores_valid(&next))return -1;
    *s=next;return 1;
}
int ssa_scores_rank(const SsaScores *s,uint32_t score){unsigned rank=0;
    if(!ssa_scores_valid(s))return -1;
    while(rank<18 && s->entries[rank].score>score)++rank;
    return (int)rank;
}
int ssa_scores_insert(SsaScores *s,uint32_t score,const char name[12]){int rank;unsigned i;char copy[12];
    if(!name || (rank=ssa_scores_rank(s,score))<0)return -1;
    for(i=0;i<12;++i){char c=name[i];if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c==' '))return -1;copy[i]=c;}
    if(rank==18)return rank;
    memmove(s->entries+rank+1,s->entries+rank,(17-(unsigned)rank)*sizeof(*s->entries));
    s->entries[rank].score=score;
    for(i=0;i<12;++i){s->name[i]=copy[i];s->entries[rank].name[i]=copy[i]=='_'?' ':copy[i];}
    return rank;
}
int ssa_scores_encode(const SsaScores *s,uint8_t *b,size_t n){unsigned i;
    if(!ssa_scores_valid(s) || !b || n!=SSA_SCORES_BYTES)return -1;
    memset(b,0,n);memcpy(b,"SSA-SCOR",8);b[8]=1;
    for(i=0;i<18;++i){write32(b+16+i*16,s->entries[i].score);memcpy(b+20+i*16,s->entries[i].name,12);}
    memcpy(b+304,s->name,12);write32(b+316,crc(b,316));return 1;
}
int ssa_scores_decode(SsaScores *s,const uint8_t *b,size_t n){SsaScores next;unsigned i;
    if(!s || !b || n!=SSA_SCORES_BYTES || memcmp(b,"SSA-SCOR",8) || b[8]!=1 || read32(b+316)!=crc(b,316))return -1;
    for(i=9;i<16;++i)if(b[i])return -1;
    memset(&next,0,sizeof next);
    for(i=0;i<18;++i){next.entries[i].score=read32(b+16+i*16);memcpy(next.entries[i].name,b+20+i*16,12);}
    memcpy(next.name,b+304,12);
    if(!ssa_scores_valid(&next))return -1;
    *s=next;return 1;
}
