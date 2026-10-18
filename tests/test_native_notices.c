/* Actual pickup/death callbacks. Relocated pickup/vulnerability fixtures are
 * adapter checks, not campaign admission evidence. Both renderers stay native. */
#define main existing_renderer_check
#include "test_pc_renderer.c"
#undef main
static unsigned starts,frames,pickups,ready,death_cases,renders,projected;
static unsigned pickup_slot(const SsaNativeGame *g){
    unsigned slot;
    for(slot=0;slot<100;++slot)if(g->spawn.actors[slot].owned && g->spawn.actors[slot].entity.type==0xe1 &&
        g->spawn.actors[slot].animation>=32 && g->spawn.actors[slot].animation<=61 &&
        !g->spawn.actors[slot].script.trigger && !(g->spawn.actors[slot].entity.body.flags&0x80))break;
    return slot;
}
static unsigned message_for(unsigned item){
    static const unsigned weapons[10]={2,3,4,6,0,5,10,8,9,7};
    if(item>=32 && item<=41)return weapons[item-32]?0x8b+weapons[item-32]:0xab;
    if(item>=42 && item<=47)return 0x97+item-42;
    if(item>=48 && item<=50)return item==50?0xa0:0x9d+item-48;
    if(item==51)return 0x96;
    if(item==55)return 0xa4;
    if(item>=52 && item<=61)return 0xa1+item-52;
    return 0xab;
}
static const char *localized(const uint8_t *rom,unsigned lang,unsigned id){
    unsigned at=0x44cf34+4*lang,table=(unsigned)rom[at]|(unsigned)rom[at+1]<<8|(unsigned)rom[at+2]<<16|(unsigned)rom[at+3]<<24;
    at=table-0x08000000+4*id;table=(unsigned)rom[at]|(unsigned)rom[at+1]<<8|(unsigned)rom[at+2]<<16|(unsigned)rom[at+3]<<24;
    return (const char *)rom+table-0x08000000;
}
static unsigned visible_letters(const SsaNativeGame *g){
    unsigned i,n=0;for(i=0;i<g->sprites.active_count;++i){const SsaWeaponObject *o=g->sprites.objects[g->sprites.active[i]];
        if(o->type==13 && (o->flags&1))++n;}return n;
}
static void draw(SsaNativeGame *g,SsaPcRenderer *r,unsigned lang,const char *kind,unsigned save){
    CHECK(ssa_native_game_draw(g)==1);CHECK(ssa_pc_renderer_draw(r,g)==1);++renders;
    if(save){SsaPcRenderer reference={0};char path[128];reference.width=240;reference.height=160;reference.pixels=g->rgb888;
        snprintf(path,sizeof(path),"build/native-tests/notice-%s-%u-reference.bmp",kind,lang);snapshot(&reference,path);
        snprintf(path,sizeof(path),"build/native-tests/notice-%s-%u-pc.bmp",kind,lang);snapshot(r,path);}
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};FILE *f;
    unsigned level,lang,frame,slot,item,expected,i;
    CHECK(argc==2 && rom && g);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_pc_renderer_resize(&r,480,320)==1);
    for(level=0;level<16;++level)for(lang=0;lang<3;++lang){
        SsaEntity *pickup;uint32_t elapsed;
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);g->hud_notice.language=lang;++starts;
        for(i=0;i<2;++i){CHECK(ssa_native_game_tick(g)==1);++frames;}
        slot=pickup_slot(g);
        /* Multiplayer respawning correctly stays disabled for one player.
         * Empty MP maps use an explicit presentation fixture, not a fake pickup. */
        if(slot==100){
            CHECK(level>=12);expected=0x8d;g->hud.game_flags=g->frame_game.flags;
            CHECK(ssa_hud_notice_set(&g->hud_notice,&g->hud,expected,0,48,0)==1);
            g->frame_game.flags=g->hud.game_flags;
            g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=120;++projected;
        }else{
        item=g->spawn.actors[slot].animation;expected=message_for(item);pickup=&g->spawn.actors[slot].entity;
        g->players[0].inventory.health=g->players[0].entity_health=50;
        g->players[0].inventory.stat_c4=g->players[0].inventory.stat_c6=0;
        memset(g->players[0].inventory.ammo,0,sizeof(g->players[0].inventory.ammo));
        CHECK(ssa_world_remove(&g->world,pickup)==1);pickup->body.x=g->players[0].entity.body.x;
        pickup->body.y=g->players[0].entity.body.y;pickup->body.z=g->players[0].entity.body.z;
        CHECK(ssa_world_insert(&g->world,pickup)==1);i=g->pickup.game.collected;
        CHECK(ssa_world_query(&g->world,&g->players[0].entity,pickup->body.x,pickup->body.y)>=0);
        CHECK(g->pickup.game.collected==i+1 && !g->spawn.actors[slot].owned);++pickups;
        }
        CHECK(g->hud_notice.id==expected && g->hud.notice_timer==120 && g->game.life_game.message_ticks==120);
        CHECK(!strcmp((const char *)g->hud_notice.text,localized(rom,lang,expected)) && !g->notice_ticks);
        g->frame_game.flags|=1;elapsed=g->frame_game.elapsed;
        for(frame=0;frame<124;++frame){
            unsigned remaining=frame<120?119-frame:0;
            CHECK(ssa_native_game_tick(g)==1);++frames;
            CHECK(g->hud.notice_timer==(int)remaining && g->game.life_game.message_ticks==remaining &&
                g->gameplay.life_game.message_ticks==remaining && g->frame_game.elapsed==elapsed);
            if(frame==24){CHECK(g->hud_notice.progress==g->hud_notice.count && (g->frame_game.flags&0x200));
                CHECK(visible_letters(g)==g->hud_notice.count);++ready;draw(g,&r,lang,"pickup",level==0);}
            if(frame==123){CHECK(!visible_letters(g) && !(g->frame_game.flags&0x240));draw(g,&r,lang,"expired",0);}
        }
        g->frame_game.flags&=(uint16_t)~1u;g->players[0].entity.body.flags&=(uint16_t)~1u;
        CHECK(ssa_combat_runtime_damage(&g->combat,&g->players[0].entity,&g->players[0].entity,NULL,10000,&g->combat_ops)==0);
        CHECK(!g->players[0].inventory.health);
        for(frame=0;frame<8;++frame){CHECK(ssa_native_game_tick(g)==1);++frames;
            CHECK(g->hud_notice.id==0xb5 && g->hud.notice_timer==599-(int)frame &&
                !strcmp((const char *)g->hud_notice.text,localized(rom,lang,0xb5)));
        }
        draw(g,&r,lang,"death",level==0 && lang==0);++death_cases;ssa_native_game_free(g);
    }
    ssa_pc_renderer_free(&r);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"starts\":%u,\"maps\":16,\"languages\":3,\"frames\":%u,\"actual_pickups\":%u,\"ready_notices\":%u,\"death_cases\":%u,\"render_pairs\":%u,\"projected_notices\":%u}\n",
        checks,starts,frames,pickups,ready,death_cases,renders,projected);return 0;
}
