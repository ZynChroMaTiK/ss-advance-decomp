#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,aligned,third,first,switch_frames;static double maximum_error;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"chase aim line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static double absolute(double n){return n<0?-n:n;}
static void alignment(SsaNativeGame *g,SsaWeaponAim *query)
{
    SsaEntity *player=&g->players[0].entity;SsaWeaponHit target;int32_t sine,cosine,ps,pc;
    double dx,dy,dz,forward,lateral,up,depth,x,y,focal=2160*(0x55b05a/65536.0)/80;
    if(!g->player.pose.third_person){
        CHECK(g->pc_camera.x==player->body.x && g->pc_camera.y==player->body.y && g->pc_camera.z==g->player.camera.eye_z);
        CHECK(g->pc_camera.heading==g->players[0].motion.movement.heading && g->render_pitch==g->player.pitch);
        return;
    }
    ssa_sincos(&g->math,(uint16_t)g->player.pitch,&ps,&pc);
    CHECK(ssa_weapon_hit_query_unlimited(query,player,g->players[0].motion.movement.heading,
        g->player.camera.eye_z-player->body.z,(int32_t)((int64_t)ps*65536/pc),&target,1)==1);
    if(!target.impact){target.x=query->end_x;target.y=query->end_y;
        target.z=(int32_t)((int64_t)query->shoot_z+(int64_t)query->range*ps/pc);}
    /* Independently project the common sight ray's contact to a4K camera plane.
     * This fails for a fixed-offset camera that merely copies weapon pitch. */
    dx=(target.x-(double)g->pc_camera.x)/65536;dy=(target.y-(double)g->pc_camera.y)/65536;
    dz=(target.z-(double)g->pc_camera.z)/65536;
    ssa_sincos(&g->math,g->pc_camera.heading,&sine,&cosine);
    forward=(dx*sine+dy*cosine)/65536;lateral=(dx*cosine-dy*sine)/65536;
    ssa_sincos(&g->math,(uint16_t)g->render_pitch,&ps,&pc);
    depth=(forward*pc+dz*ps)/65536;up=(dz*pc-forward*ps)/65536;
    CHECK(depth>0);x=absolute(lateral*focal/depth);y=absolute(up*focal/depth);
    if(x>maximum_error)maximum_error=x;
    if(y>maximum_error)maximum_error=y;
    if(x>=2 || y>=2)fprintf(stderr,"offset %.4f,%.4f at pitch%d rendered%d chase%u\n",x,y,g->player.pitch,g->render_pitch,g->player.pose.third_person);
    CHECK(x<2 && y<2);++aligned;
}
static void weapon_independent_view(SsaNativeGame *g)
{
    unsigned weapon,current=g->gameplay.weapon.current;
    SsaRenderCamera camera;SsaViewPose pose;int32_t pitch;
    g->gameplay.weapon.current=2;CHECK(ssa_native_game_prepare_view(g)==1);
    camera=g->pc_camera;pose=g->pc_pose;pitch=g->render_pitch;
    /* Compare each weapon at exactly the same player/world pose, avoiding
     * false camera differences from eye bob, jumping or moving targets. */
    for(weapon=1;weapon<=10;++weapon){
        g->gameplay.weapon.current=(uint16_t)weapon;CHECK(ssa_native_game_prepare_view(g)==1);
        CHECK(!memcmp(&camera,&g->pc_camera,sizeof(camera)));
        CHECK(!memcmp(&pose,&g->pc_pose,sizeof(pose)) && pitch==g->render_pitch);
    }
    g->gameplay.weapon.current=(uint16_t)current;CHECK(ssa_native_game_prepare_view(g)==1);
}
static void muzzle_visibility(SsaNativeGame *g,SsaPcRenderer *renderer)
{
    unsigned id,mode;uint32_t *pixels=malloc((size_t)renderer->width*renderer->height*sizeof(*pixels));
    uint16_t animations[100];CHECK(pixels && g->player.pose.third_person);
    /* Original effects for every weapon flash family, plus an impact control.
     * Keep them alive while H changes view; simulation flags must never change. */
    for(id=8;id<=12;++id)CHECK(ssa_projectile_runtime_particle(&g->projectiles,id,
        g->players[0].entity.body.x,g->players[0].entity.body.y,g->players[0].entity.body.z,&g->combat_ops)==0);
    CHECK(ssa_projectile_runtime_particle(&g->projectiles,0,g->players[0].entity.body.x,
        g->players[0].entity.body.y,g->players[0].entity.body.z,&g->combat_ops)==0);
    for(mode=0;mode<3;++mode){unsigned i,n=0,controls=0;int hidden=mode!=1;
        CHECK(ssa_native_game_prepare_view(g)==1);
        CHECK((int)g->player.pose.third_person==hidden);
        g->actor_scene.projector.camera=&g->render_view.camera;
        for(i=0;i<g->spawn.pool.active_count;++i){SsaActorSource *s=&g->sources[i];
            unsigned before=g->actor_scene.queue.projection_count;
            animations[i]=s->animation;
            if(s->entity->type!=0x96)continue;
            if(!(s->entity->body.flags&0x2000)){CHECK(s->flags==s->entity->body.flags);++controls;continue;}
            ++n;CHECK(s->entity->body.flags==0x2020);
            CHECK(hidden?((s->flags&0x80) && !(s->flags&0x2000)):s->flags==s->entity->body.flags);
            CHECK(before<32);
            CHECK(ssa_actor_project(&g->actor_scene.projector,s,(s->flags&0x2000)!=0)==1);
            CHECK(g->actor_scene.queue.projection_count==before+(unsigned)!hidden);
        }
        g->actor_scene.projector.camera=NULL;
        CHECK(n==5 && controls>=1);CHECK(ssa_pc_renderer_draw(renderer,g)==1);
        memcpy(pixels,renderer->pixels,(size_t)renderer->width*renderer->height*sizeof(*pixels));
        for(i=0;i<g->spawn.pool.active_count;++i)if(g->sources[i].entity->type==0x96 &&
            (g->sources[i].entity->body.flags&0x2000))g->sources[i].animation=0;
        CHECK(ssa_pc_renderer_draw(renderer,g)==1);
        CHECK((memcmp(pixels,renderer->pixels,(size_t)renderer->width*renderer->height*sizeof(*pixels))==0)==hidden);
        for(i=0;i<g->spawn.pool.active_count;++i)g->sources[i].animation=animations[i];
        if(mode<2){ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
            CHECK(ssa_native_game_step(g)==1);}
    }
    free(pixels);
}
int main(int argc,char **argv)
{
    SsaNativeGame *g=calloc(1,sizeof(*g)),*saved=malloc(sizeof(*saved));SsaPcRenderer renderer={0};
    SsaWeaponAim query={0};uint8_t *rom=malloc(0x800000);FILE *f;unsigned i;
    static const int pitches[]={-14564,-10923,-5461,-910,-1,0,1,910,5461,10923,14564};
    static const uint16_t headings[]={0,12345,32768,54321};
    CHECK(argc==2 && g && saved && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(ssa_weapon_aim_init(&query,&g->world,&g->math)==1);CHECK(ssa_pc_renderer_resize(&renderer,640,480)==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_pc_input_sensitivity(&g->input,1)==1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
    for(i=0;i<30;++i)CHECK(ssa_native_game_step(g)==1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
    /* Test-only inventory grant, followed by normal weapon lowering/raising. */
    g->players[0].inventory.weapons|=2;g->players[0].inventory.ammo[1]=99;
    CHECK(ssa_native_game_select_weapon(g,2)==1);
    for(i=0;i<90 && (g->gameplay.weapon.current!=2 || g->weapon.object.y!=0x5000);++i)CHECK(ssa_native_game_step(g)==1);
    CHECK(g->gameplay.weapon.current==2 && g->weapon.object.y==0x5000);
    ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
    for(i=0;i<88;++i){int pitch=pitches[(i/4)%11];uint16_t heading=headings[i%4];
        int yaw=(int)(uint16_t)(heading-g->players[0].motion.movement.heading);
        SsaPlayerSpawn player;SsaWeaponAim weapon_trace;
        if(yaw>32767)yaw-=65536;
        ssa_pc_input_runtime_relative(&g->input_runtime,yaw,g->player.pitch-pitch);
        ssa_pc_input_runtime_key(&g->input_runtime,' ',i%20<2,0);
        CHECK(ssa_native_game_step(g)==1);CHECK(g->player.free_aim && g->player.pitch==pitch);
        CHECK(g->player.view.third_person);
        player=g->players[0];weapon_trace=g->weapon.trace;
        CHECK(ssa_native_game_prepare_view(g)==1);
        CHECK(!memcmp(&player,&g->players[0],sizeof(player)) && !memcmp(&weapon_trace,&g->weapon.trace,sizeof(weapon_trace)));
        alignment(g,&query);weapon_independent_view(g);
        if(g->player.pose.third_person){++third;
            CHECK(ssa_map_query(&g->player.view_collision,g->player.pose.x,g->player.pose.y,
                g->player.pose.x,g->player.pose.y,4*65536)==1);
            CHECK((int64_t)g->player.pose.z-4*65536>=(int64_t)g->player.view_collision.floor*65536);
            CHECK((int64_t)g->player.pose.z+4*65536<=(int64_t)g->player.view_collision.ceiling*65536);
        }else ++first;
        *saved=*g;CHECK(ssa_pc_renderer_draw(&renderer,g)==1);CHECK(!memcmp(saved,g,sizeof(*g)));
    }
    CHECK(third>=24);
    /* Normal lowering/raising, with level aim and both shorter ranged and
     * longer ranged weapons on either side of the chainsaw selection. */
    g->players[0].inventory.weapons|=1u|32u;
    g->players[0].inventory.ammo[0]=99;g->players[0].inventory.ammo[5]=99;
    ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0);
    ssa_pc_input_runtime_relative(&g->input_runtime,0,g->player.pitch);
    CHECK(ssa_native_game_step(g)==1 && g->player.pitch==0);
    for(i=0;i<4;++i){unsigned tick,weapon=i==1?6:i==3?2:1;
        CHECK(ssa_native_game_select_weapon(g,weapon)==1);
        for(tick=0;tick<120;++tick){
            CHECK(ssa_native_game_step(g)==1 && g->player.pose.third_person);
            weapon_independent_view(g);alignment(g,&query);++switch_frames;
            if(g->gameplay.weapon.current==weapon && g->weapon.object.y==0x5000)break;
        }
        CHECK(tick<120);
    }
    CHECK(g->weapon_rules.range[0]==96 && g->weapon_rules.range[1]==1024 && g->weapon_rules.range[5]==2048);
    muzzle_visibility(g,&renderer);
    ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
    CHECK(ssa_native_game_step(g)==1 && !g->player.pose.third_person);alignment(g,&query);
    CHECK(ssa_weapon_aim_free(&query)==1);ssa_pc_renderer_free(&renderer);ssa_native_game_free(g);
    free(rom);free(saved);free(g);
    printf("{\"result\":\"pass\",\"checks\":%u,\"aligned_views\":%u,\"third_person\":%u,\"collision_fallbacks\":%u,\"weapon_switch_frames\":%u,\"maximum_4k_error_pixels\":%.6f}\n",
        checks,aligned,third,first,switch_frames,maximum_error);return 0;
}
