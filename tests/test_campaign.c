/* Complete consecutive original maps using only earned inventory and PC input. */
#define SSA_SECOND_LEVEL_EMBED
#include "test_second_level.c"

int main(int argc,char **argv){
 FILE *f;uint8_t *rom;unsigned level,first=2,last=SSA_NATIVE_CAMPAIGN_LEVELS-1;int result=0,resume=0,fresh=0;
 /* Development probes may reuse an earned entry snapshot. Only the default
  * uninterrupted run is accepted by test_campaign.py as campaign evidence. */
 if(argc==4 && !strcmp(argv[1],"--resume")){resume=1;++argv;--argc;}
 if(argc==3 && !strcmp(argv[1],"--fresh")){fresh=1;++argv;--argc;}
 if(argc>3)return 2;
 if(argc>=2){char *end;unsigned long value=strtoul(argv[1],&end,10);
  if(!argv[1][0] || *end || value<2 || value>11)return 2;
  last=(unsigned)value;
 }
 if(resume){
  if(!(f=fopen(argv[2],"rb")))return 1;
  if(fread(&earned,1,sizeof earned,f)!=sizeof earned || fgetc(f)!=EOF){fclose(f);return 1;}
  fclose(f);first=last;fprintf(stderr,"Development probe: resumed earned entry; not consecutive campaign proof.\n");
 }else if(!fresh && second_level_run())return 1;
 rom=malloc(0x800000);if(!rom || !(f=fopen("orig/SSA.gba","rb")))return 1;
 if(fread(rom,1,0x800000,f)!=0x800000)return 1;
 fclose(f);
 if(fresh){SsaNativeGame *g=calloc(1,sizeof(*g));first=last;
  if(!g || ssa_native_game_init_campaign(g,rom,0x800000,last,1,NULL)!=1 || ssa_native_game_carry_capture(&earned,g)!=1){free(g);free(rom);return 1;}
  ssa_native_game_free(g);free(g);
  fprintf(stderr,"Development probe: original standalone level loadout; not consecutive campaign proof.\n");
 }
 for(level=first;level<=last && !result;++level){
  /* Keep separately named earned entries when a development run stops before
   * its requested last map. Never use these as consecutive-run evidence. */
  if(argc==3 && !resume){char path[512];
   if(snprintf(path,sizeof path,"%s.level%u",argv[2],level)>=(int)sizeof path || !(f=fopen(path,"wb"))){free(rom);return 1;}
   if(fwrite(&earned,1,sizeof earned,f)!=sizeof earned){fclose(f);free(rom);return 1;}
   if(fclose(f)){free(rom);return 1;}
  }
  if(argc==3 && !resume && level==last){
   if(!(f=fopen(argv[2],"wb"))){free(rom);return 1;}
   if(fwrite(&earned,1,sizeof earned,f)!=sizeof earned){fclose(f);free(rom);return 1;}
   if(fclose(f)){free(rom);return 1;}
  }
  result=campaign_level_run(rom,level,&earned,&earned);
 }
 free(rom);return result;
}
