typedef struct TimerEntity {
    u8 pad00[0x30];
    u16 flags;
    u8 pad32[0x1c];
    s16 timer;
} TimerEntity;
typedef struct TimerGame { u8 pad00[7]; u8 ticks; } TimerGame;
extern TimerGame *D_03004010;

int sub_08012CFC(TimerEntity *entity) {
    if (!(entity->flags & 0x80)) {
        entity->timer -= D_03004010->ticks;
        if (entity->timer <= 0) {
            entity->timer = 0;
            return 1;
        }
    } else {
        entity->timer -= D_03004010->ticks;
        if (entity->timer <= 0) {
            entity->flags &= ~0x80;
            entity->timer = 30;
        }
    }
    return 0;
}

