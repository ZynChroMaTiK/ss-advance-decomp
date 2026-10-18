typedef struct StatisticsGame {
    u8 pad0[0x0c];
    u32 elapsed;
    u8 pad10[0x6c];
    u8 level;
    u8 pad7d[11];
    u32 score;
    u8 pad8c[4];
    u32 weapons[10][4];
    u32 enemies[12][3];
    u8 pad1c0[0x130];
    u32 saved_score;
    s32 health, armor6, armor4;
    u32 saved_level, total_elapsed, spawned, kills, hits, weapon_hits;
    u32 total_pickups, pickups;
} StatisticsGame;
typedef struct StatisticsBody { u8 pad[0x32]; s16 health; } StatisticsBody;
typedef struct StatisticsPlayer {
    u8 pad[0xac];
    StatisticsBody *body;
    u8 padb0[0x14];
    s16 armor4, armor6;
} StatisticsPlayer;
extern StatisticsGame *D_03004010;
extern StatisticsPlayer *D_03004648;
void sub_08001E48(void) {
    u32 i;
    StatisticsGame *game;
    D_03004010->total_elapsed += D_03004010->elapsed;
    i = 0;
    do {
        D_03004010->weapon_hits += D_03004010->weapons[i][3];
        ++i;
    } while (i <= 9);
    i = 0;
    do {
        game = D_03004010;
        game->spawned += game->enemies[i][0];
        game->kills += game->enemies[i][2];
        game->hits += game->enemies[i][1];
        ++i;
    } while (i <= 11);
    game->total_pickups += game->pickups;
    game->saved_score = game->score;
    game->health = D_03004648->body->health;
    game->armor6 = D_03004648->armor6;
    game->armor4 = D_03004648->armor4;
    game->saved_level = game->level;
}
