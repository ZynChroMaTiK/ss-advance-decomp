#include "ssa/native_completion.h"
#include <string.h>

int ssa_native_completion_init(SsaCompletion *out,const SsaNativeGame *g)
{
    SsaLevelStatistics level;
    if(!out || ssa_native_game_statistics(&level,g)!=1 || !g->finished || level.health<=0 ||
       g->gameplay.life_game.input_state!=3)return -1;
    return ssa_completion_init(out,g->rom,g->rom_size,g->level,g->damage.difficulty,
        g->frame_game.counter,g->score,level.elapsed,g->frame_game.flags,&g->players[0].inventory.ammo[2]);
}
int ssa_native_completion_record(SsaCompletionRecord *out,SsaCompletion *tally,
    const SsaNativeGame *g,const SsaStatisticsTotals *totals,uint8_t setting)
{
    SsaCompletionRecord next={0};SsaLevelStatistics level;
    if(!out || !tally || !totals || !tally->loaded || tally->updating || tally->running ||
       tally->stage!=6 || tally->recorded || ssa_native_game_statistics(&level,g)!=1 ||
       !g->finished || level.health<=0 || g->gameplay.life_game.input_state!=3 ||
       g->level!=tally->level || g->damage.difficulty!=tally->difficulty ||
       g->frame_game.counter!=tally->source_counter || g->score!=tally->source_score ||
       level.elapsed!=tally->elapsed ||
       memcmp(&g->players[0].inventory.ammo[2],tally->ammo,sizeof tally->ammo))return -1;
    if(ssa_weapon_progress_capture(&next.weapons,level.rows.weapons,&g->players[0].inventory,
       &g->gameplay.weapon,setting)!=1)return -1;
    level.score=tally->score;next.totals=*totals;
    if(ssa_statistics_complete(&next.totals,&level)!=1 ||
       ssa_statistics_checkpoint_capture(&next.checkpoint,&level)!=1)return -1;
    next.next_level=g->level+1u;*out=next;tally->recorded=1;return 1;
}
