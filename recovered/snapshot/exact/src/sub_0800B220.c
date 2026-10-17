#include "common.h"

extern int sub_0800B1B0(const int *, const int *, const int *);
int sub_0800B220(const int *camera, const short *node)
{
    int direction[2], origin[2];
    direction[0] = node[2];
    direction[1] = node[3];
    origin[0] = node[0];
    origin[1] = node[1];
    return sub_0800B1B0(camera, origin, direction);
}
