typedef struct CheckpointGame {
    u8 pad[0x90];
    u32 weapons[10][4], enemies[12][3];
    u32 saved_weapons[10][4], saved_enemies[12][3];
} CheckpointGame;
extern CheckpointGame *D_03004010;
void sub_08001CF4(void) {
    u32 i;
    CheckpointGame *game;
    CheckpointGame *other;
    i = 0;
    do {
        game = D_03004010;
        game->saved_weapons[i][0] = game->weapons[i][0];
        game->saved_weapons[i][1] = game->weapons[i][1];
        game->saved_weapons[i][2] = game->weapons[i][2];
        game->saved_weapons[i][3] = game->weapons[i][3];
        ++i;
    } while (i <= 9);
    i = 0;
    do {
        other = D_03004010;
        other->saved_enemies[i][0] = other->enemies[i][0];
        other->saved_enemies[i][1] = other->enemies[i][1];
        other->saved_enemies[i][2] = other->enemies[i][2];
        ++i;
    } while (i <= 11);
}
