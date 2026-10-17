#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,depth_checks,views;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"free look line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static unsigned rd(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int height(const uint8_t *p){unsigned v=rd(p);return v<32768?(int)v:(int)v-65536;}
static double absolute(double v){return v<0?-v:v;}
static unsigned hud_pixels;
static unsigned flat_sky_pixels,sky_top_fill,sky_bottom_fill,sky_artwork;
static uint32_t color888(unsigned n){unsigned r=n&31,g=(n>>5)&31,b=(n>>10)&31;
 return (((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));}
static void verify_original_hud(SsaPcRenderer *r,SsaNativeGame *g)
{
 uint8_t world[14400]={0};uint16_t black[256]={0},white[256],a[38400],b[38400];unsigned x,y;
 double scale=r->width/240.0<r->height/160.0?r->width/240.0:r->height/160.0;
 double left=(r->width-240*scale)/2,top=(r->height-160*scale)/2;
 for(x=0;x<256;++x)white[x]=32767;
 CHECK(ssa_display_compose_gameplay(&g->registers,world,sizeof(world),black,g->sprite_assets.palette,&g->oam,g->cache.tiles,sizeof(g->cache.tiles),a,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)==1);
 CHECK(ssa_display_compose_gameplay(&g->registers,world,sizeof(world),white,g->sprite_assets.palette,&g->oam,g->cache.tiles,sizeof(g->cache.tiles),b,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)==1);
 CHECK(ssa_pc_renderer_draw(r,g)==1);
 for(y=0;y<r->height;++y)for(x=0;x<r->width;++x){double sx=(x+0.5-left)/scale,sy=(y+0.5-top)/scale;unsigned at;
  if(sx<0 || sx>=240 || sy<0 || sy>=160)continue;
  at=(unsigned)sy*240+(unsigned)sx;
  if(a[at]==b[at]){CHECK(r->pixels[(size_t)y*r->width+x]==color888(a[at]));++hud_pixels;}}
}
/* Independent, deliberately slow per-pixel reference. Follow a 3D ray across
 * sector portals, intersecting planes before each nearest line. No renderer
 * polygons, triangle interpolation or BSP floor clipping are reused. */
static double reference(const SsaNativeGame *g,unsigned w,unsigned h,unsigned x,unsigned y)
{
 const SsaMapAssets *m=&g->map;int32_t si,co,ps,pc;unsigned step,i;
 double s,c,p,q,yn,pn,forward,v,u,dx,dy,dz,cx=g->pc_camera.x/65536.0;
 double cy=g->pc_camera.y/65536.0,cz=g->pc_camera.z/65536.0,previous=0;
 int sector=g->pc_camera.sector;
 ssa_sincos(&g->math,g->pc_camera.heading,&si,&co);s=si/65536.0;c=co/65536.0;yn=s*s+c*c;
 ssa_sincos(&g->math,(uint16_t)g->render_pitch,&ps,&pc);p=ps/65536.0;q=pc/65536.0;pn=p*p+q*q;
 u=(x+0.5-w/2.0)/(h*(0x55b05a/65536.0)/80);v=(h/2.0-y-0.5)/(h*(0x55b05a/65536.0)/80);
 forward=(q-v*p)/pn;dz=(p+v*q)/pn;dx=(s*forward+c*u)/yn;dy=(c*forward-s*u)/yn;
 for(step=0;step<256;++step){double nearest=DBL_MAX,plane=DBL_MAX;unsigned line=UINT_MAX;
  const uint8_t *sec=m->data+m->sectors+(size_t)sector*20;
  if(dz>1e-10)plane=(height(sec+2)-cz)/dz;
  if(dz< -1e-10)plane=(height(sec)-cz)/dz;
  for(i=0;i<m->line_count;++i){const uint8_t *l=m->data+m->lines+(size_t)i*40,*a,*b;
   double ax,ay,vx,vy,den,t,k;
   if(rd(l+12)!=(unsigned)sector && (!((rd(l+26)&4)) || rd(l+14)!=(unsigned)sector))continue;
   a=m->data+m->vertices+(size_t)rd(l)*4;b=m->data+m->vertices+(size_t)rd(l+2)*4;
   ax=height(a);ay=height(a+2);vx=height(b)-ax;vy=height(b+2)-ay;den=dx*vy-dy*vx;
   if(absolute(den)<1e-10)continue;
   t=((ax-cx)*vy-(ay-cy)*vx)/den;k=((ax-cx)*dy-(ay-cy)*dx)/den;
   if(t>previous+0.00001 && t<nearest && k>=0 && k<1){nearest=t;line=i;}}
  if(plane>=previous-0.00001 && plane<nearest)return plane;
  if(line==UINT_MAX)return FLT_MAX;
  {const uint8_t *l=m->data+m->lines+(size_t)line*40,*back;int other;double z=cz+nearest*dz;
   if(!(rd(l+26)&4))return nearest;
   other=(int)rd(l+(rd(l+12)==(unsigned)sector?14:12));back=m->data+m->sectors+(size_t)other*20;
   if(z<height(back) || z>height(back+2))return nearest;
   sector=other;previous=nearest;}
 }
 return FLT_MAX;
}
static void verify_depth(SsaPcRenderer *r,SsaNativeGame *g)
{
 unsigned x,y,active=g->spawn.pool.active_count;g->spawn.pool.active_count=0;
 CHECK(ssa_pc_renderer_draw(r,g)==1);g->spawn.pool.active_count=active;++views;
 for(y=7;y<r->height;y+=19)for(x=9;x<r->width;x+=23){
  double want=reference(g,r->width,r->height,x,y),got=r->depth[(size_t)y*r->width+x];
  if(absolute(want-got)>0.03+want*0.00003){fprintf(stderr,"depth pitch%d yaw%u pixel%u,%u want%.6f got%.6f\n",
   g->render_pitch,g->pc_camera.heading,x,y,want,got);abort();}
  ++depth_checks;}
}
static void bmp(const SsaPcRenderer *r,const char *path)
{
 uint32_t h[14]={0};FILE *f=fopen(path,"wb");unsigned char *b=(unsigned char *)h;CHECK(f);
 b[0]='B';b[1]='M';{uint32_t n=54+r->width*r->height*4;memcpy(b+2,&n,4);}
 b[10]=54;b[14]=40;memcpy(b+18,&r->width,4);{int32_t n=-(int32_t)r->height;memcpy(b+22,&n,4);}
 b[26]=1;b[28]=32;CHECK(fwrite(b,1,54,f)==54);CHECK(fwrite(r->pixels,4,(size_t)r->width*r->height,f)==(size_t)r->width*r->height);CHECK(!fclose(f));
}
static void verify_flat_sky(SsaPcRenderer *r,SsaNativeGame *g)
{
 SsaNativeGame *saved=malloc(sizeof(*saved));size_t n=(size_t)r->width*r->height,p;
 uint32_t *mask=malloc(n*sizeof(*mask));uint8_t *floors=malloc(g->map.sector_count);unsigned i,x,y,visible=0;
 static const int pitches[]={0,4096,-4096,14564,-14564,1,-1,4096,0};
 CHECK(saved && mask && floors);*saved=*g;
 /* Expose the lower background too: ordinary map floors otherwise hide its
  * bottom-edge extension. Retain real geometry/depth and restore all colors. */
 for(i=0;i<g->map.sector_count;++i){uint8_t *s=g->map.data+g->map.sectors+(size_t)i*20;
  floors[i]=s[10];s[10]=0;}
 for(i=0;i<256;++i)g->palette_effect.colors[i]=(uint16_t)i;
 for(i=0;i<sizeof(pitches)/sizeof(*pitches);++i){unsigned axis;int32_t sine,cosine;
  double offset;uint64_t builds=r->sky_builds,geometry=r->geometry_builds;
  g->render_pitch=pitches[i];g->pc_camera.heading=(uint16_t)(517+i*7901);
  ssa_sincos(&g->math,(uint16_t)pitches[i],&sine,&cosine);
  offset=(double)sine/cosine*r->height*(0x55b05a/65536.0)/80;
  /* Two solid skies identify visible background through the real geometry
   * and HUD. Test stripes only there, without assuming which map pixels show sky. */
  memset(g->sky.pixels[0],0,sizeof(g->sky.pixels[0]));CHECK(ssa_pc_renderer_draw(r,g)==1);
  memcpy(mask,r->pixels,n*sizeof(*mask));memset(g->sky.pixels[0],255,sizeof(g->sky.pixels[0]));
  CHECK(ssa_pc_renderer_draw(r,g)==1);
  for(p=0;p<n;++p){mask[p]=(unsigned)(mask[p]!=r->pixels[p]);visible+=mask[p];}
  for(axis=0;axis<2;++axis){
   for(y=0;y<120;++y)for(x=0;x<768;++x)g->sky.pixels[0][y*768+x]=(uint8_t)(1+(axis?x%127:y));
   CHECK(ssa_pc_renderer_draw(r,g)==1);
   for(y=0;y<(axis?r->width:r->height);++y){uint32_t first=UINT_MAX;
    for(x=0;x<(axis?r->height:r->width);++x){size_t at=axis?(size_t)x*r->width+y:(size_t)y*r->width+x;
     if(!mask[at])continue;
     if(!axis){unsigned top=r->height-r->height/4;double source_y=y-offset;
      if(source_y<0){source_y=0;++sky_top_fill;}
      else if(source_y>top-1){source_y=top-1;++sky_bottom_fill;}
      else ++sky_artwork;
      CHECK(r->pixels[at]==color888(1+(unsigned)(source_y*60/top)));}
     if(first==UINT_MAX)first=r->pixels[at];else CHECK(r->pixels[at]==first);
     ++flat_sky_pixels;
    }}
  }
  /* Pitch reversal and return to level aim refresh rows without rebuilding
   * geometry or reallocating the separable lookup. */
  if(i>=2)CHECK(r->sky_builds==builds && r->geometry_builds==geometry);
 }
 CHECK(visible>1000);
 for(i=0;i<g->map.sector_count;++i)g->map.data[g->map.sectors+(size_t)i*20+10]=floors[i];
 *g=*saved;free(floors);free(mask);free(saved);
}
int main(int argc,char **argv)
{
 SsaNativeGame *g=calloc(1,sizeof(*g)),*copy=malloc(sizeof(*copy));uint8_t *rom=malloc(0x800000);
 SsaPcRenderer r={0};SsaPcInput input;SsaPcInputSample sample;SsaWeaponAim aim={0};SsaWeaponHit hit;
 static const int pitches[]={-14564,-10923,-5461,-910,-1,1,910,5461,10923,14564};unsigned i,j;
 FILE *f;CHECK(argc>=2 && g && copy && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
 CHECK(ssa_pc_input_init(&input,64)==1);CHECK(ssa_pc_input_relative(&input,10,-20)==1);
 CHECK(ssa_pc_input_sample(&input,&sample)==1);CHECK(sample.yaw_delta==640 && sample.pitch_delta==1280);
 CHECK(ssa_pc_input_sample(&input,&sample)==1 && !sample.pitch_delta);
 CHECK(ssa_pc_input_relative(&input,0,INT32_MIN)==1);CHECK(ssa_pc_input_sample(&input,&sample)==1 && sample.pitch_delta==INT32_MAX);
 CHECK(ssa_pc_input_relative(&input,0,INT32_MAX)==1);CHECK(ssa_pc_input_sample(&input,&sample)==1 && sample.pitch_delta==INT32_MIN);
 ssa_pc_input_relative(&input,0,20);ssa_pc_input_focus(&input,0);CHECK(ssa_pc_input_sample(&input,&sample)==1 && !sample.pitch_delta);
 ssa_pc_input_focus(&input,1);ssa_pc_input_relative(&input,0,20);ssa_pc_input_enable(&input,0);CHECK(ssa_pc_input_sample(&input,&sample)==1 && !sample.pitch_delta);
 CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);CHECK(ssa_native_game_step(g)==1);
 CHECK(ssa_pc_renderer_resize(&r,640,480)==1);
 verify_flat_sky(&r,g);
 CHECK(ssa_pc_renderer_resize(&r,127,73)==1);verify_flat_sky(&r,g);
 CHECK(sky_top_fill>1000 && sky_bottom_fill>1000 && sky_artwork>1000);
 CHECK(ssa_pc_renderer_resize(&r,640,480)==1);
 for(j=0;j<4;++j)for(i=0;i<sizeof(pitches)/sizeof(*pitches);++i){
  g->render_pitch=pitches[i];g->pc_camera.heading=(uint16_t)(j*16384+517);
  verify_depth(&r,g);*copy=*g;CHECK(ssa_pc_renderer_draw(&r,g)==1);CHECK(!memcmp(g,copy,sizeof(*g)));
 }
 /* Normal event path, clamp, focus gating, H and a jump. */
 g->player.pitch=0;ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
 ssa_pc_input_runtime_relative(&g->input_runtime,0,-85);CHECK(ssa_native_game_step(g)==1);
 CHECK(g->player.pitch==5440 && g->player.free_aim);CHECK(ssa_pc_renderer_draw(&r,g)==1);
 if(argc>2)bmp(&r,argv[2]);
 ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
 ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0);ssa_pc_input_runtime_relative(&g->input_runtime,0,170);
 CHECK(ssa_native_game_step(g)==1 && g->player.pitch==-5440 && g->player.view.third_person);
 CHECK(g->weapon.controller.aim_slope<0);
 CHECK(ssa_pc_renderer_draw(&r,g)==1);if(argc>3)bmp(&r,argv[3]);
 ssa_pc_input_runtime_relative(&g->input_runtime,0,INT32_MIN);CHECK(ssa_native_game_step(g)==1 && g->player.pitch==14564);
 ssa_pc_input_runtime_relative(&g->input_runtime,0,INT32_MAX);CHECK(ssa_native_game_step(g)==1 && g->player.pitch==-14564);
 ssa_pc_input_runtime_focus(&g->input_runtime,0);ssa_pc_input_runtime_relative(&g->input_runtime,0,-100);
 CHECK(ssa_native_game_step(g)==1 && g->player.pitch==-14564);
 /* Sector-plane hits for real weapon rays, not merely the rendered horizon. */
 CHECK(ssa_weapon_aim_init(&aim,&g->world,&g->math)==1);
 for(i=0;i<4;++i){int32_t eye=g->player.camera.eye_z-g->players[0].entity.body.z;
  CHECK(ssa_weapon_hit_query_free(&aim,&g->players[0].entity,(uint16_t)(i*16384),eye,1024*65536,-5*65536,&hit)==1);
  CHECK(hit.impact && !hit.target);CHECK(absolute(hit.z/65536.0-g->players[0].entity.body.floor)<0.1);}
 {SsaEntity target={0};SsaEntity *player=&g->players[0].entity;int32_t eye=g->player.camera.eye_z-player->body.z;
  int32_t sine,cosine;uint16_t yaw=123;
  ssa_sincos(&g->math,yaw,&sine,&cosine);
  target.body.x=player->body.x+sine*48;target.body.y=player->body.y+cosine*48;
  target.body.z=g->player.camera.eye_z+40*65536;target.body.height=16*65536;
  target.radius=8*65536;target.body.flags=2;target.type=0xcd;
  CHECK(ssa_world_insert(&g->world,&target));
  CHECK(ssa_weapon_hit_query_free(&aim,player,yaw,eye,256*65536,65536,&hit)==1 && hit.target==&target);
  CHECK(ssa_weapon_hit_query_free(&aim,player,yaw,eye,256*65536,0,&hit)==1 && hit.target!=&target);
  CHECK(ssa_weapon_hit_query_free(&aim,player,yaw,eye,256*65536,3*65536,&hit)==1 && hit.target!=&target);
  CHECK(ssa_world_remove(&g->world,&target));}
 CHECK(ssa_weapon_aim_free(&aim)==1);
 /* Test-only inventory fixture grants a rocket; selection, firing, muzzle
  * placement and native projectile creation still run through normal input. */
 g->players[0].inventory.weapons|=32;g->players[0].inventory.ammo[5]=10;
 CHECK(ssa_native_game_select_weapon(g,6)==1);
 ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
 ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0);CHECK(ssa_pc_input_sensitivity(&g->input,1)==1);
 ssa_pc_input_runtime_relative(&g->input_runtime,0,-(3641-g->player.pitch));
 for(i=0;i<120 && !g->weapon_game.shots[5];++i){
  if(g->gameplay.weapon.current==6 && g->weapon.object.y==0x5000)ssa_pc_input_runtime_button(&g->input_runtime,1,1);
  CHECK(ssa_native_game_step(g)==1);}
 CHECK(g->weapon_game.shots[5]==1 && g->player.pitch==3641);
 {unsigned found=0;int32_t sine,cosine,ps,pc;double slope;
  ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
  ssa_sincos(&g->math,(uint16_t)g->player.pitch,&ps,&pc);slope=(double)ps/pc;
  for(i=0;i<100;++i)if(g->projectiles.active[i] && g->projectiles.projectiles[i].source==&g->players[0].entity &&
    g->projectiles.projectiles[i].entity->type==0x65){const SsaBody *body=&g->projectiles.projectiles[i].entity->body;
    double forward=((double)body->velocity_x*sine+(double)body->velocity_y*cosine)/65536;
    double distance=((double)body->x-g->players[0].entity.body.x)*sine/65536+
        ((double)body->y-g->players[0].entity.body.y)*cosine/65536;
    CHECK(forward>0 && body->velocity_z>0);CHECK(absolute(body->velocity_z/forward-slope)<0.0002);
    CHECK(absolute((body->z-g->player.camera.eye_z-distance*slope)/65536)<1);++found;}
  CHECK(found==1);}
 /* Vertical aiming must still use the original type18 HUD object, including
  * its original position, artwork and target animation rather than a new glyph. */
 {SsaWeaponObject *crosshair=NULL;
  for(i=0;i<84;++i)if(g->sprites.objects[i]->type==18)crosshair=g->sprites.objects[i];
  CHECK(g->player.free_aim && crosshair && (crosshair->flags&1) && crosshair->y==0x5000);
  CHECK(crosshair->animation==((g->gameplay.weapon.flags&0x20000)?0xda:0xd9));
  CHECK(ssa_pc_renderer_resize(&r,640,480)==1);verify_original_hud(&r,g);
  CHECK(ssa_pc_renderer_resize(&r,127,73)==1);verify_original_hud(&r,g);
 }
 /* Static topology and local sky directions survive ordinary turning; live
  * sector height edits invalidate neither cache, and still affect depth. */
 CHECK(ssa_pc_renderer_resize(&r,640,480)==1);g->render_pitch=5440;verify_depth(&r,g);
 {uint64_t geometry_builds=r.geometry_builds,sky_builds=r.sky_builds;
  g->pc_camera.heading+=107;verify_depth(&r,g);
  CHECK(r.geometry_builds==geometry_builds && r.sky_builds==sky_builds);
  {uint8_t *sector=g->map.data+g->map.sectors+(size_t)g->pc_camera.sector*20;
   unsigned saved=rd(sector+2),ceiling=(unsigned)(g->pc_camera.z/65536+32);
   sector[2]=(uint8_t)ceiling;sector[3]=(uint8_t)(ceiling>>8);verify_depth(&r,g);
   CHECK(r.geometry_builds==geometry_builds && r.sky_builds==sky_builds);
   sector[2]=(uint8_t)saved;sector[3]=(uint8_t)(saved>>8);}
 }
 ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);free(copy);free(rom);
 printf("{\"result\":\"pass\",\"checks\":%u,\"depth_comparisons\":%u,\"pitched_views\":%u,\"original_hud_pixels\":%u,\"flat_sky_pixels\":%u,\"sky_top_fill_pixels\":%u,\"sky_bottom_fill_pixels\":%u,\"sky_artwork_pixels\":%u}\n",checks,depth_checks,views,hud_pixels,flat_sky_pixels,sky_top_fill,sky_bottom_fill,sky_artwork);return 0;
}
