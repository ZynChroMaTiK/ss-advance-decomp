#include "ssa/run_exit.h"
int ssa_run_exit(SsaPlayerLifecycleGame *life,uint16_t *flags)
{
    int destination;
    if(!life || !flags || !life->player_count || life->player_count>4 ||
       (life->input_state!=4 && life->input_state!=5))return -1;
    life->message_ticks=0;
    if(life->input_state==4 && life->continue_flag){
        --life->continue_flag;life->input_state=0;destination=SSA_RUN_RETRY;
    }else{
        destination=life->input_state==4?SSA_RUN_SCORES:SSA_RUN_MENU;
        life->input_state=7;
    }
    *flags=2;return destination;
}
