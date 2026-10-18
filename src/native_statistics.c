#include "ssa/native_statistics.h"

int ssa_native_game_statistics(SsaLevelStatistics *out,const SsaNativeGame *g)
{
    SsaLevelStatistics next={0};SsaWeaponProgress saved;unsigned i;
    if(!out || ssa_native_game_weapon_progress(&saved,next.rows.weapons,g)!=1)return -1;
    for(i=0;i<12;++i){
        next.rows.enemies[i].spawned=g->spawn.game.spawned[i];
        next.rows.enemies[i].hits=g->damage.enemy_hits[i];
        next.rows.enemies[i].kills=g->damage.enemy_kills[i];
    }
    next.elapsed=g->frame_game.elapsed;next.score=g->score;next.pickups=g->pickup.game.collected;
    next.health=g->players[0].inventory.health;
    next.armor6=g->players[0].inventory.stat_c6;next.armor4=g->players[0].inventory.stat_c4;
    next.level=(uint8_t)g->level;
    *out=next;return 1;
}
