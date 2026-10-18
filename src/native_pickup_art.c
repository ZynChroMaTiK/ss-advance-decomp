#include "ssa/native_pickup_art.h"
#include <string.h>
#include "native_laurel.inc"

int ssa_native_pickup_art_apply(SsaActorAssets *assets,unsigned level)
{
    SsaActorResource *resource;SsaActorImage *full,*distant;
    if(!assets || !assets->loaded || level>=16)return -1;
    if(level<6 || level==12 || level==13)return 0;
    resource=&assets->resources[63];
    if(!resource->images || resource->count!=56 || resource->directions!=1 ||
       resource->mip_flags!=10 || resource->frames!=28)return -1;
    full=&resource->images[27];distant=&resource->images[55];
    if(!full->pixels || full->width!=20 || full->height!=34 ||
       full->origin_x!=-9 || full->origin_y!=-35 || !distant->pixels ||
       distant->width!=10 || distant->height!=17 ||
       distant->origin_x!=-4 || distant->origin_y!=-15)return -1;
    memcpy(full->pixels,ssa_laurel_full,sizeof(ssa_laurel_full));
    memcpy(distant->pixels,ssa_laurel_distant,sizeof(ssa_laurel_distant));
    return 1;
}
