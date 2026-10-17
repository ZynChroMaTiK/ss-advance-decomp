/* Test-only access; the port never executes ROM instructions. */
#include "../src/audio.c"
/* Archived original hardware allocator reference. The requested PC mixer
 * deliberately does NOT use this policy. Keep its ROM proof separate from
 * the native concurrency regressions in test_audio.c. */
static uint32_t original_owner(const uint8_t *rom,unsigned player,unsigned track){
    return u32(rom+PLAYER_TABLE+player*12+4)+track*0x50;
}
int ssa_test_audio_channel(const uint8_t *rom,const unsigned *in){
    unsigned i,releasing=0,priority=in[3];int best=-1;
    uint32_t owner=original_owner(rom,in[1],in[2]);
    if(in[0]){const unsigned *s=in+4+(3+in[0]-1)*5;
        if(s[0] && s[1]!=3 && s[1]!=5 && (s[4]>priority ||
           (s[4]==priority && original_owner(rom,s[2],s[3])<owner)))return -1;
        return (int)(3+in[0]-1);
    }
    for(i=0;i<3;++i){const unsigned *s=in+4+i*5;uint32_t old_owner;
        if(!s[0])return (int)i;
        old_owner=original_owner(rom,s[2],s[3]);
        if((s[1]==3 || s[1]==5) && !releasing){releasing=1;priority=s[4];owner=old_owner;best=(int)i;continue;}
        if(releasing && s[1]!=3 && s[1]!=5)continue;
        if(s[4]<priority || (s[4]==priority && old_owner>=owner)){
            priority=s[4];owner=old_owner;best=(int)i;}
    }
    return best;
}
void ssa_test_audio_volume(const uint8_t *rom,const int *in,unsigned *out){
    SsaAudio a={0};SsaAudioVoice v={0};SsaAudioTrack *t=&a.players[0].tracks[0];double l,r;
    a.rom=rom;a.rom_size=0x800000;a.sample_rate=24000;
    t->volume=(unsigned)in[0];t->pan=(unsigned)in[1];
    v.velocity=(unsigned)in[2];v.instrument_pan=in[3];v.envelope_level=(unsigned)in[4];v.length=1;
    voice_volume(&a,&v,0,out,out+1);out[2]=psg_goal(&a,&v);
    (void)sample(&a,&v,&l,&r);out[3]=(unsigned)(l*256);out[4]=(unsigned)(r*256);
}
