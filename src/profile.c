#include "ssa/profile.h"
#include <string.h>
static int valid(const SsaProfile *p){return p && p->language<3 && !(p->completed&0xf000);}
static uint32_t crc(const uint8_t *p,size_t n){
 uint32_t c=UINT32_MAX;size_t i;unsigned j;
 for(i=0;i<n;++i){c^=p[i];for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}
 return ~c;
}
void ssa_profile_reset(SsaProfile *p){if(p){p->language=0;p->completed=0;}}
int ssa_profile_unlocked(const SsaProfile *p,unsigned level){
 if(!valid(p) || level>=SSA_PROFILE_LEVELS)return -1;
 return !level || (p->completed&(1u<<(level-1)))!=0;
}
int ssa_profile_complete(SsaProfile *p,unsigned level){
 if(!valid(p) || level>=SSA_PROFILE_LEVELS)return -1;
 if(p->completed&(1u<<level))return 0;
 p->completed|=(uint16_t)(1u<<level);return 1;
}
int ssa_profile_encode(const SsaProfile *p,uint8_t *out,size_t n){
 uint8_t b[SSA_PROFILE_BYTES]={0};uint32_t sum;unsigned i;
 if(!valid(p) || !out || n!=sizeof b)return -1;
 memcpy(b,"SSA-SAVE",8);b[8]=1;b[9]=(uint8_t)p->language;b[10]=(uint8_t)p->completed;b[11]=(uint8_t)(p->completed>>8);
 sum=crc(b,12);for(i=0;i<4;++i)b[12+i]=(uint8_t)(sum>>(8*i));memcpy(out,b,sizeof b);return 1;
}
int ssa_profile_decode(SsaProfile *p,const uint8_t *b,size_t n){
 SsaProfile next;uint32_t sum=0;unsigned i;
 if(!p || !b || n!=SSA_PROFILE_BYTES || memcmp(b,"SSA-SAVE",8) || b[8]!=1)return -1;
 for(i=0;i<4;++i)sum|=(uint32_t)b[12+i]<<(8*i);
 if(sum!=crc(b,12))return -1;
 next.language=b[9];next.completed=(uint16_t)(b[10]|(unsigned)b[11]<<8);
 if(!valid(&next))return -1;
 *p=next;return 1;
}
