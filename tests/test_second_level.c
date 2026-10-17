/* Complete level0 with the established normal-input bot, then carry only its
 * earned inventory/score into level1. No grants or world-state shortcuts. */
#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "ssa/actor_raster.h"
#include <stdio.h>
typedef struct CampaignAudio {SsaAudio *mixer;unsigned errors,boss_music,post_boss_music;uint64_t nonzero;} CampaignAudio;
static void campaign_audio_play(void *p,unsigned id){CampaignAudio *a=p;if(ssa_audio_play(a->mixer,id)<0)++a->errors;}
static void campaign_audio_stop(void *p,unsigned id){CampaignAudio *a=p;if(ssa_audio_stop(a->mixer,id)<0)++a->errors;}
static void campaign_audio_source(void *p,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point){
 CampaignAudio *a=p;if(ssa_audio_source(a->mixer,command,id,source,point)<0)++a->errors;
}
static void campaign_audio_music(void *p,unsigned command,unsigned level){CampaignAudio *a=p;
 if(command==19)++a->boss_music;
 if(command==20)++a->post_boss_music;
 if(ssa_audio_music(a->mixer,command,level)<0)++a->errors;
}
static SsaNativeCarry earned;
static int (*original_combat_event)(void *,const SsaCombatRuntime *,const SsaCombatDamageTarget *,SsaCombatDamageEvent,uint32_t,uint32_t);
static int trace_combat(void *p,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,SsaCombatDamageEvent kind,uint32_t a,uint32_t b){
 SsaNativeGame *g=p;
 if(v->inventory && g->level>=3 && kind==SSA_COMBAT_DAMAGE_SOUND){const SsaEntity *s=r->damage->source;
  fprintf(stderr,"map%u hurt f%u health%d source%x at%d,%d owner%x weapon%u\n",g->level,g->frame_game.counter,*v->health,s?s->type:0,s?s->body.x/65536:0,s?s->body.y/65536:0,r->damage->source_owner?r->damage->source_owner->type:0,g->gameplay.weapon.current);
 }
 return original_combat_event(p,r,v,kind,a,b);
}
static void capture_end(SsaNativeGame *g){
    if(g->finished)(void)ssa_native_game_carry_capture(&earned,g);
    ssa_native_game_free(g);
}
#define ssa_native_game_free capture_end
#define main first_level_main
#include "test_native_playability.c"
#undef main
#undef ssa_native_game_free
/* Read-only sight planning for normal vertical mouse input on later maps.
 * Elevated D4 enemies can sit outside the original auto-aim slope window. */
static int pitched_target(SsaNativeGame *g,SsaEntity *target,uint16_t heading,int32_t *pitch){
 SsaEntity *source=&g->players[0].entity;SsaWeaponHit hit;int32_t sine,cosine,dx,dy,dz,distance,slope;unsigned height;
 dx=(target->body.x-source->body.x)/65536;dy=(target->body.y-source->body.y)/65536;
 ssa_sincos(&g->math,heading,&sine,&cosine);
 distance=(int32_t)(((int64_t)dx*sine+(int64_t)dy*cosine)/65536);if(distance<1)distance=1;
 for(height=2;height<=3;++height){
 dz=(target->body.z+(int32_t)((int64_t)target->body.height*height/4)-g->player.camera.eye_z)/65536;
 *pitch=(int16_t)ssa_direction(&g->math,(int16_t)dz,(int16_t)distance);
 if(*pitch>14564)*pitch=14564;
 if(*pitch< -14564)*pitch=-14564;
 ssa_sincos(&g->math,(uint16_t)*pitch,&sine,&cosine);slope=(int32_t)((int64_t)sine*65536/cosine);
 if(ssa_weapon_hit_query_free(&sight,source,heading,g->player.camera.eye_z-source->body.z,8000*65536,slope,&hit)<0)return -1;
 if(hit.target==target)return 1;
 }
 return 0;
}
static void evade(SsaNativeGame *g,SsaMapCollision *query,uint16_t heading,int *forward,int *back,int *strafe){
 SsaEntity *source=&g->players[0].entity;int32_t sine,cosine;int f,s,bf=0,bs=0;unsigned i;
 int64_t closest=INT64_MAX,best;
 for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];int64_t dx,dy,d;int32_t pitch;
  if(!a->owned || a->entity.type<0xcd || a->entity.type>0xd8 || a->activation.health<=0 ||
     llabs((int64_t)a->entity.body.z-source->body.z)>64*65536)continue;
  dx=((int64_t)a->entity.body.x-source->body.x)/65536;dy=((int64_t)a->entity.body.y-source->body.y)/65536;d=dx*dx+dy*dy;
  if(d<700*700 && pitched_target(g,&a->entity,ssa_direction(&g->math,(int16_t)dx,(int16_t)dy),&pitch)!=1)continue;
  if(d<closest)closest=d;
 }
 if(closest>700*700){
  if(g->level>=8 && *strafe){
   int32_t x,y;int travel=*forward-*back;
   ssa_sincos(&g->math,heading,&sine,&cosine);
   x=source->body.x+128*(travel*sine+*strafe*cosine);
   y=source->body.y+128*(travel*cosine-*strafe*sine);
   if(!walk_edge_in(query,source->body,source->radius,x,y)){
    x=source->body.x+128*(travel*sine-*strafe*cosine);
    y=source->body.y+128*(travel*cosine+*strafe*sine);
    if(walk_edge_in(query,source->body,source->radius,x,y))*strafe= -*strafe;
   }
  }
  return;
 }
 best=closest;ssa_sincos(&g->math,heading,&sine,&cosine);
 for(f=-1;f<=1;++f)for(s=-1;s<=1;++s){int32_t x,y;int64_t distance=INT64_MAX;
  if(!f && !s)continue;
  /* A long straight probe can reject the first safe step around a corner.
   * Reassess a short escape every frame instead of standing still there. */
  {int reach=g->level>=8?16:128;
   x=source->body.x+reach*(f*sine+s*cosine);y=source->body.y+reach*(f*cosine-s*sine);
  }
  if(!walk_edge_in(query,source->body,source->radius,x,y))continue;
  for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];int64_t dx,dy,d;int32_t pitch;
   if(!a->owned || a->entity.type<0xcd || a->entity.type>0xd8 || a->activation.health<=0 ||
      llabs((int64_t)a->entity.body.z-source->body.z)>64*65536)continue;
   dx=((int64_t)a->entity.body.x-x)/65536;dy=((int64_t)a->entity.body.y-y)/65536;d=dx*dx+dy*dy;
   if(d<700*700 && pitched_target(g,&a->entity,ssa_direction(&g->math,(int16_t)(a->entity.body.x/65536-source->body.x/65536),(int16_t)(a->entity.body.y/65536-source->body.y/65536)),&pitch)!=1)continue;
   if(d<distance)distance=d;
  }
  if(distance>best){best=distance;bf=f;bs=s;}
 }
 *forward=bf>0;*back=bf<0;*strafe=bs;
}
static int switch_goal(SsaNativeGame *g,int *tx,int *ty,int *ax,int *ay){
 unsigned i;int best=INT_MAX,found=0,px=g->players[0].entity.body.x/65536,py=g->players[0].entity.body.y/65536;
 for(i=0;i<g->level_runtime.switches.count;++i){const SsaSwitchEntry *s=&g->level_runtime.switches.entries[i];
  const uint8_t *l,*a,*b;int mx,my,dx,dy,scale,x,y,d,rx,ry;unsigned side;
  if(s->flags&1)continue;
  l=g->map.data+g->map.lines+s->line*40;a=g->map.data+g->map.vertices+(uint16_t)s16(l)*4;
  b=g->map.data+g->map.vertices+(uint16_t)s16(l+2)*4;
  mx=(s16(a)+s16(b))/2;my=(s16(a+2)+s16(b+2))/2;dx=s16(b)-s16(a);dy=s16(b+2)-s16(a+2);
  scale=absn(dx)>absn(dy)?absn(dx):absn(dy);if(!scale)continue;
  x=mx+dy*48/scale;y=my-dx*48/scale;
  if(ssa_map_sector(&g->map,x*65536,y*65536)!=s16(l+12)){x=mx-dy*48/scale;y=my+dx*48/scale;}
  /* Recesses can be too small/low for Sam to enter. Original interaction
   * spans64 units plus the body's32-unit radius and ignores ceiling height.
   * Try the opposite normal and a farther80-unit use position in the room.
   * This only plans input: E still uses the real original contact query. */
  {int vx=x-mx,vy=y-my;
  for(side=0;side<4;++side){int reach=side<2?48:80,sign=side&1?-1:1;
   x=mx+vx*reach/48*sign;y=my+vy*reach/48*sign;d=absn(x-px)+absn(y-py);
   if((g->level<3 && d<128) || route(g,x,y,&rx,&ry)==1){
    if(d<best){best=d;*tx=x;*ty=y;*ax=mx;*ay=my;found=1;}break;
   }
  }
  }
 }
 return found;
}
/* A closing original door kills on a rotating sample of the player's radius,
 * before it necessarily blocks XY movement. Wait outside that radius and look
 * away so the automatic use ray does not reopen it before its close endpoint
 * clears the hazard. This changes input planning only, never mover state. */
static int closing_door_wait(SsaNativeGame *g,uint16_t *aim,int *forward,int *back,int *strafe){
 SsaEntity *p=&g->players[0].entity;int32_t sine,cosine,x,y,margin=p->radius+24*65536;int dx,dy;unsigned i;
 if(!g->level_runtime.movers.hazard_count || (!*forward && !*back && !*strafe))return 0;
 ssa_sincos(&g->math,*aim,&sine,&cosine);
 x=p->body.x+8*((*forward-*back)*sine+*strafe*cosine);
 y=p->body.y+8*((*forward-*back)*cosine-*strafe*sine);
 for(dy=-1;dy<=1;++dy)for(dx=-1;dx<=1;++dx){
  int sector=ssa_map_sector(&g->map,x+dx*margin,y+dy*margin);
  for(i=0;i<g->level_runtime.movers.hazard_count;++i)if(sector==(int)g->level_runtime.movers.hazard_sectors[i]){
   *forward=*back=*strafe=0;*aim=(uint16_t)(*aim+32768);return 1;
  }
 }
 return 0;
}
/* Keep ordinary combat/escape input away from pit floors too. Looking ahead
 * covers the player's remaining momentum before a change of direction. */
static int pit_path_safe(SsaMapCollision *query,SsaBody body,int32_t x,int32_t y){
 int32_t dx=(x-body.x)/8,dy=(y-body.y)/8;unsigned k;
 int escaping=!route_floor_safe(query->map,body.x,body.y,body.floor);
 for(k=1;k<=8;++k){
  SsaBody next=body;
  if(ssa_map_try_move(query,&next,32*65536,body.x+dx,body.y+dy)!=1){
   SsaBody along_x=body,along_y=body;int mx,my;
   mx=ssa_map_try_move(query,&along_x,32*65536,body.x+dx,body.y);
   my=ssa_map_try_move(query,&along_y,32*65536,body.x,body.y+dy);
   if(mx==1 && !route_body_supported(query->map,&along_x,escaping))return 0;
   if(my==1 && !route_body_supported(query->map,&along_y,escaping))return 0;
   if(mx==1)next=along_x;else if(my==1)next=along_y;else return !escaping;
  }
  body=next;
  if(!route_body_supported(query->map,&body,escaping))return 0;
  body.z=(int32_t)body.floor*65536;
 }
 return route_floor_safe(query->map,body.x,body.y,body.floor);
}
static int jump_floor_safe(SsaNativeGame *g){
 int dx,dy;SsaEntity *p=&g->players[0].entity;
 if(!route_avoid_hazards)return 1;
 for(dx=-1;dx<=1;++dx)for(dy=-1;dy<=1;++dy)
  if(!route_floor_safe(&g->map,p->body.x+dx*128*65536,p->body.y+dy*128*65536,INT16_MIN))return 0;
 return 1;
}
static void avoid_pit_input(SsaNativeGame *g,SsaMapCollision *query,uint16_t aim,int *forward,int *back,int *strafe){
 SsaEntity *p=&g->players[0].entity;int32_t sine,cosine;int f=*forward-*back,s=*strafe,df,ds,bf=0,bs=0,best=INT_MIN;
 if(!route_avoid_hazards || (!f && !s && route_floor_safe(&g->map,p->body.x,p->body.y,p->body.floor)))return;
 ssa_sincos(&g->math,aim,&sine,&cosine);
 if(pit_path_safe(query,p->body,p->body.x+64*(f*sine+s*cosine),p->body.y+64*(f*cosine-s*sine)))return;
 for(df=-1;df<=1;++df)for(ds=-1;ds<=1;++ds){int score=df*f+ds*s;
  if((!df && !ds) || score<=best)continue;
  if(pit_path_safe(query,p->body,p->body.x+64*(df*sine+ds*cosine),p->body.y+64*(df*cosine-ds*sine)) &&
     walk_edge_in(query,p->body,p->radius,p->body.x+16*(df*sine+ds*cosine),p->body.y+16*(df*cosine-ds*sine))){best=score;bf=df;bs=ds;}
 }
 *forward=bf>0;*back=bf<0;*strafe=bs;
}
static int64_t incoming_distance(SsaNativeGame *g,uint16_t heading,int f,int s){
 SsaEntity *player=&g->players[0].entity;int32_t sine,cosine;int64_t best=INT64_MAX;unsigned i,t;
 ssa_sincos(&g->math,heading,&sine,&cosine);
 for(i=0;i<100;++i)if(g->projectiles.active[i]){SsaProjectile *p=&g->projectiles.projectiles[i];SsaBody *b=&p->entity->body;
  int64_t dx=((int64_t)b->x-player->body.x)/65536,dy=((int64_t)b->y-player->body.y)/65536;
  if(p->owner || p->entity->type<0x65 || p->entity->type>0x6b || p->entity->type==0x69 || dx*dx+dy*dy>640*640)continue;
  for(t=1;t<=12;++t){int64_t z=(int64_t)b->z+(int64_t)b->velocity_z*t,x,y,d;
   if(z+16*65536<player->body.z || z-16*65536>(int64_t)player->body.z+player->body.height)continue;
   x=dx+((int64_t)b->velocity_x*t-8*(int64_t)t*(f*(int64_t)sine+s*(int64_t)cosine))/65536;
   y=dy+((int64_t)b->velocity_y*t-8*(int64_t)t*(f*(int64_t)cosine-s*(int64_t)sine))/65536;
   d=x*x+y*y;if(d<best)best=d;
  }
 }
 return best;
}
static void dodge_projectiles(SsaNativeGame *g,SsaMapCollision *query,uint16_t aim,int *forward,int *back,int *strafe){
 SsaEntity *p=&g->players[0].entity;int f=*forward-*back,s=*strafe,df,ds,bf=f,bs=s;int32_t sine,cosine;
 int64_t best;
 if(g->level<10)return;
 best=incoming_distance(g,aim,f,s);if(best>160*160)return;
 ssa_sincos(&g->math,aim,&sine,&cosine);
 for(df=-1;df<=1;++df)for(ds=-1;ds<=1;++ds){int64_t distance;
  if(!df && !ds)continue;
  if(!walk_edge_in(query,p->body,p->radius,p->body.x+32*(df*sine+ds*cosine),p->body.y+32*(df*cosine-ds*sine)))continue;
  if(!pit_path_safe(query,p->body,p->body.x+64*(df*sine+ds*cosine),p->body.y+64*(df*cosine-ds*sine)))continue;
  distance=incoming_distance(g,aim,df,ds);
  if(distance>best && distance-best>16){best=distance;bf=df;bs=ds;}
 }
 *forward=bf>0;*back=bf<0;*strafe=bs;
}
static int carry_checks(const uint8_t *rom){
 SsaNativeGame *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b)),*before=malloc(sizeof(*before));
 SsaNativeCarry carry,bad,unchanged;unsigned i;
 if(!a || !b || !before || ssa_native_game_init(a,rom,0x800000,0)!=1 ||
    ssa_native_game_init(b,rom,0x800000,1)!=1 || ssa_native_game_carry_capture(&carry,a)!=1)return 0;
 for(i=0;i<8;++i){bad=carry;*before=*b;
  switch(i){case 0:bad.weapon=0;break;case 1:bad.inventory.health=0;break;case 2:bad.inventory.weapons=0;break;
   case 3:bad.inventory.ammo[0]=0;break;case 4:bad.inventory.pending_count=11;break;
   case 5:bad.inventory.pending_count=1;bad.inventory.pending_weapons[0]=11;break;
   case 6:bad.third_person=2;break;default:bad.inventory.weapons|=0x8000;break;}
  if(ssa_native_game_carry_apply(b,&bad)!=-1 || memcmp(before,b,sizeof(*b)))return 0;
 }
 memset(&unchanged,0x5a,sizeof(unchanged));bad=unchanged;a->players[0].inventory.health=0;
 if(ssa_native_game_carry_capture(&bad,a)!=-1 || memcmp(&bad,&unchanged,sizeof(bad)))return 0;
 carry.third_person=1;
 if(ssa_native_game_carry_apply(b,&carry)!=1 || b->score!=carry.score ||
    memcmp(&b->players[0].inventory,&carry.inventory,sizeof(SsaInventory)) ||
    b->world.collision.map!=&b->map || b->weapon.controller.inventory!=&b->players[0].inventory ||
    b->players[0].entity.body.x!=-1664*65536 || b->players[0].entity.body.y!=5120*65536)return 0;
 if(ssa_native_game_tick(b)!=1 || !b->player.view.third_person)return 0;
 *before=*b;if(ssa_native_game_carry_apply(b,&carry)!=-1 || memcmp(before,b,sizeof(*b)))return 0;
 ssa_native_game_free(a);ssa_native_game_free(b);free(a);free(b);free(before);return 1;
}
static int campaign_level_run(const uint8_t *rom,unsigned level,const SsaNativeCarry *start,SsaNativeCarry *end){
 SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i,j,spawned=0,rendered=0,airborne=0,chase=0,pitched=0,activated=0,start_locked,start_opened=0;SsaPcRenderer renderer={0};SsaMapCollision combat_route={0};
 int wx=0,wy=0,mode=0,ok,exit_x=0,exit_y=0;
 SsaEntity *navigation_goal=NULL,*combat_target=NULL;
 uint32_t progress_score=start->score;unsigned progress_pickups=0,quiet_frames=0,explore_ticks=0;
 int last_x=INT_MAX,last_y=INT_MAX;unsigned stalled=0,escape_ticks=0,escapes=0;uint16_t escape_heading=0;
 unsigned boss_frames=0,boss_hud_frames=0,boss_projectile_frames=0,boss_hurt_frames=0,boss_wait_frames=0,boss_defeated=0;
 unsigned conversion_frames=0,door_wait_frames=0,falling_effect_frames=0;
 CampaignAudio audio={0};
 if(!g || ssa_native_game_init(g,rom,0x800000,level)!=1 || ssa_native_game_carry_apply(g,start)!=1)return 1;
 route_avoid_hazards=level>=10;
 route_step=level>=10?16:64;
 original_combat_event=g->combat_ops.event;g->combat_ops.event=trace_combat;
 if(memcmp(&g->players[0].inventory,&start->inventory,sizeof(SsaInventory)) || g->score!=start->score ||
    g->gameplay.weapon.current!=start->weapon || ssa_pc_renderer_resize(&renderer,640,480)!=1)return 1;
 fprintf(stderr,"map%u entry weapons%x ammo",level,start->inventory.weapons);
 for(j=0;j<10;++j)fprintf(stderr," %d",start->inventory.ammo[j]);
 fprintf(stderr," hp%d armor%d\n",start->inventory.health,start->inventory.stat_c4);
 for(j=0;j<100;++j)if(g->spawn.actors[j].owned && g->spawn.actors[j].entity.type==0xe1 && g->spawn.actors[j].animation==0x3f){
  exit_x=g->spawn.actors[j].entity.body.x/65536;exit_y=g->spawn.actors[j].entity.body.y/65536;break;}
 /* Boss maps can create their exit through the original script later. */
 if(j==100){exit_x=g->players[0].entity.body.x/65536;exit_y=g->players[0].entity.body.y/65536;}
 /* Exercise the real first boss and later second-family levels with the
  * native mixer attached, including overlapping enemy/door/player sounds. */
 if(level==5 || level>=7){SsaNativeAudioOps ops={campaign_audio_play,campaign_audio_stop,&audio,NULL,NULL,NULL,campaign_audio_music,campaign_audio_source};
  audio.mixer=calloc(1,sizeof(*audio.mixer));
  if(!audio.mixer || ssa_audio_init(audio.mixer,rom,0x800000,48000)!=1 ||
     ssa_audio_level_music(audio.mixer,level)!=1 || ssa_native_game_bind_audio(g,&ops)!=1)return 1;
 }
 if(!ssa_map_collision_init(&combat_route,&g->map))return 1;
 start_locked=level==3 && g->level_runtime.movers.count && ((unsigned)(uint16_t)s16(g->level_runtime.movers.records[0]+14)&4);
 ox=g->world.origin_x/65536;oy=g->world.origin_y/65536;width=g->world.width*(128/route_step);height=g->world.height*(128/route_step);
 prev=malloc(width*height*sizeof(int));queue=malloc(width*height*sizeof(int));planner=g->map;
 planner.data=malloc(g->map.size);memcpy(planner.data,g->map.data,g->map.size);ssa_map_collision_init(&collision,&planner);
 ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
 if(ssa_weapon_aim_init(&sight,&g->world,&g->math)<0 || ssa_pc_input_sensitivity(&g->input,1)<0)return 1;
 for(i=0;i<30000 && !g->finished;++i){
  SsaPlayerSpawn *p=&g->players[0];SsaEntity *enemy=NULL,*goal=NULL,*elevated=NULL;int best=INT_MAX,high_distance=INT_MAX,tx=exit_x,ty=exit_y;
  int px=p->entity.body.x/65536,py=p->entity.body.y/65536,fire=0,forward=0,back=0,strafe=0,button=0,ax=0,ay=0,retreat_supply=0;uint16_t aim;int32_t slope,aim_pitch=0;
  if(level>=5)for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];
   if(a->owned && a->entity.type==0xe1 && a->animation==0x3f){exit_x=a->entity.body.x/65536;exit_y=a->entity.body.y/65536;tx=exit_x;ty=exit_y;break;}
  }
  /* Same owned-weapon selection API as Windows number keys. Prefer an earned
   * automatic gun, then shotgun, then pistol; reassess after ammo pickups. */
  for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int d;if(!a->owned)continue;
   d=absn(a->entity.body.x/65536-px)+absn(a->entity.body.y/65536-py);
   if(level>=3 && a->entity.type>=0xcd && a->entity.type<=0xd8 && a->activation.health>0 &&
      (int64_t)a->entity.body.z-p->entity.body.z>48*65536 && d<high_distance){elevated=&a->entity;high_distance=d;}
   if(a->entity.type>=0xcd && a->entity.type<=0xd8 && a->activation.health>0 &&
      !(level>=8 && (a->entity.body.flags&0x400) && d>=700) && d<best){
    uint16_t h=ssa_direction(&g->math,(int16_t)(a->entity.body.x/65536-px),(int16_t)(a->entity.body.y/65536-py));
    if(level>=3){int32_t pitch;int visible=pitched_target(g,&a->entity,h,&pitch);
     if(visible<0){fprintf(stderr,"map%u sight failure frame%u target slot%u\n",level,i,j);return 1;}
     if(visible){enemy=&a->entity;best=d;aim_pitch=pitch;}
    }else if(ssa_weapon_aim_query(&sight,&p->entity,0,0,0x600000,h,2000*65536,&slope)==1 && sight.target==&a->entity){enemy=&a->entity;best=d;}
   }
  }
  if(level>=10 && combat_target && best>=384){
   for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int32_t pitch;int dx,dy;
    if(!a->owned || &a->entity!=combat_target || a->entity.type<0xcd || a->entity.type>0xd8 ||
       a->activation.health<=0 || (a->entity.body.flags&(0x400|0x800)))continue;
    dx=a->entity.body.x/65536-px;dy=a->entity.body.y/65536-py;
    if(pitched_target(g,&a->entity,ssa_direction(&g->math,(int16_t)dx,(int16_t)dy),&pitch)==1){
     enemy=&a->entity;best=absn(dx)+absn(dy);aim_pitch=pitch;
    }
   }
  }
  combat_target=enemy;
  {unsigned desired=2,k,cluster=0;
   for(k=3;k<=5;++k)if((p->inventory.weapons&(1u<<(k-1))) && p->inventory.ammo[k-1]>0)desired=k;
   /* Use an earned shotgun against nearby targets: a single blast kills a
    * charging CD, whereas even the faster minigun still needs two rounds. */
   if(level>=3 && level<6 && enemy && best<700 && (p->inventory.weapons&4) && p->inventory.ammo[2]>=2)desired=3;
   /* This planner does not predict a rocket's muzzle clearance during the
    * delayed weapon handoff. Use earned hitscan/special weapons instead of
    * firing rockets into a nearby wall after a target disappears. */
   if(level>=5 && enemy && (((p->inventory.weapons&128) && p->inventory.ammo[7]>0) || level>=10)){
    for(k=0;k<100;++k){SsaScriptActor *a=&g->spawn.actors[k];
     if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd8 && a->activation.health>0 &&
       llabs((int64_t)a->entity.body.x-enemy->body.x)+llabs((int64_t)a->entity.body.y-enemy->body.y)<384*65536)++cluster;
    }
    if(cluster>=4 && best<1200 && (p->inventory.weapons&128) && p->inventory.ammo[7]>0)desired=8;
   }
   if(level>=6 && enemy && enemy->type<=0xd6 && !(enemy->body.flags&0x400) &&
      (p->inventory.weapons&64) && p->inventory.ammo[6]>0 &&
      (g->weapon_game.shots[6]<2 || (level>=8 && enemy->type==0xd6)) && best>300 && best<(level>=8?2400:1200))desired=7;
   if(level>=8 && enemy && enemy->type>=0xd0 && !(enemy->body.flags&0x800) &&
      best>400 && (p->inventory.weapons&256) && p->inventory.ammo[8]>0)desired=9;
   if(g->gameplay.weapon.requested!=desired && ssa_native_game_select_weapon(g,desired)!=1){fprintf(stderr,"map%u select failure frame%u weapon%u ammo%d\n",level,i,desired,p->inventory.ammo[desired-1]);return 1;}
  }
  {int distance=INT_MAX;
   for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int d,rx,ry,gx,gy;
    /* Some original maps unlock the start door through a pickup's script,
     * even when its health/ammo/armor benefit is currently unnecessary. */
    if(!a->owned || a->entity.type!=0xe1 || !((a->script.trigger&0x100) ||
       (level>=5 && a->animation==0x27 && !(p->inventory.weapons&128)) ||
       (level>=6 && a->animation==0x29 && !(p->inventory.weapons&64)) ||
       (((a->animation>=0x30 && a->animation<=0x32) || a->animation==0x37) && p->inventory.health<80) ||
       (a->animation>=0x2d && a->animation<=0x2f && p->inventory.stat_c4<25)))continue;
    gx=a->entity.body.x/65536;gy=a->entity.body.y/65536;d=absn(gx-px)+absn(gy-py);
    if(level>=6 && p->inventory.health<50 && ((a->animation>=0x30 && a->animation<=0x32) || a->animation==0x37))d-=20000;
    if(d<distance && ((level<3 && d<128) || route(g,gx,gy,&rx,&ry)==1)){distance=d;goal=&a->entity;}
   }
  }
  if(!goal && !enemy){int distance=INT_MAX;
   for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int d,rx,ry,gx,gy;
    if(!a->owned || a->entity.type<0xcd || a->entity.type>0xd8 || a->activation.health<=0 ||
       (level>=8 && (a->entity.body.flags&0x400)))continue;
    gx=a->entity.body.x/65536;gy=a->entity.body.y/65536;d=absn(gx-px)+absn(gy-py);
    if(d<distance && ((level<3 && d<128) || route(g,gx,gy,&rx,&ry)==1)){distance=d;goal=&a->entity;}
   }
  }
  /* Nearest-enemy routing can alternate between two doors around one wall.
   * Keep one live destination until combat or a necessary supply detour
   * interrupts it. This only chooses input; the original world stays intact. */
  if(enemy)navigation_goal=NULL;
  else if(!goal || goal->type!=0xe1){int valid=0;
   for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];
    if(a->owned && &a->entity==navigation_goal && a->entity.type>=0xcd && a->entity.type<=0xd8 &&
       a->activation.health>0 && !(level>=8 && (a->entity.body.flags&0x400)))valid=1;
   }
   if(valid)goal=navigation_goal;
   else navigation_goal=goal && goal->type>=0xcd && goal->type<=0xd8?goal:NULL;
  }
  /* A visible enemy behind inaccessible geometry need not block exploration
   * forever. After15seconds without a kill/pickup, try real unvisited script
   * sectors for4seconds. Close threats still interrupt this ordinary walk. */
  if(g->score!=progress_score || g->pickup.game.collected!=progress_pickups){
   progress_score=g->score;progress_pickups=g->pickup.game.collected;quiet_frames=0;
  }else ++quiet_frames;
  if(level>=5 && quiet_frames>=900 && (!enemy || best>=384)){explore_ticks=240;quiet_frames=0;++escapes;}
  if(explore_ticks && (!enemy || best>=384)){
   --explore_ticks;enemy=goal=elevated=navigation_goal=NULL;
  }
  if(enemy && (level>=3 || best<1300)){tx=enemy->body.x/65536;ty=enemy->body.y/65536;
   aim=ssa_direction(&g->math,(int16_t)(tx-px),(int16_t)(ty-py));fire=1;forward=best>(level>=5?1500:700);strafe=(i/100)%2?1:-1;mode=1;
   if(level>=3 && best<(level>=5?900:400)){forward=0;back=1;}
   if(level>=3 && (int64_t)enemy->body.z-p->entity.body.z>48*65536){
    /* Leave enough clearance for the delayed first round and camera bob,
     * then hold a steady elevated shot rather than strafing under the lip. */
    strafe=0;forward=0;if(best<650){fire=0;back=1;}
   }
  }else if(!enemy && !goal && elevated && high_distance<700){
   /* Step away from a ledge to get a clear sight line above its near edge.
    * Walking beneath an elevated enemy leaves it occluded by the floor. */
   aim=ssa_direction(&g->math,(int16_t)(elevated->body.x/65536-px),(int16_t)(elevated->body.y/65536-py));
   back=1;mode=1;
  }else{
   if(goal){tx=goal->body.x/65536;ty=goal->body.y/65536;}
   else button=switch_goal(g,&tx,&ty,&ax,&ay);
   if((level<3?i%4==0:i%60==0 || absn(wx-px)+absn(wy-py)<12) || !mode || mode==1){int result=!goal && !button?route(g,INT_MAX,0,&wx,&wy):0;
    if(!result)result=route(g,tx,ty,&wx,&wy);
    if(!result){wx=tx;wy=ty;}mode=2;}
   aim=ssa_direction(&g->math,(int16_t)(wx-px),(int16_t)(wy-py));forward=1;
   if(button && absn(tx-px)+absn(ty-py)<128){
    /* Finish the sub-cell approach, then face the wall inside E's64-unit
     * query. Stopping a grid step short can leave the use ray out of reach. */
    if(absn(tx-px)+absn(ty-py)<8){aim=ssa_direction(&g->math,(int16_t)(ax-px),(int16_t)(ay-py));forward=0;}
    else aim=ssa_direction(&g->math,(int16_t)(tx-px),(int16_t)(ty-py));
   }
  }
  /* A shot can see over a waist-high wall that the walking body cannot
   * cross. Follow the actual ground route while keeping the sight on the
   * target, rather than repeatedly walking into that visible obstruction. */
  if(enemy && forward){int32_t sine,cosine;int rx,ry;
   ssa_sincos(&g->math,aim,&sine,&cosine);
   if(!walk_edge_in(&combat_route,p->entity.body,p->entity.radius,p->entity.body.x+128*sine,p->entity.body.y+128*cosine)){
    int reachable=route(g,tx,ty,&rx,&ry);
    if(reachable!=1)reachable=route(g,INT_MAX,0,&rx,&ry);
    if(reachable){uint16_t travel=ssa_direction(&g->math,(int16_t)(rx-px),(int16_t)(ry-py));
     ssa_sincos(&g->math,(uint16_t)(travel-aim),&sine,&cosine);
     forward=cosine>16384;back=cosine< -16384;strafe=sine>16384?1:sine< -16384?-1:0;
    }
   }
  }
  if(enemy && goal && p->inventory.health<50){int gx=goal->body.x/65536,gy=goal->body.y/65536;
   if(route(g,gx,gy,&wx,&wy)==1){int32_t sine,cosine;uint16_t travel=ssa_direction(&g->math,(int16_t)(wx-px),(int16_t)(wy-py));
    ssa_sincos(&g->math,(uint16_t)(travel-aim),&sine,&cosine);
    forward=cosine>16384;back=cosine< -16384;strafe=sine>16384?1:sine< -16384?-1:0;retreat_supply=1;
   }
  }
  if(enemy && fire && !retreat_supply)evade(g,&combat_route,aim,&forward,&back,&strafe);
  /* The test planner's grid node can sit across a corner from the player's
   * current sub-cell position. Recover using ordinary movement; never move
   * the body or change geometry to make a planned edge succeed. */
  if(level>=3 && !enemy && mode==2){
   if(i%120==0){stalled=last_x!=INT_MAX && absn(px-last_x)+absn(py-last_y)<96;last_x=px;last_y=py;}
   if(stalled && !escape_ticks){unsigned direction,reach;int distance=INT_MAX,found=0;
    for(reach=128;reach>=8 && !found;reach/=4)for(direction=0;direction<8;++direction){int32_t sine,cosine,x,y;int d;
     if(direction==0)continue;
     uint16_t heading=(uint16_t)((aim+direction*8192)&0xe000);
     ssa_sincos(&g->math,heading,&sine,&cosine);
     x=p->entity.body.x+sine*(int)reach;y=p->entity.body.y+cosine*(int)reach;
     if(!walk_edge_in(&combat_route,p->entity.body,p->entity.radius,x,y))continue;
     d=absn(x/65536-tx)+absn(y/65536-ty);
     if(d<distance){distance=d;escape_heading=heading;found=1;}
    }
    if(found){escape_ticks=48;++escapes;fprintf(stderr,"map%u route escape%u frame%u at%d,%d heading%u\n",level,escapes,i,px,py,escape_heading);}
    else{const uint8_t *sector=g->map.data+g->map.sectors+p->entity.sector*20;
     fprintf(stderr,"map%u route escape blocked frame%u at%d,%d floor%d ceiling%d flags%x line%d\n",level,i,px,py,s16(sector),s16(sector+2),p->entity.body.flags,combat_route.blocking_line);
     escape_heading=(uint16_t)(aim+(++escapes%8)*8192);escape_ticks=24;
    }
    stalled=0;
   }
   if(escape_ticks){--escape_ticks;aim=escape_heading;forward=1;back=strafe=0;}
  }else{stalled=0;escape_ticks=0;}
  avoid_pit_input(g,&combat_route,aim,&forward,&back,&strafe);
  dodge_projectiles(g,&combat_route,aim,&forward,&back,&strafe);
  door_wait_frames+=(unsigned)closing_door_wait(g,&aim,&forward,&back,&strafe);
  ssa_pc_input_runtime_relative(&g->input_runtime,(int16_t)(aim-p->motion.movement.heading),level>=3?g->player.pitch-(fire?aim_pitch:0):0);
  ssa_pc_input_runtime_key(&g->input_runtime,'W',forward,0);ssa_pc_input_runtime_key(&g->input_runtime,'A',strafe<0,0);ssa_pc_input_runtime_key(&g->input_runtime,'D',strafe>0,0);
  ssa_pc_input_runtime_key(&g->input_runtime,'S',back,0);
  ssa_pc_input_runtime_key(&g->input_runtime,'E',i%30==0,0);ssa_pc_input_runtime_button(&g->input_runtime,1,fire);
  ssa_pc_input_runtime_key(&g->input_runtime,' ',((enemy && best<1300 && jump_floor_safe(g) &&
      (level<3 || (int64_t)enemy->body.z-p->entity.body.z<=48*65536))) && i%(level>=8?36:90)<4,0);
  if(level>=2)ssa_pc_input_runtime_key(&g->input_runtime,'H',i%720==120,0);
  if(ssa_native_game_tick(g)!=1){SsaEntity *target=g->error_slot<100?g->enemies.navigation[g->error_slot].target:NULL;
   fprintf(stderr,"second FAIL %u stage%x entry%x slot%u line%u target%p type%x owner%p active%u\n",i,g->error_stage,g->error_entry,g->error_slot,g->error_line,(void *)target,target?target->type:0,target?(void *)target->owner:NULL,g->spawn.pool.active_count);break;}
  if(audio.mixer){int16_t pcm[1600];
   if(ssa_audio_render(audio.mixer,pcm,800)!=1 || audio.mixer->errors || audio.errors){fprintf(stderr,"map%u audio failure frame%u\n",level,i);break;}
   for(j=0;j<1600;++j)audio.nonzero+=pcm[j]!=0;
  }
  for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];
   if(a->owned && (a->entity.type==0xd7 || a->entity.type==0xd8) && a->activation.state){
    ++boss_frames;
    boss_hurt_frames+=a->activation.health>0 && a->activation.health<3000;
    boss_wait_frames+=a->activation.state==5;
    if(a->activation.state==7 && a->group_released && a->activation.health<=0)boss_defeated=1;
    if(g->hud.boss_present && g->hud.boss_health==a->activation.health)++boss_hud_frames;
   }
   if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd6 && (a->entity.body.flags&0x400))++conversion_frames;
   if(g->projectiles.active[j]){const SsaProjectile *shot=&g->projectiles.projectiles[j];
    if(shot->entity->type==0xe3)++falling_effect_frames;
    if(shot->entity->type==0x6a && shot->source && (shot->source->type==0xd7 || shot->source->type==0xd8))++boss_projectile_frames;
   }
  }
  airborne+=p->entity.body.z>(int32_t)p->entity.body.floor*65536;
  chase+=g->player.view.third_person!=0;
  pitched+=g->player.pitch!=0;
  if(level==3){unsigned sector=(uint16_t)s16(g->level_runtime.movers.records[0]);const uint8_t *s=g->map.data+g->map.sectors+sector*20;
   if(s16(s+2)-s16(s)>=p->entity.body.height/65536)start_opened=1;
  }
  if(i%180==0){int32_t pitch;
   if(ssa_native_game_draw(g)!=1){SsaActorQueue *q=&g->actor_scene.queue;uint8_t scratch[14400]={0};
    fprintf(stderr,"map%u reference render failure frame%u projections%u actors%u clips%u\n",level,i,q->projection_count,q->actor_count,q->clip_cursor);
    for(j=0;j<q->projection_count;++j){SsaActorProjection *v=&q->projections[j];
     fprintf(stderr," projection%u anim%u image%x xy%d,%d wh%d,%d scale%d left%d width%d raster%d\n",j,v->animation,v->frame_flags,v->x,v->y,v->width,v->height,v->scale,v->left,v->clipped_width,
      ssa_actor_raster_draw(&g->actor_assets,v,q->clip_words+v->clip_offset,SSA_ACTOR_CLIP_WORDS-v->clip_offset,scratch,sizeof scratch));
    }return 1;}
   if(ssa_pc_renderer_draw(&renderer,g)!=1){fprintf(stderr,"map%u PC render failure frame%u\n",level,i);return 1;}
   ++rendered;pitch=g->render_pitch;g->render_pitch=5440;
   if(ssa_pc_renderer_draw(&renderer,g)!=1){fprintf(stderr,"map%u pitched render failure frame%u\n",level,i);return 1;}
   ++rendered;g->render_pitch=pitch;
  }
  if(i%300==0)fprintf(stderr,"map%u f%u pos%d,%d sec%d hp%d ammo%d/%d score%u scripts%u goal%d,%d waypoint%d,%d z%d floor%d wall%d\n",level,i,px,py,p->entity.sector,p->inventory.health,p->inventory.ammo[1],p->inventory.ammo[2],g->score,g->level_runtime.script.count,tx,ty,wx,wy,p->entity.body.z/65536,p->entity.body.floor,g->world.collision.blocking_line);
  if(level>=3 && i%300==0 && g->world.blocking_entity){SsaEntity *b=g->world.blocking_entity;fprintf(stderr," blocker%x at%d,%d z%d flags%x\n",b->type,b->body.x/65536,b->body.y/65536,b->body.z/65536,b->body.flags);}
  if(!p->inventory.health){const uint8_t *sector=g->map.data+g->map.sectors+p->entity.sector*20;
   fprintf(stderr,"map%u death frame%u at%d,%d sector%d floor_texture%u z%d floor%d hazards",level,i,p->entity.body.x/65536,p->entity.body.y/65536,p->entity.sector,sector[10],p->entity.body.z/65536,p->entity.body.floor);
   for(j=0;j<g->level_runtime.movers.hazard_count;++j)fprintf(stderr," %u",g->level_runtime.movers.hazard_sectors[j]);
   fprintf(stderr,"\n");break;
  }
 }
 for(j=0;j<12;++j)spawned+=g->spawn.game.spawned[j];
 for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd8 && a->activation.health>0)
  fprintf(stderr,"alive slot%u type%x group%u health%d at%d,%d z%d floor%d state%u flags%x\n",j,a->entity.type,a->script.trigger,a->activation.health,a->entity.body.x/65536,a->entity.body.y/65536,a->entity.body.z/65536,a->entity.body.floor,a->activation.state,a->entity.body.flags);}
 for(j=0;j<g->level_runtime.switches.count;++j){SsaSwitchEntry *s=&g->level_runtime.switches.entries[j];
  const uint8_t *line=g->map.data+g->map.lines+s->line*40,*v=g->map.data+g->map.vertices+(unsigned)(uint16_t)s16(line)*4;
  fprintf(stderr,"switch%u line%u flags%x trigger%u at%d,%d\n",j,s->line,s->flags,s->trigger,s16(v),s16(v+2));}
 for(j=0;j<g->map.sector_count;++j)if(((unsigned)(uint16_t)s16(g->map.data+g->map.sectors+j*20+16)&0x100) && !g->level_runtime.script.visited[j])fprintf(stderr,"unvisited sector%u flags%x\n",j,(uint16_t)s16(g->map.data+g->map.sectors+j*20+16));
 ok=g->finished && !g->error_line && g->players[0].inventory.health>0 && g->level_runtime.script.exit_toggle==1;
 if(level==2 && (!airborne || !chase || chase==i || !g->weapon_game.shots[3] || g->level_runtime.switches.count!=2))ok=0;
 if(level>=3 && (!airborne || !chase || chase==i || !pitched))ok=0;
 if(level==3 && (!start_locked || !start_opened))ok=0; /* Script OPEN does not clear the original lock bit. */
 for(j=0;j<g->level_runtime.switches.count;++j)if(g->level_runtime.switches.entries[j].flags&1)++activated;
 if(level<3 && activated!=g->level_runtime.switches.count)ok=0;
 if(level==3 && activated<2)ok=0; /* The third switch is not required by the original exit. */
 if(level==5 && (!boss_frames || !boss_hud_frames || !boss_projectile_frames || !boss_hurt_frames || !boss_defeated))ok=0;
 if(level==5 && (!audio.boss_music || !audio.post_boss_music))ok=0;
 if(level==8 && (!g->weapon_game.shots[8] || !falling_effect_frames || !conversion_frames || !door_wait_frames))ok=0;
 if(audio.mixer && (!audio.nonzero || audio.errors || audio.mixer->errors || audio.mixer->frames!=(uint64_t)i*800))ok=0;
 if(!ok && level>=3){SsaPlayerSpawn *p=&g->players[0];SsaMapCollision c={0};SsaBody b=p->entity.body;int r;
  if(ssa_map_collision_init(&c,&g->map)){
   r=ssa_map_try_move(&c,&b,p->entity.radius,p->entity.body.x+8*65536,p->entity.body.y);
   fprintf(stderr,"diagnostic east%d floor%d ceil%d low%d line%d radius%d height%d flags%x controls%x move%x velocity%d,%d heading%u\n",r,c.floor,c.ceiling,c.low_floor,c.blocking_line,p->entity.radius/65536,p->entity.body.height/65536,p->entity.body.flags,g->input_runtime.sample.keys,p->motion.movement.flags,p->entity.body.velocity_x,p->entity.body.velocity_y,p->motion.movement.heading);
   {int32_t sine,cosine;ssa_sincos(&g->math,p->motion.movement.heading,&sine,&cosine);b=p->entity.body;
    r=ssa_map_try_move(&c,&b,p->entity.radius,b.x+8*sine,b.y+8*cosine);
    fprintf(stderr,"diagnostic forward%d floor%d ceil%d low%d line%d bodyfloor%d z%d motionflags%x weaponflags%x gameflags%x\n",r,c.floor,c.ceiling,c.low_floor,c.blocking_line,p->entity.body.floor,p->entity.body.z/65536,p->motion.movement.flags,g->gameplay.weapon.flags,g->frame_game.flags);
   }
   ssa_map_collision_free(&c);
  }
  for(j=0;j<100;++j)if(g->spawn.actors[j].owned && ssa_entity_contact(&p->entity,&g->spawn.actors[j].entity,p->entity.body.x+8*65536,p->entity.body.y)==0)fprintf(stderr,"diagnostic entity blocker slot%u type%x\n",j,g->spawn.actors[j].entity.type);
 }
 if(ok && ssa_native_game_carry_capture(end,g)!=1)ok=0;
 printf("{\"result\":\"%s\",\"level\":%u,\"frames\":%u,\"finished\":%u,\"score\":%u,\"collected\":%u,\"spawned\":%u,\"health\":%d,\"rendered_views\":%u,\"carry_guards\":%u,\"airborne_frames\":%u,\"third_person_frames\":%u,\"pitched_frames\":%u,\"machinegun_shots\":%u,\"activated_switches\":%u,\"boss_frames\":%u,\"boss_hud_frames\":%u,\"boss_projectile_frames\":%u,\"boss_hurt_frames\":%u,\"boss_wait_frames\":%u,\"boss_defeated\":%u,\"boss_music\":%u,\"post_boss_music\":%u,\"mixed_frames\":%llu,\"audio_errors\":%u,\"owned_weapons\":%u,\"conversion_shots\":%u,\"conversion_frames\":%u,\"navigation_recoveries\":%u,\"closing_door_wait_frames\":%u,\"meteor_shots\":%u,\"falling_effect_frames\":%u}\n",ok?"pass":"fail",level,i,g->finished,g->score,g->pickup.game.collected,spawned,g->players[0].inventory.health,rendered,level==1?10u:0u,airborne,chase,pitched,g->weapon_game.shots[3],activated,boss_frames,boss_hud_frames,boss_projectile_frames,boss_hurt_frames,boss_wait_frames,boss_defeated,audio.boss_music,audio.post_boss_music,(unsigned long long)(audio.mixer?audio.mixer->frames:0),audio.errors+(audio.mixer?audio.mixer->errors:0),g->players[0].inventory.weapons,g->weapon_game.shots[6],conversion_frames,escapes,door_wait_frames,g->weapon_game.shots[8],falling_effect_frames);
 if(level==5)fprintf(stderr,"boss evidence frames%u HUD%u projectiles%u hurt%u wait%u defeated%u music%u/%u\n",boss_frames,boss_hud_frames,boss_projectile_frames,boss_hurt_frames,boss_wait_frames,boss_defeated,audio.boss_music,audio.post_boss_music);
 fprintf(stderr,"map%u closing-door input waits%u\n",level,door_wait_frames);
 ssa_pc_renderer_free(&renderer);ssa_weapon_aim_free(&sight);ssa_map_collision_free(&combat_route);ssa_native_game_free(g);free(audio.mixer);ssa_map_collision_free(&collision);route_graph_free();free(planner.data);free(prev);free(queue);free(g);return ok?0:1;
}
static int second_level_run(void){
 FILE *f;uint8_t *rom=malloc(0x800000);int result;char *args[]={"first-level",NULL};
 if(first_level_main(1,args) || !rom || !(f=fopen("orig/SSA.gba","rb")))return 1;
 if(fread(rom,1,0x800000,f)!=0x800000)return 1;
 fclose(f);if(!carry_checks(rom))return 1;
 result=campaign_level_run(rom,1,&earned,&earned);free(rom);return result;
}
#ifndef SSA_SECOND_LEVEL_EMBED
int main(void){return second_level_run();}
#endif
