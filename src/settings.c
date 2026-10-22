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
    /* v22 adds byte11 bit4 for presentation VSync; the16-byte CRC layout
     * and all prior fields remain intact. Older records inherit default VSync. */
    memcpy(b,"SSA",3);b[3]=22;
    b[4]=(uint8_t)(s->music_style|(s->renderer<<2)|(s->display_mode<<4)|(s->hud_style<<5)|(s->aspect_ratio<<6));
    b[5]=(uint8_t)(s->music_volume|(s->automap_enabled<<7));
    b[6]=(uint8_t)(s->sound_volume|(s->jump_enabled<<7));
    b[7]=(uint8_t)(s->fov_offset|(s->vertical_aim<<6)|(s->thirdperson_enabled<<7));
    /* Keep both v18 axis bytes equal for compatibility. Existing unequal
     * v18 values migrate to the horizontal setting when decoded. */
    b[8]=b[9]=(uint8_t)s->mouse_sensitivity;
    b[10]=(uint8_t)(s->ingame_netricsa|(s->alternate_weapons<<1)|(s->weapon_bobbing<<2)|
        (s->surface_decals<<3)|(s->water_reflections<<4)|(s->mipmaps<<5)|(s->godrays<<6)|(s->sprite_shadows<<7));
    b[11]=(uint8_t)(s->vulkan_antialiasing|(s->vulkan_anisotropic<<1)|(s->vulkan_ambient_occlusion<<2)|(s->vulkan_motion_blur<<3)|(s->vsync<<4));
    c=crc(b);for(i=0;i<4;++i)b[12+i]=(uint8_t)(c>>(8*i));return 1;
}
int ssa_settings_decode(SsaSettings *s,const uint8_t *b,size_t n){uint32_t c=0;unsigned i,version;SsaSettings next;
    if(!s || !b || n!=SSA_SETTINGS_BYTES)return -1;
    if(!memcmp(b,"SSA",3) && (b[3]>=18 && b[3]<=22)){
        if((b[4]&128) || (b[3]==18 && (b[10]&128)) || (b[11] & (b[3]>=22?224:b[3]>=21?240:b[3]>=20?248:255)) || b[9]>SSA_MOUSE_RANGE)return -1;
        for(i=0;i<4;++i)c|=(uint32_t)b[12+i]<<(8*i);
        if(c!=crc(b))return -1;
        next=(SsaSettings)SSA_SETTINGS_DEFAULT;
        next.music_style=b[4]&3;next.renderer=(b[4]>>2)&3;
        next.display_mode=(b[4]>>4)&1;next.hud_style=(b[4]>>5)&1;next.aspect_ratio=(b[4]>>6)&1;
        next.music_volume=b[5]&127;next.automap_enabled=b[5]>>7;
        next.sound_volume=b[6]&127;next.jump_enabled=b[6]>>7;
        next.fov_offset=b[7]&63;next.vertical_aim=(b[7]>>6)&1;next.thirdperson_enabled=b[7]>>7;
        next.mouse_sensitivity=b[8];
        next.ingame_netricsa=b[10]&1;next.alternate_weapons=(b[10]>>1)&1;next.weapon_bobbing=(b[10]>>2)&1;
        next.surface_decals=(b[10]>>3)&1;next.water_reflections=(b[10]>>4)&1;
        next.mipmaps=(b[10]>>5)&1;next.godrays=(b[10]>>6)&1;next.sprite_shadows=b[3]>=19?b[10]>>7:0;
        if(b[3]>=20){next.vulkan_antialiasing=b[11]&1;next.vulkan_anisotropic=(b[11]>>1)&1;
            next.vulkan_ambient_occlusion=(b[11]>>2)&1;}
        if(b[3]>=21)next.vulkan_motion_blur=(b[11]>>3)&1;
        if(b[3]>=22)next.vsync=(b[11]>>4)&1;
        if(!ssa_settings_valid(&next))return -1;
        *s=next;return 1;
    }
    if((b[0]&127)!='S' || memcmp(b+1,"SA-O",4) || (b[5]&127)!='P' || (b[6]&127)!='T')return -1;
    version=b[8]&15;if(b[0]&128){if(!version)return -1;version+=16;}else if(!version)version=16;
    if(version<1 || version>17 || (version<17 && b[5]!='P') || (version<16 && b[6]!='T') || (b[7] & (version>=15?0:version>=14?128:version>=13?254:255)) || (version<12 && (b[8]&16)) || (version<9 && (b[8]&128)) || (version<10 && (b[8]&64)) || (version<11 && (b[8]&32)))return -1;
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
    next.mipmaps=version>=16?b[6]>>7:0; /* Preserve prior native appearance. */
    next.godrays=version>=17?b[5]>>7:next.surface_decals; /* Retain the former coupled effect. */
    if(!ssa_settings_valid(&next))return -1;
    *s=next;return 1;
}
