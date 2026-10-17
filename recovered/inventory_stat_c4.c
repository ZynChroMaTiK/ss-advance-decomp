typedef struct PickupStats {
    u8 padding[0xc4];
    s16 value;
} PickupStats;

void sub_080054D8(PickupStats *player, s16 amount) {
    if (amount == 100) {
        player->value += 100;
    } else if (player->value < 100) {
        player->value += amount;
        if (player->value > 100) player->value = 100;
    }
}

