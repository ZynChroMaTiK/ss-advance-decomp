#include "ssa/pc_input_runtime.h"
#include <string.h>

static int ready(const SsaPcInputRuntime *r) {
    unsigned i;
    if(!(r && r->input && r->input->yaw_per_count>0 && r->game && r->game->frame &&
        r->game->frame->game && r->game->entities && r->game->entities->combat &&
        r->game->entities->combat->player_count<=4 &&
        r->player_index<r->game->entities->combat->player_count &&
        r->game->entities->players[r->player_index] &&
        r->game->entities->players[r->player_index]->runtime))return 0;
    for(i=0;i<r->game->entities->combat->player_count;++i)
        if(!r->game->entities->players[i] || !r->game->entities->players[i]->runtime ||
           !r->game->entities->players[i]->runtime->player)return 0;
    return 1;
}
static void policy(SsaPcInputRuntime *r) {
    ssa_pc_input_enable(r->input,r->controls_enabled &&
        !(r->game->frame->game->flags&1) && r->game->life_game.input_state!=3 &&
        (r->game->entities->players[r->player_index]->runtime->player->motion.movement.flags&4));
    if((!r->input->enabled || !r->input->focused) && r->game->weapons[r->player_index]){
        r->game->weapons[r->player_index]->fire_pending_weapon=0;
        r->game->weapons[r->player_index]->time_strike_empty_pending=0;
    }
}
static void suppress_sample(SsaPcInputRuntime *r) {
    memset(&r->sample,0,sizeof(r->sample));
    if(r->polled && r->game->updating && r->game->stage==SSA_FRAME_POLL &&
       r->poll_serial==r->game->step_serial)r->game->frame->processed_input=0;
}
static int event_ready(SsaPcInputRuntime *r) {
    return ready(r) && (!r->game->updating || (r->game->stage==SSA_FRAME_POLL &&
        r->game->external_active));
}
int ssa_pc_input_runtime_init(SsaPcInputRuntime *r,SsaPcInput *p,SsaGameRuntime *g,unsigned index) {
    if(!r || r->input || !p || !g || g->updating)return -1;
    memset(r,0,sizeof(*r));r->input=p;r->game=g;r->player_index=index;r->controls_enabled=1;
    if(!ready(r)) {memset(r,0,sizeof(*r));return -1;}
    policy(r);return 0;
}
int ssa_pc_input_runtime_free(SsaPcInputRuntime *r) {
    if(!ready(r) || r->game->updating)return -1;
    memset(r,0,sizeof(*r));return 0;
}
void ssa_pc_input_runtime_enable(SsaPcInputRuntime *r,int enabled) {
    if(!event_ready(r))return;
    r->controls_enabled=(uint8_t)!!enabled;policy(r);
    if(!r->input->enabled || !r->input->focused)suppress_sample(r);
}
int ssa_pc_input_runtime_key(SsaPcInputRuntime *r,int key,int down,int repeat) {
    if(!event_ready(r))return -1;
    policy(r);return ssa_pc_input_key(r->input,key,down,repeat);
}
int ssa_pc_input_runtime_button(SsaPcInputRuntime *r,unsigned button,int down) {
    if(!event_ready(r))return -1;
    policy(r);return ssa_pc_input_button(r->input,button,down);
}
int ssa_pc_input_runtime_relative(SsaPcInputRuntime *r,int32_t dx,int32_t dy) {
    if(!event_ready(r))return -1;
    policy(r);return ssa_pc_input_relative(r->input,dx,dy);
}
void ssa_pc_input_runtime_focus(SsaPcInputRuntime *r,int focused) {
    if(!event_ready(r))return;
    ssa_pc_input_focus(r->input,focused);policy(r);
    if(!focused) {
        suppress_sample(r);
        ssa_player_runtime_focus_lost(r->game->entities->players[r->player_index]->runtime);
    }
}
int ssa_pc_input_runtime_poll(SsaPcInputRuntime *r) {
    SsaGameRuntime *g;
    if(!ready(r) || !r->game->updating || !r->game->external_active ||
        r->game->stage!=SSA_FRAME_POLL ||
        (r->sampled && r->poll_serial==r->game->step_serial))return -1;
    g=r->game;r->polled=0;policy(r);
    if(ssa_pc_input_sample(r->input,&r->sample)<0)return -1;
    g->frame->processed_input=r->sample.packed;
    r->polled=1;r->sampled=1;r->poll_serial=g->step_serial;r->last_error=0;
    /* Entity updates are skipped while paused, but jump edges must still be
     * consumed; a held disabled press cannot become a new jump on resume. */
    if(!r->sample.enabled)g->entities->players[r->player_index]->runtime->jump.was_down=0;
    return 0;
}
int ssa_pc_input_runtime_distribute(SsaPcInputRuntime *r) {
    SsaInputDispatch d={0};SsaGameRuntime *g;SsaPlayerRuntimeInput *p;unsigned i;int result;
    if(!ready(r) || !r->polled || !r->game->updating || !r->game->external_active ||
        r->game->stage!=SSA_FRAME_DISTRIBUTE || !r->game->weapon_game ||
        r->poll_serial!=r->game->step_serial)return -1;
    g=r->game;d.player_count=(uint8_t)g->entities->combat->player_count;
    policy(r);
    if(!r->input->enabled || !r->input->focused) {
        suppress_sample(r);g->frame->input=g->frame->processed_input=0;
    }
    d.game_flags=g->frame->game->flags;d.motion_index=(uint8_t)r->player_index;
    d.exit_flag=g->life_game.exit_flag;
    for(i=0;i<4;++i) {
        d.players[i].flags=i<d.player_count?&g->entities->players[i]->runtime->player->motion.movement.flags:
            &r->unused_flags[i];
        d.players[i].keys=&g->input.entities.players[i].keys;
    }
    result=ssa_input_dispatch_local(&d,g->frame->input);
    r->polled=0;r->last_error=result;
    if(result<0)return result;
    if(!(*d.players[r->player_index].flags&4))return 0;
    p=&g->input.entities.players[r->player_index];
    p->keys=r->sample.keys;p->yaw_delta=r->sample.yaw_delta;
    p->pitch_delta=r->sample.pitch_delta;
    p->jump_down=r->sample.jump_down;p->controls_enabled=r->sample.enabled;
    g->input.held[r->player_index]=r->sample.held;
    g->input.pressed[r->player_index]=r->sample.pressed;
    g->weapon_game->motion_index=(uint8_t)r->player_index;
    if(r->sample.view_toggle && r->sample.enabled) {
        SsaPlayerRuntime *player=g->entities->players[r->player_index]->runtime;
        ssa_player_runtime_key(player,'H',1,0,1);
        ssa_player_runtime_key(player,'H',0,0,1);
    }
    return 0;
}
