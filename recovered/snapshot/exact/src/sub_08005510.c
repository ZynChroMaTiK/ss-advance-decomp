#include "common.h"

typedef struct PickupStats {
    u8 padding[0xc6];
    s16 value;
} PickupStats;

void sub_08005510(PickupStats *player, s16 amount) {
    if (amount == 100) {
        player->value += 100;
    } else if (player->value < 100) {
        player->value += amount;
        if (player->value > 100) player->value = 100;
    }
}
