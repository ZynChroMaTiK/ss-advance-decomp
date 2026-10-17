#include "ssa/view.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define U INT32_C(65536)
static unsigned checks, real_poses,pitched_poses;
#define CHECK(x) do { assert(x); ++checks; } while (0)
static void put16(uint8_t *p,int n) { p[0]=(uint8_t)n; p[1]=(uint8_t)((unsigned)n>>8); }
static void wall(SsaMapAssets *m,int y,int flags,int back_floor,int back_ceiling) {
    uint8_t *d=m->data, *l=d+m->lines;
    put16(d+m->nodes+2,y);
    put16(d+m->vertices+2,y); put16(d+m->vertices+6,y);
    put16(l+16,y); put16(l+18,y); put16(l+26,flags);
    put16(d+m->sectors+20,back_floor); put16(d+m->sectors+22,back_ceiling);
}
static void fixture(SsaMapAssets *m) {
    unsigned i;
    uint8_t *d;
    memset(m,0,sizeof(*m)); m->data=calloc(1,4096); assert(m->data); m->size=4096;
    m->sector_count=2; m->node_count=1; m->leaf_count=2; m->line_count=1; m->vertex_count=2;
    m->sectors=72; m->nodes=112; m->leaves=144; m->lines=160; m->vertices=200; m->blockmap=208;
    d=m->data; put16(d+0x1a,71);
    put16(d+m->sectors+2,256); put16(d+m->nodes+4,1024);
    put16(d+m->nodes+24,0x8000); put16(d+m->nodes+26,0x8001);
    put16(d+m->leaves+4,1); put16(d+m->leaves+12,0);
    put16(d+m->vertices,-512); put16(d+m->vertices+4,512);
    put16(d+m->lines+2,1); put16(d+m->lines+4,1024); put16(d+m->lines+14,1);
    put16(d+m->lines+20,-512); put16(d+m->lines+22,512);
    put16(d+m->blockmap,-512); put16(d+m->blockmap+2,-512);
    put16(d+m->blockmap+4,8); put16(d+m->blockmap+6,8);
    for (i=0;i<64;++i) put16(d+m->blockmap+8+2*i,68);
    put16(d+m->blockmap+140,0xffff);
    wall(m,-64,4,0,256);
}
static SsaViewPose pose(SsaView *v,SsaBody *b,SsaCamera *eye,unsigned yaw,SsaMathTables *t,SsaMapCollision *c) {
    SsaViewPose p;
    SsaBody saved=*b; SsaCamera saved_eye=*eye;
    CHECK(ssa_view_pose(v,b,eye,(uint16_t)yaw,t,c,&p));
    CHECK(memcmp(&saved,b,sizeof(*b))==0 && memcmp(&saved_eye,eye,sizeof(*eye))==0);
    return p;
}
int main(int argc,char **argv) {
    FILE *file;
    uint8_t *rom;
    long size;
    SsaMathTables tables;
    SsaView v;
    SsaMapAssets m={0}; SsaMapCollision c={0};
    SsaBody b={0}; SsaCamera eye={0}; SsaViewPose p;
    unsigned i,level;
    assert(argc==2); file=fopen(argv[1],"rb"); assert(file);
    assert(fseek(file,0,SEEK_END)==0); size=ftell(file); assert(size>0); rewind(file);
    rom=malloc((size_t)size); assert(rom); assert(fread(rom,1,(size_t)size,file)==(size_t)size); fclose(file);
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)size));
    ssa_view_init(&v); CHECK(!v.third_person);
    CHECK(!ssa_view_key(&v,'G',1,0,1));
    CHECK(ssa_view_key(&v,'H',1,0,1) && v.third_person);
    for (i=0;i<20;++i) CHECK(!ssa_view_key(&v,'H',1,i%2,1) && v.third_person);
    CHECK(!ssa_view_key(&v,'h',0,0,1));
    CHECK(ssa_view_key(&v,'h',1,0,1) && !v.third_person);
    ssa_view_focus_lost(&v);
    CHECK(!ssa_view_key(&v,'H',1,1,1) && !v.third_person); /* repeat after focus loss */
    ssa_view_key(&v,'H',0,0,1);
    CHECK(!ssa_view_key(&v,'H',1,0,0)); /* menu/text input */
    CHECK(!ssa_view_key(&v,'H',1,0,1) && !v.third_person);
    ssa_view_key(&v,'H',0,0,1);
    CHECK(ssa_view_key(&v,'H',1,0,1) && v.third_person);
    ssa_view_focus_lost(&v); CHECK(!v.h_down && v.third_person);

    fixture(&m); CHECK(ssa_map_collision_init(&c,&m));
    b.flags=16; b.height=100*U; b.ceiling=256; eye.eye_z=96*U;
    v.third_person=0;
    p=pose(&v,&b,&eye,0,&tables,&c);
    CHECK(!p.third_person && p.x==0 && p.y==0 && p.z==96*U && p.target_y==256*U);
    v.third_person=1;
    for (i=0;i<4;++i) {
        static const int x[]={0,-256,0,256}, y[]={-256,0,256,0};
        p=pose(&v,&b,&eye,i*0x4000,&tables,&c);
        CHECK(p.third_person && !p.obstructed && p.boom_distance==256*U);
        CHECK(p.x==x[i]*U && p.y==y[i]*U && p.z==160*U);
    }
    wall(&m,-64,0,0,256);
    p=pose(&v,&b,&eye,0,&tables,&c);
    CHECK(p.third_person && p.obstructed && p.boom_distance==60*U && p.y==-60*U);
    wall(&m,-24,0,0,256);
    p=pose(&v,&b,&eye,0,&tables,&c);
    CHECK(!p.third_person && p.obstructed && p.x==b.x && p.y==b.y && p.z==eye.eye_z);
    CHECK(v.third_person); /* obstruction must not change user preference */
    wall(&m,-64,4,0,100); /* low portal */
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.obstructed && p.boom_distance<=60*U);
    wall(&m,-64,4,120,256); /* raised rear floor */
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.obstructed && p.boom_distance<=60*U);
    wall(&m,-64,4,0,256); /* expands as soon as obstruction clears */
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.third_person && p.boom_distance==256*U);
    put16(m.data+m.sectors+2,110);
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.third_person && p.obstructed && p.z<=106*U);
    put16(m.data+m.sectors+2,98);
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(!p.third_person && p.obstructed);
    put16(m.data+m.sectors+2,256);
    p=pose(&v,&b,&eye,0,&tables,NULL); CHECK(!p.third_person && p.obstructed);
    v.distance=INT32_MIN; v.lift=INT32_MAX;
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(!p.third_person);
    v.distance=INT32_MAX;
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.boom_distance==256*U && p.z==160*U);
    v.lift=INT32_MIN;
    p=pose(&v,&b,&eye,0,&tables,&c); CHECK(p.z==eye.eye_z);
    b.x=INT32_MAX; v.third_person=0;
    p=pose(&v,&b,&eye,0x4000,&tables,&c); CHECK(p.target_x==INT32_MAX);
    { SsaViewPose saved=p; CHECK(!ssa_view_pose(NULL,&b,&eye,0,&tables,&c,&p)); CHECK(memcmp(&saved,&p,sizeof(p))==0); }
    ssa_map_collision_free(&c); ssa_map_assets_free(&m);

    /* Free orbit in a tall room: rotating pitch changes position, keeps the
     * camera behind the eye, and preserves the default world-up height bias. */
    fixture(&m);wall(&m,-64,4,0,2048);put16(m.data+m.sectors+2,2048);
    CHECK(ssa_map_collision_init(&c,&m));memset(&b,0,sizeof(b));b.z=512*U;b.height=100*U;eye.eye_z=608*U;
    ssa_view_init(&v);v.third_person=1;
    for(i=0;i<9;++i){int pitch=((int)i-4)*3000;SsaViewPose tilted;int32_t sp,cp;uint16_t heading=0;
        SsaBody saved=b;SsaCamera saved_eye=eye;int32_t rendered=0;
        CHECK(ssa_view_pose_pitch(&v,&b,&eye,0,pitch,&tables,&c,&tilted));
        CHECK(!memcmp(&saved,&b,sizeof(b)) && !memcmp(&saved_eye,&eye,sizeof(eye)));
        CHECK(tilted.third_person && !tilted.obstructed && tilted.y<0);
        ssa_sincos(&tables,(uint16_t)pitch,&sp,&cp);
        CHECK(tilted.y==-(int32_t)((int64_t)cp*256) && tilted.z==eye.eye_z+64*U-(int32_t)((int64_t)sp*256));
        tilted.target_y=512*U;tilted.target_z=eye.eye_z+(int32_t)((int64_t)512*U*sp/cp);
        CHECK(ssa_view_angles(&tilted,&tables,&heading,&rendered));CHECK(heading==0);
        if(!pitch)CHECK(rendered<0); /* Raised camera converges down to the shot. */
    }
    ssa_map_collision_free(&c);ssa_map_assets_free(&m);

    for (level=0;level<16;++level) {
        SsaPlayerStart start; SsaPlayerVertical player={0}; SsaJump jump;
        SsaCameraFrame frame={0}; SsaViewPose previous={0};
        CHECK(ssa_map_assets_load(&m,rom,(size_t)size,level)); CHECK(ssa_map_collision_init(&c,&m));
        CHECK(ssa_map_player_start(&m,&start));
        memset(&b,0,sizeof(b)); b.x=start.x; b.y=start.y; b.z=start.z;
        b.height=100*U; b.flags=16; b.floor=start.floor; b.ceiling=start.ceiling;
        player.eye_offset=96*U; player.flags=4;
        ssa_view_init(&v); CHECK(ssa_view_key(&v,'H',1,0,1)); ssa_jump_init(&jump);
        for (i=0;i<128;++i) {
            ssa_player_vertical_step(&b,&player,&jump,1,1,1);
            ssa_camera_step(&b,&player,&eye,&frame,&tables);
            p=pose(&v,&b,&eye,start.heading,&tables,&c); ++real_poses;
            CHECK(p.target_z==eye.eye_z && p.boom_distance>=0 && p.boom_distance<=256*U);
            if (p.third_person) {
                CHECK(ssa_map_query(&c,p.x,p.y,p.x,p.y,4*U)==1);
                CHECK((int64_t)p.z-4*U>=(int64_t)c.floor*U && (int64_t)p.z+4*U<=(int64_t)c.ceiling*U);
            } else CHECK(p.x==b.x && p.y==b.y && p.z==eye.eye_z);
            if(!(i%16)){
                SsaViewPose tilted;SsaBody saved=b;SsaCamera saved_eye=eye;
                CHECK(ssa_view_pose_pitch(&v,&b,&eye,start.heading,((int)(i/16)-4)*3600,&tables,&c,&tilted));
                CHECK(!memcmp(&saved,&b,sizeof(b)) && !memcmp(&saved_eye,&eye,sizeof(eye)));++pitched_poses;
                if(tilted.third_person){
                    CHECK(ssa_map_query(&c,tilted.x,tilted.y,tilted.x,tilted.y,4*U)==1);
                    CHECK((int64_t)tilted.z-4*U>=(int64_t)c.floor*U && (int64_t)tilted.z+4*U<=(int64_t)c.ceiling*U);
                }else CHECK(tilted.x==b.x && tilted.y==b.y && tilted.z==eye.eye_z);
            }
            if (i==0) previous=p;
            if (i==8) CHECK(p.target_z>previous.target_z); /* follows jump upward */
            if (i==64) { ssa_view_key(&v,'H',0,0,1); CHECK(ssa_view_key(&v,'H',1,0,1)); }
            if (i>64) CHECK(!p.third_person && p.z==eye.eye_z);
        }
        CHECK(b.z==start.z && eye.eye_z==start.z+96*U);
        ssa_map_collision_free(&c); ssa_map_assets_free(&m);
    }
    free(rom);
    printf("{\"checks\":%u,\"real_map_camera_frames\":%u,\"pitched_map_poses\":%u,\"maps\":16}\n",checks,real_poses,pitched_poses);
    return 0;
}
