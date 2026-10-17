/* Requested PC behavior, deliberately different from GBA voice arbitration. */
static unsigned concurrent_cases;
static unsigned active_voices(const SsaAudio *a,unsigned player){unsigned i,count=0;
    for(i=0;i<SSA_AUDIO_VOICES;++i)count+=a->voices[i].active && a->voices[i].player==player;
    return count;
}
static void same_music(const SsaAudio *a,const SsaAudio *b){
    CHECK(!memcmp(&a->players[0],&b->players[0],sizeof(a->players[0])));
    CHECK(!memcmp(a->voices,b->voices,SSA_AUDIO_MUSIC_VOICES*sizeof(a->voices[0])));
}
static void concurrency_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0},b={0};unsigned type,i,j;
    uint32_t h=0x7fd000,bank=0x7fd100,wave=0x7fd800;
    SsaAudioPoint left={-256*65536,0,0},right={256*65536,0,0},moved={128*65536,0,0};
    SsaAudioListener ear={{0,0,0},65536,0};
    /* Two tracks of the same oscillator type. Both start a second tied note
     * after one tick: music replaces it, effects retain both notes. */
    static const uint8_t sequence[]={0xbd,0,0xbe,32,0xcd,8,32,0xcd,9,8,
        0xcf,60,127,0x81,0xcf,64,127,0xb0,0xb2,0x10,0xf0,0x7f,0x08};
    CHECK(copy);memcpy(copy,rom,0x800000);
    put32(copy+0x3e99d0+0x14*8,0x08000000+h);put32(copy+0x3e99d0+0x73*8,0x08000000+h);
    copy[h]=2;copy[h+2]=10;put32(copy+h+4,0x08000000+bank);
    put32(copy+wave,0x40000000);put32(copy+wave+4,24000*1024);
    put32(copy+wave+8,0);put32(copy+wave+12,16);memset(copy+wave+16,64,16);
    for(i=0;i<2;++i){put32(copy+h+8+i*4,0x087ff000+i*32);
        memcpy(copy+0x7ff000+i*32,sequence,sizeof(sequence));
        put32(copy+0x7ff000+i*32+19,0x087ff000+i*32+17);}
    for(type=0;type<=4;++type){unsigned first,second;
        uint8_t tone[]={0,60,0,0,2,0,0,0,0,0,15,0};tone[0]=(uint8_t)type;
        if(!type){tone[8]=255;tone[10]=255;}
        memcpy(copy+bank,tone,12);
        put32(copy+bank+4,type==0?0x08000000+wave:type==3?0x08000000+wave+16:2);
        memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
        CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_init(&b,copy,0x800000,24000)==1);
        CHECK(ssa_audio_play(&a,0x73)==1 && ssa_audio_play(&b,0x73)==1);
        CHECK(ssa_audio_listen(&a,&ear)==1 && ssa_audio_play_at(&a,0x14,&left)==1);
        first=latest(&a,0x14);CHECK(ssa_audio_play_at(&a,0x14,&right)==1);second=latest(&a,0x14);
        CHECK(first!=second && instances(&a,0x14)==2 && active_voices(&a,0)==2);
        CHECK(active_voices(&a,first)==2 && active_voices(&a,second)==2);same_music(&a,&b);
        {SsaAudio fx=a;int16_t mixed[2],music[2],effects[2];
            CHECK(ssa_audio_stop(&fx,0x73)==1);
            CHECK(ssa_audio_render(&a,mixed,1)==1 && ssa_audio_render(&b,music,1)==1 && ssa_audio_render(&fx,effects,1)==1);
            CHECK(abs(mixed[0]-music[0]-effects[0])<=2 && abs(mixed[1]-music[1]-effects[1])<=2);
            CHECK(mixed[0]!=music[0] && mixed[1]!=music[1]);}
        mix_frames(&a,401);mix_frames(&b,401);same_music(&a,&b);
        CHECK(active_voices(&a,0)==2 && a.voices[0].note==64 && a.voices[1].note==64);
        CHECK(active_voices(&a,first)==4 && active_voices(&a,second)==4);
        CHECK(ssa_audio_move(&a,0x14,&moved)==1);
        CHECK(!memcmp(&a.players[first].position,&left,sizeof(left)) &&
              !memcmp(&a.players[second].position,&moved,sizeof(moved)));
        /* Fill/recycle effects repeatedly. Music never loses a track/voice. */
        for(i=0;i<SSA_AUDIO_EFFECT_PLAYERS+8;++i){CHECK(ssa_audio_play(&a,0x14)==1);same_music(&a,&b);}
        CHECK(instances(&a,0x14)==SSA_AUDIO_EFFECT_PLAYERS && a.effect_recycles>0);
        mix_frames(&a,400);mix_frames(&b,400);same_music(&a,&b);
        CHECK(a.voice_recycles>0 && !a.errors && !a.clipped);
        CHECK(ssa_audio_stop(&a,0x14)==1 && !instances(&a,0x14));same_music(&a,&b);
        for(i=SSA_AUDIO_MUSIC_VOICES;i<SSA_AUDIO_VOICES;++i)CHECK(!a.voices[i].active);
        /* With effects stopped, the next buffer equals music-only exactly. */
        {int16_t actual[800],expected[800];
            CHECK(ssa_audio_render(&a,actual,400)==1 && ssa_audio_render(&b,expected,400)==1);
            CHECK(!memcmp(actual,expected,sizeof(actual)));}
        ++concurrent_cases;
    }
    /* An ended sequence's release/echo still owns its instance. Starting a
     * second identical effect must not reset that tail's track or oscillator. */
    copy[h]=1;copy[bank]=0;copy[bank+8]=255;copy[bank+10]=255;copy[bank+11]=255;
    put32(copy+bank+4,0x08000000+wave);
    {static const uint8_t tail[]={0xbd,0,0xbe,16,0xcd,8,32,0xcd,9,8,0xcf,60,127,0x81,0xb1};
        unsigned first,second;SsaAudioPlayer saved;SsaAudioVoice voice;
        memcpy(copy+0x7ff000,tail,sizeof(tail));memset(&a,0,sizeof(a));
        CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_play(&a,0x14)==1);
        first=latest(&a,0x14);mix_frames(&a,401);CHECK(!a.players[first].active && active_voices(&a,first)==1);
        saved=a.players[first];voice=a.voices[SSA_AUDIO_MUSIC_VOICES];
        CHECK(ssa_audio_play(&a,0x14)==1);second=latest(&a,0x14);
        CHECK(first!=second && !memcmp(&saved,&a.players[first],sizeof(saved)) &&
              !memcmp(&voice,&a.voices[SSA_AUDIO_MUSIC_VOICES],sizeof(voice)));
        CHECK(ssa_audio_stop(&a,0x14)==1);
        for(j=0;j<SSA_AUDIO_VOICES;++j)CHECK(!a.voices[j].active);
        ++concurrent_cases;
    }
    free(copy);
}
