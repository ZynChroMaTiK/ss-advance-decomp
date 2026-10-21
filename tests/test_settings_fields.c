#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_legacy.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"settings line%d: %s\n",__LINE__,#x);exit(1); } } while(0)
static void checksum(uint8_t *b){
    uint32_t c=UINT32_MAX;unsigned i,j;
    for(i=0;i<12;++i){c^=b[i];for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}
    c=~c;for(i=0;i<4;++i)b[12+i]=(uint8_t)(c>>(8*i));
}
int main(void)
{
    SsaSettings base=SSA_SETTINGS_DEFAULT,next,decoded;
    uint8_t bytes[SSA_SETTINGS_BYTES],encoded[SSA_SETTINGS_BYTES];
    CHECK(ssa_settings_valid(&base));CHECK(ssa_settings_equal(&base,&base));
    CHECK(base.renderer==SSA_RENDER_VULKAN);
    CHECK(!ssa_settings_valid(NULL));CHECK(!ssa_settings_equal(&base,NULL));
    CHECK(ssa_settings_encode(&base,bytes,sizeof bytes)==1);
#define CHECK_FIELD(name,max) \
    next=base;next.name=base.name?0:(max); \
    CHECK(ssa_settings_valid(&next));CHECK(!ssa_settings_equal(&base,&next)); \
    CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==1); \
    CHECK(memcmp(bytes,encoded,sizeof bytes)); \
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1); \
    CHECK(ssa_settings_equal(&next,&decoded)); \
    next.name=(max)+1;CHECK(!ssa_settings_valid(&next)); \
    CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==-1);
    SSA_SETTINGS_FIELDS(CHECK_FIELD)
#undef CHECK_FIELD
    CHECK(base.vertical_aim && base.mipmaps && !base.godrays);
    CHECK(bytes[3]==19 && base.mouse_sensitivity==9);
    for(unsigned x=0;x<=SSA_MOUSE_RANGE;++x){
        next=base;next.mouse_sensitivity=x;
        CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==1);
        CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && ssa_settings_equal(&next,&decoded));
    }
    CHECK(!base.sprite_shadows);
    memcpy(encoded,bytes,sizeof bytes);encoded[3]=18;checksum(encoded);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && !decoded.sprite_shadows);
    /* Existing two-axis v18 saves retain horizontal speed for both axes. */
    memcpy(encoded,bytes,sizeof bytes);encoded[3]=18;encoded[8]=20;encoded[9]=3;checksum(encoded);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && decoded.mouse_sensitivity==20);
    CHECK(ssa_settings_encode(&decoded,encoded,sizeof encoded)==1 && encoded[8]==20 && encoded[9]==20);
    /* v17 migrates mouse defaults and retains every existing preference. */
    next=base;next.godrays=1;next.renderer=SSA_RENDER_GBA;next.music_volume=43;next.fov_offset=60;
    CHECK(settings_legacy(&next,encoded)==1);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && ssa_settings_equal(&next,&decoded));
    /* Reject unsupported versions, reserved bits and invalid ranges, even with a valid CRC. */
    for(unsigned i=0;i<5;++i){memcpy(encoded,bytes,sizeof bytes);
        if(!i)encoded[3]=20;else if(i==1)encoded[11]=1;else if(i==2)encoded[4]|=128;
        else if(i==3)encoded[8]=30;else encoded[9]=255;
        checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    }
    /* Existing v12 settings retain every choice; the new extra defaults On. */
    next=base;next.mipmaps=0;next.renderer=SSA_RENDER_SOFTWARE;next.godrays=next.water_reflections=next.weapon_bobbing=next.surface_decals=1;
    next.music_volume=71;next.sound_volume=63;next.aspect_ratio=SSA_ASPECT_GBA;
    CHECK(settings_legacy(&next,encoded)==1);
    encoded[0]='S';encoded[5]='P';encoded[6]='T';encoded[7]=0;encoded[8]=(uint8_t)((encoded[8]&240)|12);checksum(encoded);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1);
    CHECK(ssa_settings_equal(&next,&decoded) && decoded.vertical_aim);
    encoded[7]=1;checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    encoded[8]=(uint8_t)((encoded[8]&240)|13);checksum(encoded);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && decoded.vertical_aim);
    encoded[7]=2;checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    /* Version13 stores the explicit aiming choice but has no FOV field. */
    encoded[7]=0;checksum(encoded);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && !decoded.vertical_aim && decoded.fov_offset==10);
    encoded[8]=(uint8_t)((encoded[8]&240)|14);
    for(unsigned fov=0;fov<=60;++fov){encoded[7]=(uint8_t)(1|(fov<<1));checksum(encoded);
        CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && decoded.fov_offset==fov && decoded.vertical_aim);}
    encoded[7]=122;checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    encoded[7]=128;checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    /* Version15 adds Vulkan without shifting any older flags. */
    for(unsigned renderer=0;renderer<SSA_RENDERERS;++renderer){next=base;next.renderer=renderer;
        next.jump_enabled=next.alternate_weapons=next.water_reflections=1;
        CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==1);
        CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && ssa_settings_equal(&next,&decoded));}
    /* Existing v15 retains all settings and its full-resolution native look. */
    next=base;CHECK(settings_legacy(&next,encoded)==1);
    encoded[0]='S';encoded[5]='P';encoded[6]='T';encoded[8]=(uint8_t)((encoded[8]&240)|15);checksum(encoded);
    next.mipmaps=0;CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && ssa_settings_equal(&next,&decoded));
    encoded[9]|=4;checksum(encoded);CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==-1);
    /* v16 migrates the formerly coupled godrays flag without changing mips. */
    for(unsigned decals=0;decals<2;++decals){next=base;next.surface_decals=decals;
        CHECK(settings_legacy(&next,encoded)==1);
        encoded[0]='S';encoded[5]='P';encoded[8]&=240;checksum(encoded);
        next.godrays=decals;
        CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && ssa_settings_equal(&next,&decoded));
    }

    printf("{\"result\":\"pass\",\"checks\":%u}\n",checks);return 0;
}
