/* Native side of the whole original idle HUD presentation comparison. */
#include "ssa/hud_scene.h"
#include <string.h>
static SsaHudScene scene;
static SsaSpriteAssets assets;
static SsaMathTables math_tables;
static SsaInventory inventory;
static SsaEntity body;
static uint32_t state[32];
static uint16_t numeric[32];
int start(const void *rom,size_t size,unsigned level)
{
    if(scene.loaded && ssa_hud_scene_free(&scene)!=1)return -1;
    if(!assets.loaded && (ssa_sprite_assets_load(&assets,rom,size)!=1 ||
        ssa_math_tables_load(&math_tables,rom,size)!=1))return -1;
    memset(&inventory,0,sizeof(inventory));memset(&body,0,sizeof(body));
    inventory.health=100;
    return ssa_hud_scene_init(&scene,&inventory,&body,&assets,&math_tables,rom,size,level,0);
}
int step(unsigned frame)
{
    scene.state.health=(int16_t)(100-frame%101);
    scene.state.armor=(int16_t)(frame%101);
    scene.state.score=frame*12345;scene.state.time=frame*1234;
    scene.state.local_flags=(uint16_t)(frame%17==3?8:0);
    scene.state.game_flags=(uint16_t)(frame%13==7?1:0);
    return ssa_hud_scene_step_idle(&scene,1,3,0);
}
int display(const uint8_t *world,unsigned family,uint16_t *out)
{return ssa_hud_scene_draw(&scene,world,14400,family,out,38400);}
const void *output(unsigned kind)
{
    unsigned i;SsaSpriteCache *c=&scene.cache;
    switch(kind){
    case 0:return scene.objects;case 1:return scene.dispatch.active;
    case 2:return scene.pool.free_slots;case 3:return c->entries;
    case 4:return c->free_blocks;case 5:return c->used_blocks;
    case 6:return c->used_images;case 7:return c->requests;
    case 8:return c->tiles;case 9:return scene.oam.entries;
    case 10:
        memcpy(numeric,scene.state.ammo_x,6);memcpy(numeric+3,scene.state.ammo_image,6);
        memcpy(numeric+6,scene.state.score_x,16);memcpy(numeric+14,scene.state.score_image,16);
        memcpy(numeric+22,scene.state.time_x,10);memcpy(numeric+27,scene.state.time_image,10);
        return numeric;
    case 11:
        state[0]=scene.dispatch.active_count;state[1]=scene.pool.free_count;
        state[2]=scene.dispatch.counter;state[3]=c->free_count;state[4]=c->used_count;
        state[5]=c->image_count;state[6]=c->read_cursor;state[7]=c->write_cursor;
        state[8]=c->allocation_failures;state[9]=c->budget;
        state[10]=scene.control.current;state[11]=scene.control.flags;
        state[12]=scene.control.requested;state[13]=inventory.weapons;
        state[14]=inventory.pending_count;state[15]=scene.state.game_flags;
        for(i=0;i<10;++i)state[16+i]=(uint16_t)inventory.ammo[i];
        return state;
    case 12:return assets.palette;
    case 13:return assets.background_palette[0];
    case 14:return assets.background_palette[1];
    default:return NULL;
    }
}
