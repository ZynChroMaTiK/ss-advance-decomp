#define main cadence_fixture_main
#include "test_weapon_cadence.c"
#undef main
#include "ssa/audio.h"
#include "ssa/world_raster.h"
#include <string.h>

typedef struct Sink {SsaAudio *audio;unsigned ends,stops;} Sink;
static void play_fx(void *ctx,unsigned id){Sink *s=ctx;if(id==0x13 || id==0x1a)++s->ends;CHECK(ssa_audio_play(s->audio,id)>=0);}
static void stop_fx(void *ctx,unsigned id){Sink *s=ctx;++s->stops;CHECK(ssa_audio_stop(s->audio,id)>=0);}
static void music_fx(void *ctx,unsigned command,unsigned level){Sink *s=ctx;CHECK(ssa_audio_music(s->audio,command,level)>=0);}
static void source_fx(void *ctx,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point){
    Sink *s=ctx;if(command==SSA_AUDIO_SOURCE_STOP)++s->stops;
    if(command==SSA_AUDIO_SOURCE_PLAY && (id==0x13 || id==0x1a))++s->ends;
    CHECK(ssa_audio_source(s->audio,command,id,source,point)>=0);
}
static unsigned active_song(const SsaAudio *a,unsigned song,uint64_t source,int scoped){
    unsigned i,n=0;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].active && a->players[i].song==song && (!scoped || a->players[i].source==source))++n;
    return n;
}
static void audio_tick(SsaNativeGame *g,SsaAudio *a){int16_t pcm[1600];
    CHECK(ssa_native_game_tick(g)==1);CHECK(ssa_audio_render(a,pcm,800)==1 && !a->errors);
}
static void loop_case(const uint8_t *rom,unsigned weapon,unsigned mode,unsigned scoped){
    SsaNativeGame *g=click_game(rom,weapon);SsaAudio *a=calloc(1,sizeof(*a));Sink sink={a,0,0};
    SsaNativeAudioOps ops={play_fx,stop_fx,&sink,NULL,NULL,NULL,music_fx,scoped?source_fx:NULL};
    unsigned i,song=weapon==1?0x12:weapon==4?0x16:0x19;
    CHECK(a && ssa_audio_init(a,rom,0x800000,48000)==1);
    CHECK(ssa_audio_level_music(a,0)==1 && ssa_native_game_bind_audio(g,&ops)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    for(i=0;i<24;++i)audio_tick(g,a);
    CHECK(g->weapon_game.shots[weapon-1]>0 && g->audio_weapon_loop==song);
    CHECK(active_song(a,song,1,scoped)==1);
    if(scoped)CHECK(ssa_audio_source(a,SSA_AUDIO_SOURCE_LOOP,song,999,NULL)==1);
    switch(mode){
    case 0:CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);break;
    case 1:g->players[0].inventory.ammo[1]=100;CHECK(ssa_native_game_select_weapon(g,2)==1);break;
    case 2:g->players[0].inventory.ammo[weapon-1]=0;break;
    case 3:ssa_pc_input_runtime_focus(&g->input_runtime,0);break;
    case 4:ssa_pc_input_runtime_enable(&g->input_runtime,0);break;
    case 5:g->frame_game.flags|=1;break;
    case 6:g->players[0].inventory.health=0;break;
    default:CHECK(ssa_native_game_bind_audio(g,NULL)==1);break;
    }
    audio_tick(g,a);
    CHECK(!g->audio_weapon_loop && !active_song(a,song,1,scoped));
    if(scoped && mode!=6)CHECK(active_song(a,song,999,1)==1); /* Death cue intentionally clears all effects. */
    if(mode!=6)CHECK(a->players[0].active && a->players[0].song==0x73 && a->music_command==2);
    if(mode==0 && weapon!=4)CHECK(sink.ends==1); /* Wind-down remains audible. */
    CHECK(sink.stops>0);
    for(i=0;i<8;++i){audio_tick(g,a);
        if(active_song(a,song,1,scoped))fprintf(stderr,"loop restarted weapon%u mode%u scoped%u tick%u current%u requested%u held%u flags%x ammo%d\n",weapon,mode,scoped,i,g->gameplay.weapon.current,g->gameplay.weapon.requested,g->weapon.controller.held,(unsigned)g->gameplay.weapon.flags,g->players[0].inventory.ammo[weapon-1]);
        CHECK(!active_song(a,song,1,scoped));}
    ssa_native_game_free(g);free(g);free(a);
}
static void boss_loop_case(const uint8_t *rom,unsigned mode,unsigned scoped){
    SsaNativeGame *g=click_game(rom,5);SsaAudio *a=calloc(1,sizeof(*a));Sink sink={a,0,0};unsigned i;
    SsaNativeAudioOps ops={play_fx,stop_fx,&sink,NULL,NULL,NULL,music_fx,scoped?source_fx:NULL};
    CHECK(a && ssa_audio_init(a,rom,0x800000,48000)==1);
    g->music_mode=mode;
    CHECK(ssa_native_game_bind_audio(g,&ops)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    for(i=0;i<24;++i)audio_tick(g,a);
    CHECK(g->weapon_game.shots[4]>0 && g->audio_weapon_loop==0x19 && active_song(a,0x19,1,scoped)==1);
    CHECK(a->players[0].active);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    audio_tick(g,a);CHECK(!g->audio_weapon_loop && !active_song(a,0x19,1,scoped) && sink.ends==1);
    CHECK(a->players[0].active);
    ssa_native_game_free(g);free(g);free(a);
}
static void projectile_case(const uint8_t *rom,int pitch,unsigned third,unsigned weapon){
    SsaNativeGame *g=click_game(rom,weapon);SsaProjectile *p=NULL;unsigned i,j;
    int32_t x,y,z,vx,vy,vz;uint16_t facing;
    CHECK(g->projectiles.ballistic_player_rockets==1);
    CHECK(g->projectiles.ballistic_player_prion==1);
    CHECK(g->projectiles.ballistic_player_cannon==1);
    g->input.yaw_per_count=1;
    g->vertical_aim=1;
    CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,-pitch)==1);
    if(third){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
    tap(g);await_click(g,weapon,0);
    for(j=0;j<100;++j)if(g->projectiles.active[j] && g->projectiles.projectiles[j].entity->type==(weapon==6?0x65:weapon==10?0x67:0x69)){p=&g->projectiles.projectiles[j];break;}
    CHECK(p && p->owner==1 && (weapon==7 || p->weapon==weapon) && !(p->entity->body.flags&0x4000));
    CHECK(p->entity->body.flags&0x100); /* Original owner collision exclusion. */
    if(weapon==7){int32_t sine,cosine,ps,pc;double dx,dy,forward,side,up,slope;
        CHECK((g->player.free_aim!=0)==(pitch!=0)); /* Level aim before any pitch input. */
        ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
        ssa_sincos(&g->math,(uint16_t)g->player.pitch,&ps,&pc);slope=(double)ps/pc;
        dx=(p->entity->body.x-(double)g->players[0].entity.body.x)/65536;
        dy=(p->entity->body.y-(double)g->players[0].entity.body.y)/65536;
        forward=(dx*sine+dy*cosine)/65536;side=(dx*cosine-dy*sine)/65536;
        up=(p->entity->body.z-(double)g->player.camera.eye_z)/65536;
        if(side<=-0.04 || side>=0.04)fprintf(stderr,"Prion spawn pitch%d third%u heading%u side%.6f forward%.6f pos(%d,%d) player(%d,%d) timer%u\n",
            pitch,third,g->players[0].motion.movement.heading,side,forward,p->entity->body.x,p->entity->body.y,
            g->players[0].entity.body.x,g->players[0].entity.body.y,p->timer);
        CHECK(side>-0.04 && side<0.04); /* Old40-unit sideways offset fails. */
        CHECK(forward-192.0*pc/65536>-0.04 && forward-192.0*pc/65536<0.04);
        CHECK(up-forward*slope>-0.04 && up-forward*slope<0.04);
        CHECK(!p->timer && p->source==&g->players[0].entity && p->speed==16*65536);
        CHECK(g->player.view.third_person==third); /* Close walls may legitimately retract the camera. */
    }
    vx=p->entity->body.velocity_x;vy=p->entity->body.velocity_y;vz=p->entity->body.velocity_z;facing=p->facing;
    CHECK(vx || vy);if(pitch)CHECK(vz);
    if(weapon==7){int32_t sine,cosine,ps,pc;double forward,error;
        ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
        ssa_sincos(&g->math,(uint16_t)g->player.pitch,&ps,&pc);
        forward=((double)vx*sine+(double)vy*cosine)/65536;
        error=vz-forward*(double)ps/pc;CHECK(error>-16 && error<16);
    }
    for(i=0;i<6;++i){
        x=p->entity->body.x;y=p->entity->body.y;z=p->entity->body.z;
        CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,i%2? -8192:16384,i%2?1200:-800)==1);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0)==1);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',i<2,0)==1);
        CHECK(ssa_native_game_tick(g)==1 && g->projectiles.active[j]);
        CHECK(p->entity->body.velocity_x==vx && p->entity->body.velocity_y==vy && p->facing==facing);
        CHECK(p->entity->body.x==x+vx && p->entity->body.y==y+vy && p->entity->body.z==z+vz);
        CHECK(p->entity->body.velocity_z==vz);
    }
    ssa_native_game_free(g);free(g);
}
static void clipped_portal(void){
    uint8_t clipped[14520+120],original[14520+120],texture[128];
    unsigned top,x,i,j;static const uint32_t steps[]={0,1,0x12345678u,0x80000000u,0xffffffffu};
    for(i=0;i<128;++i)texture[i]=(uint8_t)i;
    for(top=1;top<120;top+=5)for(x=0;x<120;x+=17)for(j=0;j<5;++j){
        uint32_t fraction=steps[(j+2)%5];size_t at=120+top*120+x;
        memset(clipped,0xa5,sizeof(clipped));memcpy(original,clipped,sizeof(original));
        CHECK(ssa_world_raster_column(original,sizeof(original),at,texture,128,fraction,steps[j],121-top,25)==0);
        CHECK(ssa_world_raster_column_clip_bottom(clipped,14520,at,texture,128,fraction,steps[j],121-top,25)==0);
        CHECK(!memcmp(clipped,original,14520));
        for(i=14520;i<sizeof(clipped);++i)CHECK(clipped[i]==0xa5);
    }
    memset(clipped,0xa5,sizeof(clipped));memcpy(original,clipped,sizeof(original));
    CHECK(ssa_world_raster_column_clip_bottom(clipped,14520,14519,texture,128,0,0,3,25)==-1);
    CHECK(!memcmp(clipped,original,sizeof(clipped)));
}
static unsigned shot_slot(SsaNativeGame *g,unsigned weapon){
    unsigned slot;tap(g);await_click(g,weapon,0);
    for(slot=0;slot<100;++slot)if(g->projectiles.active[slot] &&
        g->projectiles.projectiles[slot].entity->type==(weapon==6?0x65:weapon==7?0x69:0x67))break;
    CHECK(slot<100);return slot;
}
static void ceiling_case(const uint8_t *rom,unsigned weapon,unsigned third,unsigned mode){
    SsaNativeGame *g=click_game(rom,weapon);SsaProjectile *p;SsaBody *b;
    unsigned slot,i,particles=0;uint8_t before[100];int32_t contact;
    CHECK(g->projectiles.ceiling_impacts==1);
    if(third){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
    slot=shot_slot(g,weapon);p=&g->projectiles.projectiles[slot];b=&p->entity->body;
    contact=(int32_t)b->ceiling*65536-b->height;
    b->z=contact-8*65536;b->velocity_x=b->velocity_y=0;b->velocity_z=4*65536;
    /* Below the ceiling remains alive. Then test exact contact, overshoot,
     * and a larger timestep which crosses the entire remaining aperture. */
    CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,1,&g->projectile_ops)==0);
    CHECK(g->projectiles.active[slot] && b->z==contact-4*65536);
    b->velocity_z=(mode==0?4:mode==1?16:64)*65536;
    for(i=0;i<100;++i)before[i]=g->spawn.actors[i].owned;
    CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,mode==2?3:1,&g->projectile_ops)==1);
    CHECK(!g->projectiles.active[slot]);
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && !before[i] && g->spawn.actors[i].entity.type==0x96 &&
        g->spawn.actors[i].animation==g->particle_rules.animation[4]){
        CHECK(g->spawn.actors[i].entity.body.z==contact);++particles;
    }
    CHECK(particles==(weapon==6?1u:0u));
    /* A ceiling is never an enemy target for the conversion projectile. */
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned)CHECK(!(g->spawn.actors[i].entity.body.flags&0x400));
    ssa_native_game_free(g);free(g);
}
static void cannon_floor_case(const uint8_t *rom,unsigned third,unsigned owner,unsigned mode){
    SsaNativeGame *g=click_game(rom,10);SsaProjectile *p;SsaBody *b;
    unsigned slot,i;int32_t vx,vy,x,y,floor;
    CHECK(g->projectiles.rolling_cannonballs==1);
    if(third){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
    slot=shot_slot(g,10);p=&g->projectiles.projectiles[slot];b=&p->entity->body;
    p->owner=owner; /* Exercise the floor policy for owned and enemy shots. */
    vx=b->velocity_x;vy=b->velocity_y;floor=(int32_t)b->floor*65536;
    CHECK((vx || vy) && (b->flags&SSA_BODY_NO_GRAVITY));
    b->z=floor+8*65536;b->velocity_z=-4*65536;
    CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,1,&g->projectile_ops)==0 && g->projectiles.active[slot]);
    CHECK(b->z==floor+4*65536 && b->velocity_z==-4*65536); /* No gravity added. */
    b->velocity_z=-(mode==0?4:mode==1?16:64)*65536;
    CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,mode==2?3:1,&g->projectile_ops)==0 && g->projectiles.active[slot]);
    CHECK(b->z==(int32_t)b->floor*65536 && !b->velocity_z);
    for(i=0;i<8;++i){x=b->x;y=b->y;
        CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,1,&g->projectile_ops)==0 && g->projectiles.active[slot]);
        CHECK(b->x==x+vx && b->y==y+vy && b->velocity_x==vx && b->velocity_y==vy);
        CHECK(b->z==(int32_t)b->floor*65536 && !b->velocity_z && (b->flags&SSA_BODY_NO_GRAVITY));
    }
    ssa_native_game_free(g);free(g);
}
int main(int argc,char **argv){
    FILE *f;uint8_t *rom=malloc(0x800000);unsigned w,mode,scoped,loops=0,rockets=0,prions=0,cannons=0;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    clipped_portal();
    for(scoped=0;scoped<2;++scoped)
        for(mode=0;mode<3;++mode){ceiling_case(rom,6,scoped,mode);ceiling_case(rom,7,scoped,mode);
            cannon_floor_case(rom,scoped,0,mode);cannon_floor_case(rom,scoped,1,mode);}
    for(w=1;w<=5;++w)if(w==1 || w==4 || w==5)for(scoped=0;scoped<2;++scoped)for(mode=0;mode<8;++mode){
        if(w==1 && mode==2)continue; /* Original chainsaw ammo restores to999. */
        loop_case(rom,w,mode,scoped);++loops;
    }
    for(scoped=0;scoped<2;++scoped){boss_loop_case(rom,19,scoped);boss_loop_case(rom,20,scoped);
        projectile_case(rom,0,scoped,6);projectile_case(rom,3000,scoped,6);projectile_case(rom,-3000,scoped,6);rockets+=3;
        projectile_case(rom,0,scoped,7);projectile_case(rom,3000,scoped,7);projectile_case(rom,-3000,scoped,7);prions+=3;
        projectile_case(rom,0,scoped,10);projectile_case(rom,3000,scoped,10);projectile_case(rom,-3000,scoped,10);cannons+=3;}
    free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"weapon_loop_cases\":%u,\"rocket_trajectory_cases\":%u,\"rocket_frames\":%u,\"prion_trajectory_cases\":%u,\"prion_frames\":%u,\"cannon_trajectory_cases\":%u,\"cannon_frames\":%u,\"cannon_floor_cases\":12,\"ceiling_cases\":12}\n",checks,loops,rockets,rockets*6,prions,prions*6,cannons,cannons*6);return 0;
}
