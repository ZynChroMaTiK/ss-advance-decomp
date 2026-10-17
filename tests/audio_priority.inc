/* Saturation must not evict or duck local pickup/weapon/hurt ownership. */
static unsigned priority_cases;
static void audio_priority_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    static const unsigned local_songs[]={0x35,0x19,0x0f};
    static const uint8_t sequence[]={0xbd,0,0xbe,16,0xcf,60,127,0xb0,0xb2,0x07,0xf0,0x7f,0x08};
    unsigned i,j,slots[3],voice;uint64_t serial[3];SsaAudioPoint point={0,0,0};
    int16_t local[16],crowded[16];uint32_t h=0x7fd000,bank=0x7fd100,wave=0x7fd800;
    CHECK(copy && a && b);memcpy(copy,rom,0x800000);
    copy[h]=4;copy[h+2]=10;put32(copy+h+4,0x08000000+bank);
    memset(copy+bank,0,12);copy[bank+1]=60;copy[bank+8]=255;copy[bank+10]=255;copy[bank+11]=255;
    put32(copy+bank+4,0x08000000+wave);put32(copy+wave,0x40000000);
    put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,16);memset(copy+wave+16,64,16);
    for(i=0;i<4;++i){put32(copy+h+8+i*4,0x087ff000);}
    memcpy(copy+0x7ff000,sequence,sizeof(sequence));
    put32(copy+0x3e99d0+0x14*8,0x08000000+h);put32(copy+0x3e99d0+0x73*8,0x08000000+h);
    for(i=0;i<3;++i)put32(copy+0x3e99d0+local_songs[i]*8,0x08000000+h);
    CHECK(ssa_audio_init(a,copy,0x800000,24000)==1 && ssa_audio_play(a,0x73)==1);
    for(i=0;i<3;++i){CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_PLAY,local_songs[i],1,NULL)==1);
        slots[i]=latest(a,local_songs[i]);serial[i]=a->players[slots[i]].serial;
        CHECK(a->players[slots[i]].local_priority && active_voices(a,slots[i])==4);
    }
    *b=*a;
    for(i=0;i<128;++i){CHECK(ssa_audio_source(b,SSA_AUDIO_SOURCE_PLAY,0x14,100+i,&point)==1);
        same_music(a,b);
        for(j=0;j<3;++j)CHECK(!memcmp(&a->players[slots[j]],&b->players[slots[j]],sizeof(a->players[0])));
        for(voice=SSA_AUDIO_MUSIC_VOICES;voice<SSA_AUDIO_VOICES;++voice)
            if(a->voices[voice].active)CHECK(!memcmp(&a->voices[voice],&b->voices[voice],sizeof(a->voices[0])));
    }
    CHECK(b->effect_recycles && b->voice_recycles && !b->effect_drops && !b->voice_drops);++priority_cases;
    /* Loud crowds yield headroom; stopping the crowd immediately restores the
     * exact local+music output, without a crowd-induced local recovery fade. */
    CHECK(ssa_audio_render(a,local,4)==1 && ssa_audio_render(b,crowded,4)==1);
    CHECK(a->effects_headroom_gain==b->effects_headroom_gain && b->world_headroom_gain<1 && !b->clipped);
    for(i=0;i<128;++i)CHECK(ssa_audio_source(b,SSA_AUDIO_SOURCE_STOP,SSA_AUDIO_SONGS,100+i,NULL)>=0);
    CHECK(ssa_audio_render(a,local,8)==1 && ssa_audio_render(b,crowded,8)==1);
    CHECK(!memcmp(local,crowded,sizeof(local)));same_music(a,b);++priority_cases;
    /* Released player tails retain the same protection as held notes. */
    for(voice=SSA_AUDIO_MUSIC_VOICES;voice<SSA_AUDIO_VOICES;++voice)
        if(a->voices[voice].active){a->voices[voice].released=b->voices[voice].released=1;}
    for(i=0;i<128;++i)CHECK(ssa_audio_play_at(b,0x14,&point)==1);
    for(voice=SSA_AUDIO_MUSIC_VOICES;voice<SSA_AUDIO_VOICES;++voice)
        if(a->voices[voice].active)CHECK(!memcmp(&a->voices[voice],&b->voices[voice],sizeof(a->voices[0])));
    for(j=0;j<3;++j)CHECK(b->players[slots[j]].serial==serial[j]);
    ++priority_cases;
    /* Local starts can reclaim world slots even when both pools are full. */
    CHECK(ssa_audio_source(b,SSA_AUDIO_SOURCE_PLAY,0x35,2,NULL)==1);
    CHECK(b->players[latest(b,0x35)].local_priority && active_voices(b,latest(b,0x35))==4);++priority_cases;
    ssa_audio_reset(a);CHECK(a->world_headroom_gain==1 && ssa_audio_play(a,0x73)==1);
    for(i=0;i<SSA_AUDIO_EFFECT_VOICES/4;++i)CHECK(ssa_audio_play(a,0x35)==1);
    CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_PLAY,0x14,100,&point)==1 && a->voice_drops==4);
    CHECK(!active_voices(a,latest(a,0x14)) && active_voices(a,0)==4 && !a->errors);++priority_cases;
    /* Freeing protected voices lets a subsequent world effect sound again. */
    CHECK(ssa_audio_stop(a,0x35)==1 && ssa_audio_play_at(a,0x14,&point)==1);
    CHECK(active_voices(a,latest(a,0x14))==4);++priority_cases;
    ssa_audio_reset(a);CHECK(ssa_audio_play(a,0x73)==1);
    for(i=0;i<SSA_AUDIO_EFFECT_PLAYERS;++i)CHECK(ssa_audio_play(a,0x35)==1);
    *b=*a;
    CHECK(ssa_audio_play_at(a,0x14,&point)==0 && a->effect_drops==b->effect_drops+1);
    CHECK(!memcmp(a->players,b->players,sizeof(a->players)) && !memcmp(a->voices,b->voices,sizeof(a->voices)));
    CHECK(!a->errors && !a->clipped);++priority_cases;
    free(a);free(b);free(copy);
}
