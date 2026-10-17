typedef struct CooldownGame { u8 pad[7]; u8 ticks; } CooldownGame;
extern CooldownGame *D_03004010;
typedef struct CooldownPlayer { u8 pad[0x4A]; s16 timers[10]; } CooldownPlayer;

void sub_08012200(CooldownPlayer *player) {
    u32 i = 0;
    CooldownGame *game = D_03004010;
    for(i=0;i<10;++i) {
        player->timers[i] -= game->ticks;
        if(player->timers[i] < 0) player->timers[i]=0;
    }
}
