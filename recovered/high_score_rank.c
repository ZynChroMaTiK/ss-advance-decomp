/* Original unsigned ranking: an equal score takes the earlier slot. */
typedef struct ScoreRecord {u32 score;u8 name[12];} ScoreRecord;
typedef struct ScoreMenu {u8 pad[0x40];ScoreRecord entries[18];} ScoreMenu;
extern ScoreMenu *D_0300572C;
u32 sub_08016F70(u32 score) {
    ScoreMenu *menu=D_0300572C;
    ScoreRecord *entry=menu->entries;
    u32 rank=0;
    if(menu->entries[0].score>score){
again:
        ++rank;
        ++entry;
        if(rank<=17){if(entry->score>score)goto again;}
    }
    return rank;
}
