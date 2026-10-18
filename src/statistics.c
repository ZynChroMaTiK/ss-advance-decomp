#include "ssa/statistics.h"

int ssa_statistics_complete(SsaStatisticsTotals *out,const SsaLevelStatistics *level)
{
    unsigned i;
    if(!out || !level)return -1;
    out->elapsed+=level->elapsed;
    for(i=0;i<10;++i)out->weapon_hits+=level->rows.weapons[i].hits;
    for(i=0;i<12;++i){
        out->spawned+=level->rows.enemies[i].spawned;
        out->kills+=level->rows.enemies[i].kills;
        out->hits+=level->rows.enemies[i].hits;
    }
    out->pickups+=level->pickups;
    out->score=level->score;
    out->health=level->health;out->armor6=level->armor6;out->armor4=level->armor4;
    out->level=level->level;
    return 1;
}
int ssa_statistics_checkpoint_capture(SsaStatisticsRows *out,const SsaLevelStatistics *level)
{
    if(!out || !level)return -1;
    *out=level->rows;
    return 1;
}
int ssa_statistics_checkpoint_restore(SsaLevelStatistics *level,const SsaStatisticsRows *saved)
{
    if(!level || !saved)return -1;
    level->rows=*saved;level->pickups=0;
    return 1;
}
