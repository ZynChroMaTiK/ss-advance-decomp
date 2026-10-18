/* Playability exercise: generates only normal PC input; never changes health,
 * ammo, world bodies, controller state, script counters or damage. */
#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
static int16_t s16(const uint8_t *p){return (int16_t)(p[0]|p[1]<<8);}
static int absn(int n){return n<0?-n:n;}
static unsigned width,height;static int ox,oy;static int *prev,*queue;
static SsaMapCollision collision;
static SsaMapAssets planner;
static SsaWeaponAim sight;
static unsigned route_avoid_hazards;
static int32_t route_clearance;
static int route_step=64;
static int route_goal_gap;
static unsigned route_allow_jump;
static int route_floor_safe(const SsaMapAssets *map,int32_t x,int32_t y,int floor){
 int sector;unsigned texture;
 if(!route_avoid_hazards)return 1;
 sector=ssa_map_sector(map,x,y);
 if(sector<0)return 0;
 texture=map->data[map->sectors+(unsigned)sector*20+10];
 (void)floor;
 return texture!=35 && texture!=255;
}
static int route_body_supported(const SsaMapAssets *map,const SsaBody *body,int escaping){
 int sector;
 if(route_floor_safe(map,body->x,body->y,body->floor))return 1;
 sector=ssa_map_sector(map,body->x,body->y);
 return escaping && sector>=0 && body->floor>s16(map->data+map->sectors+(unsigned)sector*20);
}
static uint8_t *route_edges;static size_t route_cells;static uint64_t route_signature;
static int route_tree_start=-1,route_tree_count;
static void route_graph_free(void){free(route_edges);route_edges=NULL;route_cells=0;route_signature=0;route_tree_start=-1;route_tree_count=0;}
/* Check a grid edge with short moves and floor updates, as the walking
 * player would traverse it. Keep this planning state separate from gameplay. */
static int walk_edge_in(SsaMapCollision *query,SsaBody body,int32_t radius,int32_t x,int32_t y){
 int32_t sx=body.x,sy=body.y;int64_t dx=(int64_t)x-sx,dy=(int64_t)y-sy;
 int escaping=!route_floor_safe(query->map,body.x,body.y,body.floor);
 int64_t length=llabs(dx)>llabs(dy)?llabs(dx):llabs(dy);unsigned n=(unsigned)((length+8*65536-1)/(8*65536)),k;
 if(!n)n=1;
 for(k=1;k<=n;++k){
  if(ssa_map_try_move(query,&body,radius,sx+(int32_t)(dx*k/n),sy+(int32_t)(dy*k/n))!=1){
   int rise=query->floor-body.floor;
   /* Plan a real PC jump over a low step. This changes only the copied body;
    * the input driver must deliver Space before the actual player can cross. */
   if(!route_allow_jump || query->blocking_line>=0 || rise<=48 || rise>96 ||
      query->ceiling-query->floor<body.height/65536)return 0;
   body.z=query->floor*65536;
   if(ssa_map_try_move(query,&body,radius,sx+(int32_t)(dx*k/n),sy+(int32_t)(dy*k/n))!=1)return 0;
  }
  if(!route_body_supported(query->map,&body,escaping))return 0;
  body.z=(int32_t)body.floor*65536;
 }
 return 1;
}
static int walk_edge(SsaBody body,int32_t radius,int32_t x,int32_t y){return walk_edge_in(&collision,body,radius,x,y);}
static int clear_node(SsaBody body,int32_t radius,int32_t x,int32_t y){
 int sector=ssa_map_sector(&planner,x,y);
 if(sector<0)return 0;
 body.x=x;body.y=y;body.z=(int32_t)s16(planner.data+planner.sectors+sector*20)*65536;
 if(route_avoid_hazards){
  if(ssa_map_query(&collision,x,y,x,y,radius)!=1)return 0;
  body.floor=(int16_t)collision.floor;body.z=(int32_t)collision.floor*65536;
  if(!route_floor_safe(&planner,x,y,body.floor))return 0;
 }
 return ssa_map_try_move(&collision,&body,radius,x,y)==1;
}
static int cell(int x,int y){int bx=(x-ox+route_step/2)/route_step,by=(y-oy+route_step/2)/route_step;return bx>=0&&by>=0&&bx<(int)width&&by<(int)height?by*(int)width+bx:-1;}
static int route(SsaNativeGame *g,int tx,int ty,int *outx,int *outy){
 int explore=tx==INT_MAX,start=cell(g->players[0].entity.body.x/65536,g->players[0].entity.body.y/65536),goal=explore?-2:cell(tx,ty),head=0,tail=0,found=-1,best=INT_MAX,reached=0;
 unsigned i;int32_t radius=g->players[0].entity.radius+(g->level>=10?route_clearance:g->level>=3?4*65536:0);static const int dx[]={1,0,-1,0},dy[]={0,1,0,-1};
 route_goal_gap=INT_MAX;
 if(start<0 || (!explore && goal<0))return 0;
 memcpy(planner.data,g->map.data,g->map.size);
 for(i=0;i<g->level_runtime.movers.count;++i){uint8_t *r=g->level_runtime.movers.records[i];unsigned flags=(uint16_t)s16(r+14),sector=(uint16_t)s16(r);
  if(!(flags&(4|256))){uint8_t *sec=planner.data+planner.sectors+sector*20;unsigned top=(unsigned)(s16(sec)+224);sec[2]=(uint8_t)top;sec[3]=(uint8_t)(top>>8);}}
 {uint64_t signature=14695981039346656037ull;size_t cells=(size_t)width*height;SsaEntity *player=&g->players[0].entity;
  uint32_t values[]={g->level,width,height,(uint32_t)ox,(uint32_t)oy,(uint32_t)radius,(uint32_t)route_step,route_avoid_hazards,route_allow_jump,
   (uint32_t)player->body.height,player->body.flags&4u};unsigned j;
  for(j=0;j<sizeof(values)/sizeof(values[0]);++j){signature^=values[j];signature*=1099511628211ull;}
  for(j=0;j<planner.sector_count;++j)for(i=0;i<4;++i){signature^=planner.data[planner.sectors+j*20+i];signature*=1099511628211ull;}
  if(route_cells!=cells){route_graph_free();route_edges=calloc(cells,1);if(!route_edges)return 0;route_cells=cells;}
  if(signature!=route_signature){memset(route_edges,0,cells);route_signature=signature;route_tree_start=-1;}
 }
 {int sx=start%(int)width,sy=start/(int)width,near=-1,bd=INT_MAX,xx,yy;
  for(yy=sy-1;yy<=sy+1;++yy)for(xx=sx-1;xx<=sx+1;++xx){SsaBody b=g->players[0].entity.body;int x=ox+xx*route_step,y=oy+yy*route_step,d=absn(x-b.x/65536)+absn(y-b.y/65536);
   if(xx>=0 && yy>=0 && xx<(int)width && yy<(int)height && d<bd &&
      walk_edge(b,g->players[0].entity.radius,x*65536,y*65536) &&
      (g->level<3 || clear_node(b,radius,x*65536,y*65536))){near=yy*(int)width+xx;bd=d;}}
  if(near<0)return 0;
  start=near;
 }
 if(g->level>=10 && route_tree_start==start){tail=route_tree_count;goto select_route;}
 for(i=0;i<width*height;++i)prev[i]=-1;
 prev[start]=start;queue[tail++]=start;
 while(head<tail){int n=queue[head++],x=ox+(n%(int)width)*route_step,y=oy+(n/(int)width)*route_step,dist=explore?INT_MAX:absn(x-tx)+absn(y-ty),sector=ssa_map_sector(&g->map,x*65536,y*65536);SsaBody b;
  if(sector<0)continue;
  if(g->level<10){
   if(explore){unsigned flags=(uint16_t)s16(g->map.data+g->map.sectors+sector*20+16);if((flags&0x100) && !g->level_runtime.script.visited[sector]){found=n;best=0;reached=1;break;}}
   else {if(dist<best){best=dist;found=n;}if(n==goal){if(g->level>=3)found=n;reached=1;break;}}
  }
  b=g->players[0].entity.body;b.x=x*65536;b.y=y*65536;
  b.floor=s16(planner.data+planner.sectors+sector*20);b.z=(int32_t)b.floor*65536;
  b.ceiling=s16(planner.data+planner.sectors+sector*20+2);
  /* A standing player's floor is the highest touched stair, which can be
   * above the sector under its center. Keep that real collision height. */
  if(route_avoid_hazards){
   if(ssa_map_query(&collision,b.x,b.y,b.x,b.y,radius)!=1)continue;
   b.floor=(int16_t)collision.floor;b.z=(int32_t)b.floor*65536;b.ceiling=(int16_t)collision.ceiling;
  }
  for(i=0;i<4;++i){int nx=n%(int)width+dx[i],ny=n/(int)width+dy[i],nn;SsaBody trial=b;
   if(nx<0||ny<0||nx>=(int)width||ny>=(int)height)continue;
   nn=ny*(int)width+nx;if(prev[nn]>=0)continue;
   if(!(route_edges[n]&(1u<<i))){
    route_edges[n]|=(uint8_t)(1u<<i);
    if(walk_edge(trial,radius,(ox+nx*route_step)*65536,(oy+ny*route_step)*65536))route_edges[n]|=(uint8_t)(16u<<i);
   }
   if(route_edges[n]&(16u<<i)){prev[nn]=n;queue[tail++]=nn;}
  }
 }
 if(g->level>=10){route_tree_start=start;route_tree_count=tail;}
select_route:
 /* A complete BFS tree can answer every goal from this start cell. Keep the
  * original queue order and predecessor ties; only reuse it while geometry,
  * clearance, body dimensions and the start cell still match. */
 if(g->level>=10){
  if(!explore && prev[goal]>=0){found=goal;best=0;reached=1;}
  else for(head=0;head<tail;++head){int n=queue[head],x=ox+n%(int)width*route_step,y=oy+n/(int)width*route_step;
   int sector=ssa_map_sector(&g->map,x*65536,y*65536),dist=explore?INT_MAX:absn(x-tx)+absn(y-ty);
   if(sector<0)continue;
   if(explore){unsigned flags=(uint16_t)s16(g->map.data+g->map.sectors+sector*20+16);
    if((flags&0x100) && !g->level_runtime.script.visited[sector]){found=n;best=0;reached=1;break;}
   }else if(dist<best){best=dist;found=n;}
  }
 }
 if(found<0)return 0;
 route_goal_gap=best;
 if(absn(g->players[0].entity.body.x/65536-(ox+start%(int)width*route_step))+absn(g->players[0].entity.body.y/65536-(oy+start/(int)width*route_step))>route_step/2){found=start;}
 else if(found==start){
  if(g->level>=3 && reached){*outx=explore?ox+start%(int)width*route_step:tx;*outy=explore?oy+start/(int)width*route_step:ty;return 1;}
  return 0;
 }
 while(prev[found]!=start && prev[found]!=found)found=prev[found];
 *outx=ox+found%(int)width*route_step;*outy=oy+found/(int)width*route_step;
 return g->level>=3?(reached?1:2):(best<256?1:2);
}
int main(int argc,char **argv){
 FILE *f=fopen("orig/SSA.gba","rb");uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i,frames,steps=argc>1?(unsigned)atoi(argv[1]):10000,spawned=0,live=0,groups=0;int wx=0,wy=0,mode=0,ok;
 if(!f || !rom || !g || fread(rom,1,0x800000,f)!=0x800000)return 1;
 fclose(f);
 if(ssa_native_game_init(g,rom,0x800000,0)!=1){fprintf(stderr,"init%u\n",g->error_line);return 1;}
 ox=g->world.origin_x/65536;oy=g->world.origin_y/65536;width=g->world.width*2;height=g->world.height*2;
 prev=malloc(width*height*sizeof(int));queue=malloc(width*height*sizeof(int));planner=g->map;planner.data=malloc(g->map.size);memcpy(planner.data,g->map.data,g->map.size);ssa_map_collision_init(&collision,&planner);
 ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
 if(ssa_weapon_aim_init(&sight,&g->world,&g->math)<0 || ssa_pc_input_sensitivity(&g->input,1)<0)return 1;
 for(i=0;i<steps && !g->finished;++i){
  SsaPlayerSpawn *p=&g->players[0];SsaEntity *enemy=NULL,*goal=NULL;unsigned j;int best=INT_MAX,tx=3840,ty=1088,px=p->entity.body.x/65536,py=p->entity.body.y/65536;uint16_t aim;int32_t slope;int fire=0,forward=0,strafe=0;
  for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int d;if(!a->owned)continue;
   d=absn(a->entity.body.x/65536-px)+absn(a->entity.body.y/65536-py);
   if(a->entity.type>=0xcd && a->entity.type<=0xd8 && a->activation.health>0 && d<best){
    uint16_t h=ssa_direction(&g->math,(int16_t)(a->entity.body.x/65536-px),(int16_t)(a->entity.body.y/65536-py));
    if(ssa_weapon_aim_query(&sight,&p->entity,0,0,0x600000,h,2000*65536,&slope)==1 && sight.target==&a->entity){enemy=&a->entity;best=d;}
   }
  }
  /* Acquire the starting pistol and nearest gun before crossing the level. */
  if(!(p->inventory.weapons&2)){goal=&g->spawn.actors[4].entity;}
  else if(!(p->inventory.weapons&4) && g->spawn.actors[5].owned)goal=&g->spawn.actors[5].entity;
  if(!goal && g->score<28000 && (p->inventory.health<80 || p->inventory.stat_c4<25)){
   int gd=INT_MAX;
   for(j=0;j<100;++j){SsaScriptActor *a=&g->spawn.actors[j];int d;
    if(!a->owned || a->entity.type!=0xe1 ||
       !((a->animation>=0x30 && a->animation<=0x32 && p->inventory.health<80) ||
         (a->animation>=0x2d && a->animation<=0x2f && p->inventory.stat_c4<25)))continue;
    d=absn(a->entity.body.x/65536-px)+absn(a->entity.body.y/65536-py);
    if(d<gd){gd=d;goal=&a->entity;}
   }
  }
  /* Finish a visible threat before routing to health. Longer-range shotgun
   * pellets change wave timing; fleeing with a live adjacent enemy can pin
   * the bot against a wall while it stops firing. Inputs remain ordinary. */
  if(enemy && best<1300 && (p->inventory.weapons&2)){
   tx=enemy->body.x/65536;ty=enemy->body.y/65536;
   aim=ssa_direction(&g->math,(int16_t)(tx-px),(int16_t)(ty-py));fire=1;
   forward=best>700;strafe=(i/100)%2?1:-1;mode=1;
  }else{
   if(goal){tx=goal->body.x/65536;ty=goal->body.y/65536;}
   if(i%4==0 || !mode || mode==1){int r=!goal?route(g,INT_MAX,0,&wx,&wy):0;if(!r)r=route(g,tx,ty,&wx,&wy);if(!r){wx=tx;wy=ty;}mode=2;}
   aim=ssa_direction(&g->math,(int16_t)(wx-px),(int16_t)(wy-py));forward=1;
  }
  {int16_t delta=(int16_t)(aim-p->motion.movement.heading);ssa_pc_input_runtime_relative(&g->input_runtime,delta,0);}
  ssa_pc_input_runtime_key(&g->input_runtime,'W',forward,0);ssa_pc_input_runtime_key(&g->input_runtime,'A',strafe<0,0);ssa_pc_input_runtime_key(&g->input_runtime,'D',strafe>0,0);
  ssa_pc_input_runtime_key(&g->input_runtime,'E',i%30==0,0);ssa_pc_input_runtime_button(&g->input_runtime,1,fire);
  if(ssa_native_game_step(g)!=1){fprintf(stderr,"FAIL frame%u stage%x entry%x slot%u line%u\n",i,g->error_stage,g->error_entry,g->error_slot,g->error_line);break;}
  if(i%300==0){unsigned alive=0;for(j=0;j<100;++j)if(g->spawn.actors[j].owned && g->spawn.actors[j].entity.type>=0xcd && g->spawn.actors[j].entity.type<=0xd8 && g->spawn.actors[j].activation.health>0)++alive;
   fprintf(stderr,"f%u pos%d,%d sec%d hp%d weapon%u own%x pickup%u score%u alive%u scripts%u goal%d,%d mode%d aim%u\n",i,px,py,p->entity.sector,p->inventory.health,g->gameplay.weapon.current,p->inventory.weapons,g->pickup.game.collected,g->score,alive,g->level_runtime.script.count,tx,ty,mode,aim);}
  if(!p->inventory.health){fprintf(stderr,"DEAD frame%u\n",i);break;}
 }
 frames=i;
 for(i=0;i<12;++i)spawned+=g->spawn.game.spawned[i];
 for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];
  if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd8 &&
     a->activation.health>0 && !(a->entity.body.flags&0x20))++live;}
 for(i=0;i<sizeof(g->level_runtime.script.alive);++i)if(g->level_runtime.script.alive[i])++groups;
 /* Crowd sliding changes how many kamikazes detonate without player credit.
  * Require earned score, all enemies/groups resolved and the actual VM exit;
  * do not equate a historical player-kill score with level completion. */
 ok=g->finished && !g->error_line && g->score>=2000 && !(g->score%2000) && !live && !groups && g->pickup.game.collected>=2 &&
    g->players[0].inventory.health>0 && spawned==19 && g->level_runtime.script.exit_toggle==1;
 printf("{\"result\":\"%s\",\"frames\":%u,\"finished\":%u,\"score\":%u,\"collected\":%u,\"spawned\":%u,\"health\":%d,\"pistol_shots\":%u,\"shotgun_shots\":%u,\"live_enemies\":%u,\"open_groups\":%u}\n",
  ok?"pass":"fail",frames,g->finished,g->score,g->pickup.game.collected,spawned,g->players[0].inventory.health,g->weapon_game.shots[1],g->weapon_game.shots[2],live,groups);
 ssa_weapon_aim_free(&sight);ssa_native_game_free(g);ssa_map_collision_free(&collision);route_graph_free();free(planner.data);free(prev);free(queue);free(g);free(rom);return ok?0:1;
}
