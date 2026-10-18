#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks,views,bob_views;static double maximum_error;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GBA aim line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static SsaWeaponObject *cursor(SsaNativeGame *g)
{
    unsigned i;
    for(i=0;i<84;++i)if(g->sprites.objects[i]->type==18)return g->sprites.objects[i];
    CHECK(0);return NULL;
}
static void verify(SsaNativeGame *g,SsaWeaponAim *query)
{
    SsaEntity *player=&g->players[0].entity;SsaWeaponHit hit;SsaWeaponObject *c=cursor(g);
    int32_t sine,cosine;double depth,expected,error;unsigned i,parts=0;
    unsigned image=c->previous_frame&0x3fff,tile=g->cache.entries[image].tile*2;
    CHECK(c->flags&1);CHECK(c->x==122*256 && c->screen_y==c->y/256);
    CHECK(!g->player.free_aim && !g->player.pitch);
    CHECK(c->animation==((g->gameplay.weapon.flags&0x20000)?0xda:0xd9));
    CHECK(ssa_weapon_hit_query_unlimited(query,player,(uint16_t)((unsigned)g->players[0].motion.facing<<8),
        96*65536,g->weapon.controller.aim_slope,&hit,1)==1);
    if(!hit.impact){hit.x=query->end_x;hit.y=query->end_y;
        hit.z=(int32_t)((int64_t)query->shoot_z+(int64_t)query->range*g->weapon.controller.aim_slope/65536);}
    ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
    depth=((hit.x-(double)g->player.pose.x)*sine+(hit.y-(double)g->player.pose.y)*cosine)/65536;
    CHECK(depth>0);
    /* Independent pinhole projection in the reference display's160 rows.
     * Original distance-table/pixel quantization may differ by a pixel. */
    expected=80-(hit.z-((double)g->player.pose.z+player->body.z+g->players[0].vertical.eye_offset-g->player.camera.eye_z))*2*(0x55b05a/65536.0)/depth;
    if(expected<2)expected=2;
    if(expected>157)expected=157;
    error=c->y/256.0-expected;if(error<0)error=-error;
    if(error>maximum_error)maximum_error=error;
    if(error>=2)fprintf(stderr,"cursor%f expected%f depth%f slope%d third%u\n",c->y/256.0,expected,depth/65536,g->weapon.controller.aim_slope,g->player.pose.third_person);
    CHECK(error<2);
    /* Verify actual OAM placement, not just a controller's scratch value. */
    {const SsaSpriteImage *im=&g->sprite_assets.images[image];unsigned part;
        for(part=0;part<im->part_count;++part){const SsaSpritePart *piece=&im->parts[part];
            int y=piece->attribute0&511,x=piece->attribute1&511;unsigned found=0;
            if(y&256)y-=512;
            if(x&256)x-=512;
            if(c->previous_frame&0x4000)y=-y-piece->height;
            if(c->previous_frame&0x8000)x=-x-piece->width;
            for(i=0;i<g->oam.count;++i)if((g->oam.entries[i].attribute2&1023)==tile+piece->tile*2u &&
                (g->oam.entries[i].attribute1&511)==((unsigned)(c->screen_x+x)&511)){
                CHECK((g->oam.entries[i].attribute0&255)==((unsigned)(c->screen_y+y)&255));++found;
            }
            CHECK(found==1);++parts;
        }
    }
    CHECK(parts>0);++views;
}
static void toggle(SsaNativeGame *g)
{
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);
}
static void steady_cursor(SsaNativeGame *g)
{
    SsaWeaponObject *c=cursor(g);SsaBody moving=g->players[0].entity.body;
    SsaCamera original_eye=g->player.camera;SsaViewPose original_pose=g->player.pose;
    SsaView original_view=g->player.view;unsigned mode,i;int32_t baseline=0,low=INT32_MAX,high=INT32_MIN;
    moving.velocity_x=4*65536;moving.velocity_y=0;
    for(mode=0;mode<3;++mode){
        g->player.view.third_person=mode!=0;g->player.view.distance=(mode==2?96:256)*65536;
        for(i=0;i<64;++i){
            SsaPlayerVertical vertical=g->players[0].vertical;SsaCameraFrame frame={0};SsaCamera before;
            SsaPlayerVertical before_vertical=g->players[0].vertical;
            frame.counter=(uint16_t)(i*7);vertical.flags&=(uint16_t)~4u;
            (void)ssa_camera_step(&moving,&vertical,&g->player.camera,&frame,&g->math);
            if(g->player.camera.eye_z<low)low=g->player.camera.eye_z;
            if(g->player.camera.eye_z>high)high=g->player.camera.eye_z;
            CHECK(ssa_view_pose(&g->player.view,&g->players[0].entity.body,&g->player.camera,
                g->players[0].motion.movement.heading,&g->math,&g->player.view_collision,&g->player.pose));
            before=g->player.camera;
            CHECK(g->sprite_ops.handler(g,&g->sprites,0,c->slot)==0);
            CHECK(g->player.camera.eye_z==before.eye_z && g->player.camera.bob_amplitude==before.bob_amplitude);
            CHECK(g->players[0].vertical.eye_offset==before_vertical.eye_offset &&
                g->players[0].vertical.eye_velocity==before_vertical.eye_velocity);
            if(!i)baseline=c->y;
            CHECK(c->y==baseline);++bob_views;
            if(!mode && !g->weapon.controller.aim_slope)CHECK(c->y==80*256);
        }
    }
    CHECK(high-low>65536); /* Exercise real positive and negative camera bob. */
    g->player.camera=original_eye;g->player.pose=original_pose;g->player.view=original_view;
}
int main(int argc,char **argv)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaWeaponAim query={0};SsaScriptActor *fixture=NULL,saved;SsaEntity *target;
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned i,mode;int32_t sine,cosine;
    int32_t first_y=0;static const int elevations[]={-20,0,20};
    CHECK(argc==2 && g && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(ssa_weapon_aim_init(&query,&g->world,&g->math)==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
    for(i=0;i<30;++i)CHECK(ssa_native_game_tick(g)==1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
    g->players[0].inventory.weapons|=2;g->players[0].inventory.ammo[1]=99;
    CHECK(ssa_native_game_select_weapon(g,2)==1);
    for(i=0;i<90 && (g->gameplay.weapon.current!=2 || g->weapon.object.y!=0x5000);++i)
        CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->gameplay.weapon.current==2 && g->weapon.object.y==0x5000);
    /* Simulate a renderer change after software mouse look was in use. */
    g->player.pitch=5440;g->player.free_aim=1;g->gba_view=1;
    CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,-85)==1);
    CHECK(ssa_native_game_tick(g)==1 && !g->input.pending_pitch && !g->player.free_aim && !g->player.pitch);
    /* Reposition a registered pickup as a test target, retaining the real
     * pool/world ownership invariant throughout each complete game frame. */
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && g->spawn.actors[i].entity.type==0xe1){fixture=&g->spawn.actors[i];break;}
    CHECK(fixture);saved=*fixture;target=&fixture->entity;
    CHECK(ssa_world_remove(&g->world,target));
    ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
    target->body.x=g->players[0].entity.body.x+sine*48;
    target->body.y=g->players[0].entity.body.y+cosine*48;
    target->body.height=8*65536;target->radius=8*65536;target->body.flags=2;
    for(mode=0;mode<2;++mode){
        for(i=0;i<3;++i){
            target->body.z=g->players[0].entity.body.z+(96+elevations[i]-4)*65536;
            CHECK(ssa_world_insert(&g->world,target));
            CHECK(ssa_native_game_tick(g)==1);
            CHECK(g->weapon.controller.aim.target==target);
            CHECK(i==0?g->weapon.controller.aim_slope<0:i==1?g->weapon.controller.aim_slope==0:g->weapon.controller.aim_slope>0);
            verify(g,&query);
            if(!mode){CHECK(i==0?cursor(g)->y>80*256:i==2?cursor(g)->y<80*256:1);
                if(i==1)first_y=cursor(g)->y;
            }else if(i==1)CHECK(cursor(g)->y>first_y);
            CHECK(ssa_world_remove(&g->world,target));
        }
        target->body.flags=0;CHECK(ssa_world_insert(&g->world,target));
        /* No target still marks the actual horizontal shot from the raised camera. */
        CHECK(ssa_native_game_tick(g)==1);verify(g,&query);
        CHECK(ssa_native_game_draw(g)==1);
        if(!mode){toggle(g);CHECK(ssa_native_game_tick(g)==1 && g->player.pose.third_person);}
        CHECK(ssa_world_remove(&g->world,target));target->body.flags=2;
    }
    *fixture=saved;fixture->entity.owner=NULL;
    fixture->entity.block_next=fixture->entity.block_previous=NULL;
    fixture->entity.sector_next=fixture->entity.sector_previous=NULL;
    CHECK(ssa_world_insert(&g->world,&fixture->entity));
    /* A shortened boom changes cursor placement; no fixed third-person offset. */
    g->player.view.distance=96*65536;CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->player.pose.third_person);verify(g,&query);
    steady_cursor(g);
    /* Software mode retains its centered original crosshair and vertical look. */
    g->gba_view=0;CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,-85)==1);
    CHECK(ssa_native_game_tick(g)==1 && g->player.free_aim && g->player.pitch==5440);
    CHECK(cursor(g)->y==80*256);
    CHECK(ssa_native_game_select_weapon(g,1)==1);
    for(i=0;i<90 && g->gameplay.weapon.current!=1;++i)CHECK(ssa_native_game_tick(g)==1);
    g->gba_view=1;CHECK(ssa_native_game_tick(g)==1 && !(cursor(g)->flags&1));
    CHECK(ssa_weapon_aim_free(&query)==1);ssa_native_game_free(g);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"projected_views\":%u,\"bob_views\":%u,\"maximum_error_pixels\":%.6f}\n",checks,views,bob_views,maximum_error);
    return 0;
}
