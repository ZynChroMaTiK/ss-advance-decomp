#include "common.h"

typedef struct {
    s32 x, y, z, vx, vy, vz;
    u8 padding[0xa0];
    u16 heading;
} SyncPlayer;
typedef struct {
    s32 x, y, z, vx, vy, vz;
    u8 padding[0x27];
    u8 facing;
} SyncEntity;
void sub_08005E04(SyncPlayer *player, SyncEntity *entity) {
    entity->x=player->x;
    entity->y=player->y;
    entity->z=player->z;
    entity->vx=player->vx;
    entity->vy=player->vy;
    entity->vz=player->vz;
    entity->facing=player->heading>>8;
}
