struct ActorQueueCounters {
    unsigned short head, tail, wall_count, actor_count;
};
extern struct ActorQueueCounters D_03003F60;
unsigned int sub_08000F98(void)
{
    unsigned int result;
    if (D_03003F60.actor_count >= 32)
        result = 65535;
    else
        result = D_03003F60.actor_count++;
    return result;
}

