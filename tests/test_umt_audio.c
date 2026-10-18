#include "ssa/audio.h"
#include "ssa/pickup.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks,pickups,runs;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"U.M.T. audio line%d: %s\n",__LINE__,#x);abort();}}while(0)
typedef struct Sink {SsaAudio *audio;unsigned source,sounds,message,removed;} Sink;
static void event(void *context,enum SsaPickupEvent kind,unsigned a,unsigned b){
    Sink *s=context;(void)b;
    if(kind==SSA_PICKUP_SOUND){
        CHECK(a==0x1d);++s->sounds;
        CHECK((s->source?ssa_audio_source(s->audio,SSA_AUDIO_SOURCE_PLAY,a,1,NULL):ssa_audio_play(s->audio,a))==1);
    }else if(kind==SSA_PICKUP_MESSAGE){CHECK(a==0x93);s->message=a;}
    else CHECK(0);
}
static void remove_item(void *context,SsaPickup *item){Sink *s=context;CHECK(item->item==0x27);++s->removed;}
static unsigned busy(const SsaAudio *a,unsigned song,uint64_t source,unsigned scoped){
    unsigned i,n=0;
    for(i=0;i<SSA_AUDIO_PLAYERS;++i){unsigned v,active=a->players[i].active;
        if(a->players[i].song!=song || (scoped && a->players[i].source!=source))continue;
        for(v=0;v<SSA_AUDIO_VOICES;++v)if(a->voices[v].active && a->voices[v].player==i)active=1;
        n+=active!=0;
    }
    return n;
}
static unsigned tick(SsaAudio *a){
    int16_t out[1600];unsigned i,energy=0;
    CHECK(ssa_audio_render(a,out,a->sample_rate/60)==1 && !a->errors);
    for(i=0;i<a->sample_rate/30;++i)energy|=out[i]!=0;
    return energy;
}
static void pickup_case(SsaAudio *a,const uint8_t *rom,unsigned rate,unsigned source,unsigned music){
    SsaInventoryRules rules;SsaInventory inventory={0};SsaPickup item={0x27,0,NULL};
    SsaPickupPlayer player={&inventory,0xc9,0,0,NULL};Sink sink={a,source,0,0,0};
    SsaPickupGame game={&rules,{event,remove_item,NULL,&sink},0,1,0};unsigned i,heard=0;
    ssa_audio_reset(a);a->sample_rate=rate;a->music_enabled=1;
    CHECK(ssa_inventory_rules_load(&rules,rom,0x800000)==0);
    if(music)CHECK(ssa_audio_level_music(a,0)==1);
    CHECK(ssa_pickup_touch(&game,&player,&item,0)==1);
    CHECK(sink.sounds==1 && sink.message==0x93 && sink.removed==1 && game.collected==1);
    CHECK(inventory.pending_count==1 && inventory.pending_weapons[0]==8);
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].song==0x1d && a->players[i].active)
        CHECK(!a->players[i].positioned && a->players[i].local_priority && !a->players[i].source_loop);
    CHECK(busy(a,0x1d,1,source)==1);
    for(i=0;i<240;++i){
        heard|=tick(a);
        if(music && i==20)CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_LOOP,0x67,44,NULL)==1);
        if(music && i%20==0)CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_PLAY,0x14,1,NULL)==1);
    }
    CHECK(heard && !busy(a,0x1d,0,0));
    if(music){
        CHECK(a->players[0].active && a->players[0].song==0x73);
        CHECK(busy(a,0x67,44,1)==1);
    }else for(i=0;i<60;++i)CHECK(tick(a)==0);
    ++pickups;++runs;
}
static void overlapping_case(SsaAudio *a){
    unsigned i;
    ssa_audio_reset(a);a->sample_rate=48000;
    CHECK(ssa_audio_level_music(a,0)==1);
    CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_LOOP,0x67,44,NULL)==1);
    CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_LOOP,0x1d,88,NULL)==1);
    for(i=0;i<360;++i){
        if(i==0 || i==20 || i==40)CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_PLAY,0x1d,1,NULL)==1);
        if(i==41)CHECK(busy(a,0x1d,1,1)==3);
        (void)tick(a);
    }
    CHECK(!busy(a,0x1d,1,1));
    CHECK(busy(a,0x1d,88,1)==1 && a->loops>0);
    CHECK(busy(a,0x67,44,1)==1 && a->players[0].active && a->players[0].song==0x73);
    CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_STOP,0x1d,88,NULL)==1);
    CHECK(!busy(a,0x1d,0,0) && busy(a,0x67,44,1)==1);
    ++runs;
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);SsaAudio *a=calloc(1,sizeof(*a));FILE *f;unsigned rate,source,music;
    CHECK(argc==2 && rom && a && (f=fopen(argv[1],"rb")));
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_audio_init(a,rom,0x800000,24000)==1);
    for(rate=24000;rate<=48000;rate+=24000)for(source=0;source<2;++source)for(music=0;music<2;++music)
        pickup_case(a,rom,rate,source,music);
    overlapping_case(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"runs\":%u,\"actual_pickups\":%u,\"decoder_errors\":%u}\n",checks,runs,pickups,a->errors);
    free(a);free(rom);return 0;
}
