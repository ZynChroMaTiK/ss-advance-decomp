/* Earn all inventory in levels0/1, then complete level2 using only PC input. */
#define SSA_SECOND_LEVEL_EMBED
#include "test_second_level.c"

int main(void){
 FILE *f;uint8_t *rom;int result;
 if(second_level_run())return 1;
 rom=malloc(0x800000);if(!rom || !(f=fopen("orig/SSA.gba","rb")))return 1;
 if(fread(rom,1,0x800000,f)!=0x800000)return 1;
 fclose(f);result=campaign_level_run(rom,2,&earned,&earned);free(rom);return result;
}
