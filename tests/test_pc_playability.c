/* Run the established normal-input completion with independent high-resolution
 * frames sampled along the entire route. Simulation must finish identically. */
#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
static SsaPcRenderer pc;
static unsigned pc_frames,pc_pitched_frames,pc_failures,pc_steps;
static int checked_step(SsaNativeGame *g){
 int result=ssa_native_game_tick(g);
 if(result==1)++pc_steps;
 if(result==1 && g->frame_game.counter%60==0){
  int32_t pitch=g->render_pitch;
  if(ssa_native_game_prepare_view(g)<0 || ssa_pc_renderer_resize(&pc,1280,720)<0 || ssa_pc_renderer_draw(&pc,g)<0){++pc_failures;return -1;}
  ++pc_frames;
  g->render_pitch=(pc_frames&1)?5440:-5440;
  if(ssa_pc_renderer_draw(&pc,g)<0){++pc_failures;g->render_pitch=pitch;return -1;}
  g->render_pitch=pitch;++pc_pitched_frames;
 }
 return result;
}
#define ssa_native_game_step checked_step
#define main bot_main
#include "test_native_playability.c"
#undef main
#undef ssa_native_game_step
int main(int argc,char **argv){int result=bot_main(argc,argv);
 ssa_pc_renderer_free(&pc);
 printf("{\"pc_frames\":%u,\"pc_pitched_frames\":%u,\"pc_failures\":%u,\"width\":1280,\"height\":720}\n",pc_frames,pc_pitched_frames,pc_failures);
 return result || pc_failures || !pc_frames || pc_frames!=pc_steps/60 || pc_pitched_frames!=pc_frames;
}
