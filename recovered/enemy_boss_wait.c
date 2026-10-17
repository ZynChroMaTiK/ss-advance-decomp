typedef struct BossPlayerRecord { u8 pad[0xAC]; void *body; } BossPlayerRecord;
extern BossPlayerRecord *D_03004648;
extern u8 D_03005714;
extern u8 *D_030057B0;
extern u8 D_0844C078[];
extern void sub_08014518(void *, void *);
extern void sub_080147D4(void *);
extern void sub_08014734(void *);
void sub_08014804(void *entity) {
    u8 remaining;
    u8 *entry;
    sub_08014518(entity, D_03004648->body);
    entry = D_0844C078 + D_03005714;
    remaining = *(*entry + D_030057B0);
    if (remaining == 0) {
        if (++D_03005714 > 1) D_03005714 = remaining;
        sub_080147D4(entity);
    }
    sub_08014734(entity);
}
