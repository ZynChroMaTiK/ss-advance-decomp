#include "ssa/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,submissions;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"scores line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void step(SsaMenu *m,unsigned input){CHECK(ssa_menu_step(m,input)==1);}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000),encoded[SSA_SCORES_BYTES],bad[SSA_SCORES_BYTES];FILE *f;
    SsaScores scores,before,decoded;SsaMenu *m=calloc(1,sizeof(*m));unsigned i,j,language;uint16_t *pixels=malloc(38402*sizeof(*pixels));
    CHECK(argc==2 && rom && m && pixels);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_scores_init(&scores,rom,0x800000)==1 && ssa_scores_valid(&scores));
    CHECK(scores.entries[0].score==2000000 && scores.entries[17].score==100000);
    CHECK(!memcmp(scores.entries[0].name,"ROBBER DOB  ",12) && !memcmp(scores.name,"____________",13));
    before=scores;CHECK(ssa_scores_init(&scores,rom,0x7fffff)==-1 && !memcmp(&scores,&before,sizeof scores));
    CHECK(ssa_scores_encode(&scores,encoded,sizeof encoded)==1);
    CHECK(ssa_scores_decode(&decoded,encoded,sizeof encoded)==1 && !memcmp(&scores,&decoded,sizeof scores));
    for(i=0;i<sizeof encoded;++i){memcpy(bad,encoded,sizeof bad);bad[i]^=1;decoded=before;
        CHECK(ssa_scores_decode(&decoded,bad,sizeof bad)==-1 && !memcmp(&decoded,&before,sizeof before));
        CHECK(ssa_scores_decode(&decoded,encoded,i)==-1 && !memcmp(&decoded,&before,sizeof before));}
    scores.entries[1].score=UINT32_MAX;CHECK(!ssa_scores_valid(&scores) && ssa_scores_encode(&scores,encoded,sizeof encoded)==-1);
    scores=before;scores.name[3]=0;CHECK(!ssa_scores_valid(&scores));scores=before;
    CHECK(ssa_scores_insert(&scores,99999,"LOW_________")==18 && !memcmp(&scores,&before,sizeof scores));
    CHECK(ssa_scores_insert(&scores,UINT32_MAX,"SAM_SERIOUS_")==0 && scores.entries[0].score==UINT32_MAX);
    CHECK(!memcmp(scores.entries[0].name,"SAM SERIOUS ",12) && !memcmp(scores.name,"SAM_SERIOUS_",13));
    CHECK(ssa_scores_insert(&scores,UINT32_MAX,"TIE_________")==0 && !memcmp(scores.entries[1].name,"SAM SERIOUS ",12));
    CHECK(ssa_scores_insert(&scores,UINT32_MAX,scores.name)==0 && ssa_scores_valid(&scores));
    for(language=0;language<3;++language)for(i=0;i<18;++i){unsigned rank,ticks;uint32_t score=before.entries[i].score;
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        m->fading=m->fade_volume=0;m->language=language;
        CHECK(ssa_menu_score_begin(m,score)==1);step(m,0);CHECK(m->page==SSA_MENU_SCORE_NAME && m->score_cursor==0);
        CHECK(ssa_menu_text(m,'s')==1 && ssa_menu_text(m,'a')==1 && ssa_menu_text(m,'x')==1);
        CHECK(ssa_menu_text(m,8)==1 && ssa_menu_text(m,'m')==1 && ssa_menu_text(m,' ')==1);
        CHECK(ssa_menu_text(m,'b')==1 && !memcmp(m->scores.name,"SAM_B_______",12));
        m->score_cursor=4;CHECK(ssa_menu_text(m,127)==1 && !memcmp(m->scores.name,"SAM_________",12));
        CHECK(ssa_menu_text(m,'2')==1 && ssa_menu_text(m,'!')==0);
        step(m,SSA_MENU_BACK);CHECK(m->page==SSA_MENU_SCORE_NAME); /* Original B does not discard a record. */
        pixels[0]=0x1234;pixels[38401]=0x4321;
        CHECK(ssa_menu_draw(m,pixels+1,38400)==1 && pixels[0]==0x1234 && pixels[38401]==0x4321);
        step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_SCORES && m->score_submitted && !m->done);
        rank=m->score_highlight-1;CHECK(rank==i && m->scores.entries[rank].score==score && !memcmp(m->scores.entries[rank].name,"SAM 2       ",12));
        CHECK(ssa_scores_valid(&m->scores));
        for(ticks=0;ticks<250;++ticks){step(m,0);if(!m->score_speed)break;}
        CHECK(ticks<250 && m->score_y==(i<6?16:i<12?-160:-336)*65536 && !m->score_submitted);
        CHECK(ssa_scores_encode(&m->scores,encoded,sizeof encoded)==1 && ssa_scores_decode(&decoded,encoded,sizeof encoded)==1);
        CHECK(!memcmp(decoded.entries,m->scores.entries,sizeof decoded.entries));++submissions;
    }
    CHECK(ssa_menu_score_begin(m,UINT32_MAX)==1);step(m,0);
    for(j=0;j<12;++j)CHECK(ssa_menu_text(m,'a'+j)==1);
    CHECK(ssa_menu_text(m,'z')==0 && m->score_cursor==12 && !memcmp(m->scores.name,"ABCDEFGHIJKL",13));
    step(m,SSA_MENU_UP);CHECK(m->score_cursor==11 && !m->score_typed && m->scores.name[11]=='M');
    free(pixels);free(m);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"typed_submissions\":%u,\"codec_bytes\":320,\"languages\":3}\n",checks,submissions);return 0;
}
