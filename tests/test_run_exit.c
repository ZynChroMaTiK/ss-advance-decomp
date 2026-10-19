#include "ssa/native_game.h"
#include "ssa/run_exit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,deaths,retries,gameovers,voices,music_changes;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"run exit line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void play(void *p,unsigned id){(void)p;if(id==17)++voices;}
static void stop(void *p,unsigned id){(void)p;(void)id;}
static void music(void *p,unsigned command,unsigned level){(void)p;CHECK(command==10 && level==0);++music_changes;}
static void hud_lives(const SsaNativeGame *g)
{
    unsigned i,count=0;
    CHECK(g->hud.game_value==g->game.life_game.continue_flag);
    for(i=0;i<g->sprites.active_count;++i){const SsaWeaponObject *o=g->sprites.objects[g->sprites.active[i]];
        if(o->type==16){CHECK((o->flags&1) && o->animation==0x6bu+g->game.life_game.continue_flag);++count;}}
    CHECK(count==1);
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned difficulty,language,lives,t;
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaNativeCarry entry;
    const unsigned counts[3]={5,3,2};SsaNativeAudioOps ops={play,stop,NULL,NULL,NULL,NULL,music,NULL};
    CHECK(argc==2 && rom && g && (f=fopen(argv[1],"rb")));
    CHECK(fread(rom,1,0x800000,f)==0x800000 && !fclose(f));
    for(difficulty=0;difficulty<3;++difficulty)for(language=0;language<3;++language){
        CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,difficulty,NULL)==1);
        CHECK(g->game.life_game.continue_flag==counts[difficulty] && g->gameplay.life_game.continue_flag==counts[difficulty]);
        g->hud_notice.language=language;g->score=123456;
        CHECK(ssa_native_game_carry_capture(&entry,g)==1);
        for(lives=counts[difficulty];lives;--lives){unsigned before_voices=voices,before_music=music_changes;
            CHECK(ssa_native_game_tick(g)==1 && g->game.life_game.continue_flag==lives);++frames;
            hud_lives(g);
            CHECK(ssa_native_game_bind_audio(g,&ops)==1);
            g->players[0].inventory.health=g->players[0].entity_health=0;g->score=3500000;
            for(t=1;t<=400;++t){
                CHECK(ssa_native_game_tick(g)==1);++frames;
                if(t==1)hud_lives(g);
                if(lives==1){
                    CHECK(!g->game.life_game.continue_flag);
                    CHECK(g->music_mode==(t<91?2u:10u));
                    if(t>=91)CHECK(g->hud_notice.id==0xb4);
                }else CHECK(g->game.life_game.continue_flag==lives && g->music_mode==2 && g->hud_notice.id==0xb5);
                if(g->game.life_game.input_state==4)break;
            }
            CHECK(t==(lives==1?392u:301u));
            CHECK(voices-before_voices==(lives==1?0u:1u));
            CHECK(music_changes-before_music==(lives==1?1u:0u));
            CHECK(g->players[0].motion.movement.flags&0x10);++deaths;
            CHECK(ssa_run_exit(&g->game.life_game,&g->frame_game.flags)==(lives==1?SSA_RUN_SCORES:SSA_RUN_RETRY));
            CHECK(g->frame_game.flags==2 && !g->game.life_game.message_ticks && g->game.life_game.continue_flag==lives-1);
            if(lives==1){CHECK(g->game.life_game.input_state==7 && g->score==3500000);++gameovers;}
            else{unsigned remaining=g->game.life_game.continue_flag;
                ssa_native_game_free(g);CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,difficulty,&entry)==1);
                g->game.life_game.continue_flag=g->gameplay.life_game.continue_flag=(uint8_t)remaining;
                g->hud_notice.language=language;
                CHECK(g->players[0].inventory.health==100 && g->score==entry.score &&
                      !memcmp(&g->players[0].inventory,&entry.inventory,sizeof entry.inventory));++retries;
            }
        }
        ssa_native_game_free(g);
    }
    free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"native_frames\":%u,\"deaths\":%u,\"retries\":%u,\"gameovers\":%u,\"death_voices\":%u,\"gameover_music_changes\":%u,\"languages\":3,\"difficulties\":3}\n",
        checks,frames,deaths,retries,gameovers,voices,music_changes);return 0;
}
