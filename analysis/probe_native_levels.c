/* Read-only ROM reconnaissance through the native composition. */
#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g));FILE *f;unsigned level;
 if(argc!=2 || !rom || !g || !(f=fopen(argv[1],"rb")))return 1;
 if(fread(rom,1,0x800000,f)!=0x800000)return 1;
 fclose(f);
 for(level=0;level<14;++level){unsigned i,tick;
  if(ssa_native_game_init(g,rom,0x800000,level)!=1){printf("level%u init error%u\n",level,g->error_line);return 1;}
  printf("level%u objects%zu start%d,%d\n",level,g->plan.object_count,g->players[0].entity.body.x/65536,g->players[0].entity.body.y/65536);
  for(i=0;i<100;++i)if(g->spawn.actors[i].owned){SsaScriptActor *a=&g->spawn.actors[i];
   printf(" slot%u type%x anim%x at%d,%d group%u\n",i,a->entity.type,a->animation,a->entity.body.x/65536,a->entity.body.y/65536,a->script.trigger);}
  ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
  for(tick=0;tick<600;++tick){ssa_pc_input_runtime_key(&g->input_runtime,'W',tick%120<60,0);
   ssa_pc_input_runtime_relative(&g->input_runtime,32,0);
   if(ssa_native_game_tick(g)!=1)break;}
  printf(" ticks%u error%u stage%x entry%x health%d\n",tick,g->error_line,g->error_stage,g->error_entry,g->players[0].inventory.health);
  ssa_native_game_free(g);
 }
 free(g);free(rom);return 0;
}
