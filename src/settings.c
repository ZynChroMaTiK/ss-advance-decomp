#include "ssa/settings.h"
#include <string.h>
static uint32_t crc(const uint8_t *b){uint32_t c=UINT32_MAX;unsigned i,j;
    for(i=0;i<12;++i){c^=b[i];for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;
}
int ssa_settings_valid(const SsaSettings *s){
    if(!s)return 0;
#define CHECK_FIELD(name,max) if(s->name>(max))return 0;
    SSA_SETTINGS_FIELDS(CHECK_FIELD)
#undef CHECK_FIELD
    return 1;
}
int ssa_settings_equal(const SsaSettings *a,const SsaSettings *b){
    if(!a || !b)return 0;
#define COMPARE_FIELD(name,max) if(a->name!=b->name)return 0;
    SSA_SETTINGS_FIELDS(COMPARE_FIELD)
#undef COMPARE_FIELD
    return 1;
}
int ssa_settings_encode(const SsaSettings *s,uint8_t *b,size_t n){uint32_t c;unsigned i;
    if(!ssa_settings_valid(s) || !b || n!=SSA_SETTINGS_BYTES)return -1;
    memcpy(b,"SSA-OPT",7);b[7]=(uint8_t)(s->vertical_aim|(s->fov_offset<<1)|((s->renderer>>1)<<7));b[8]=(uint8_t)(15|(s->water_reflections<<4)|(s->surface_decals<<5)|(s->weapon_bobbing<<6)|(s->aspect_ratio<<7));b[9]=(uint8_t)(s->music_style|((s->renderer&1)<<2)|(s->jump_enabled<<3)|(s->ingame_netricsa<<4)|(s->thirdperson_enabled<<5)|(s->display_mode<<6)|(s->alternate_weapons<<7));
    /* Version15 uses byte7 bit7 for the renderer's second bit. Earlier
     * software/GBA values, all other choices, size and CRC stay intact. */
    b[10]=(uint8_t)(s->music_volume|(s->automap_enabled<<7));b[11]=(uint8_t)(s->sound_volume|(s->hud_style<<7));
    c=crc(b);for(i=0;i<4;++i)b[12+i]=(uint8_t)(c>>(8*i));return 1;
}
int ssa_settings_decode(SsaSettings *s,const uint8_t *b,size_t n){uint32_t c=0;unsigned i,version;SsaSettings next;
    if(!s || !b || n!=SSA_SETTINGS_BYTES || memcmp(b,"SSA-OPT",7))return -1;
    version=b[8]&15;
    if(version<1 || version>15 || (b[7] & (version>=15?0:version>=14?128:version>=13?254:255)) || (version<12 && (b[8]&16)) || (version<9 && (b[8]&128)) || (version<10 && (b[8]&64)) || (version<11 && (b[8]&32)))return -1;
    for(i=0;i<4;++i)c|=(uint32_t)b[12+i]<<(8*i);
    if(c!=crc(b))return -1;
    next=(SsaSettings)SSA_SETTINGS_DEFAULT;
    /* Version1 predates renderer selection and used Software. */
    if(version==1){if(b[9]>=SSA_MUSIC_STYLES)return -1;next.music_style=b[9];next.renderer=SSA_RENDER_SOFTWARE;}
    else{if(b[9]&(version==2?0xf0:version==3?0xe0:version==4?0xc0:version==5?0x80:0))return -1;next.music_style=b[9]&3;next.renderer=(b[9]>>2)&1;next.jump_enabled=(b[9]>>3)&1;
        if(version>=3)next.ingame_netricsa=(b[9]>>4)&1;
        if(version>=4)next.thirdperson_enabled=(b[9]>>5)&1;
        if(version>=5)next.display_mode=(b[9]>>6)&1;
        if(version>=6)next.alternate_weapons=(b[9]>>7)&1;}
    next.music_volume=version>=7?b[10]&127:b[10];next.sound_volume=version>=8?b[11]&127:b[11];
    if(version>=7)next.automap_enabled=b[10]>>7;
    if(version>=8)next.hud_style=b[11]>>7;
    if(version>=9)next.aspect_ratio=b[8]>>7;
    if(version>=10)next.weapon_bobbing=(b[8]>>6)&1;
    if(version>=11)next.surface_decals=(b[8]>>5)&1;
    if(version>=12)next.water_reflections=(b[8]>>4)&1;
    if(version>=13)next.vertical_aim=b[7]&1;
    if(version>=14)next.fov_offset=(b[7]>>1)&63;
    if(version>=15)next.renderer|=(b[7]>>7)<<1;
    if(!ssa_settings_valid(&next))return -1;
    *s=next;return 1;
}
