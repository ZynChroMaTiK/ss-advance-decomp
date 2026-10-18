#include "ssa/settings.h"
#include <string.h>
static uint32_t crc(const uint8_t *b){uint32_t c=UINT32_MAX;unsigned i,j;
    for(i=0;i<12;++i){c^=b[i];for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;
}
static int valid(const SsaSettings *s){return s && s->music_style<SSA_MUSIC_STYLES && s->music_volume<=100 && s->sound_volume<=100 && s->renderer<=SSA_RENDER_GBA && s->jump_enabled<=1 && s->ingame_netricsa<=1 && s->thirdperson_enabled<=1 && s->display_mode<=SSA_DISPLAY_WINDOWED && s->alternate_weapons<=1;}
int ssa_settings_encode(const SsaSettings *s,uint8_t *b,size_t n){uint32_t c;unsigned i;
    if(!valid(s) || !b || n!=SSA_SETTINGS_BYTES)return -1;
    memcpy(b,"SSA-OPT",8);b[8]=6;b[9]=(uint8_t)(s->music_style|(s->renderer<<2)|(s->jump_enabled<<3)|(s->ingame_netricsa<<4)|(s->thirdperson_enabled<<5)|(s->display_mode<<6)|(s->alternate_weapons<<7));b[10]=(uint8_t)s->music_volume;b[11]=(uint8_t)s->sound_volume;
    c=crc(b);for(i=0;i<4;++i)b[12+i]=(uint8_t)(c>>(8*i));return 1;
}
int ssa_settings_decode(SsaSettings *s,const uint8_t *b,size_t n){uint32_t c=0;unsigned i;SsaSettings next;
    if(!s || !b || n!=SSA_SETTINGS_BYTES || memcmp(b,"SSA-OPT",8) || b[8]<1 || b[8]>6)return -1;
    for(i=0;i<4;++i)c|=(uint32_t)b[12+i]<<(8*i);
    if(c!=crc(b))return -1;
    next=(SsaSettings)SSA_SETTINGS_DEFAULT;
    if(b[8]==1){if(b[9]>=SSA_MUSIC_STYLES)return -1;next.music_style=b[9];}
    else{if(b[9]&(b[8]==2?0xf0:b[8]==3?0xe0:b[8]==4?0xc0:b[8]==5?0x80:0))return -1;next.music_style=b[9]&3;next.renderer=(b[9]>>2)&1;next.jump_enabled=(b[9]>>3)&1;
        if(b[8]>=3)next.ingame_netricsa=(b[9]>>4)&1;
        if(b[8]>=4)next.thirdperson_enabled=(b[9]>>5)&1;
        if(b[8]>=5)next.display_mode=(b[9]>>6)&1;
        if(b[8]>=6)next.alternate_weapons=(b[9]>>7)&1;}
    next.music_volume=b[10];next.sound_volume=b[11];
    if(!valid(&next))return -1;
    *s=next;return 1;
}
