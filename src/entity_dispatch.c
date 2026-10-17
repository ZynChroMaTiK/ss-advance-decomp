#include "ssa/entity_dispatch.h"

uint32_t ssa_entity_dispatch_handler(uint16_t type)
{
    switch (type) {
    case 0x64: return 0x08010d7c;
    case 0x65: return 0x08010e5c;
    case 0x66: return 0x08010ef0;
    case 0x67: return 0x080111c0;
    case 0x68: return 0x08011350;
    case 0x69: return 0x080113c4;
    case 0x6a: return 0x08011090;
    case 0x6b: return 0x08011120;
    case 0x96: return 0x08014c58;
    case 0xc9: case 0xca: case 0xcb: case 0xcc: return 0x08005ccc;
    case 0xcd: return 0x08013978;
    case 0xce: return 0x080135c4;
    case 0xcf: return 0x08013040;
    case 0xd0: return 0x0801319c;
    case 0xd1: return 0x08013b40;
    case 0xd2: return 0x08013350;
    case 0xd3: return 0x08013fa4;
    case 0xd4: return 0x080141e8;
    case 0xd5: return 0x08013db0;
    case 0xd6: return 0x08013780;
    case 0xd7: return 0x08014ab8;
    case 0xd8: return 0x08014648;
    case 0xe1: return 0x0801b848;
    case 0xe2: return 0x080113e4;
    case 0xe3: return 0x08011424;
    default: return 0;
    }
}
static int known(const SsaEntityDispatch *d,const SsaEntity *e)
{
    unsigned i;
    for (i=0;i<100;++i) if (d->entities[i]==e) return 1;
    return 0;
}
int ssa_entity_dispatch_step(SsaEntityDispatch *d,const SsaEntityDispatchOps *ops)
{
    unsigned i,slot;
    int result=-1;
    if (!d || d->updating || !d->pool || d->pool->active_count>100 ||
        !d->entities || !ops || !ops->handler) return -1;
    for (i=0;i<100;++i) if (!d->entities[i]) return -1;
    for (i=0;i<d->pool->active_count;++i) if (d->pool->active_slots[i]>=100) return -1;
    d->updating=1;
    ++d->frame_counter;
    d->walk_count=d->path_count=0;
    d->auxiliary_count=0;
    for (i=0;i<d->pool->active_count;++i) {
        uint32_t entry;
        if (d->pool->active_count>100 || i>=100 || d->pool->active_slots[i]>=100) goto done;
        slot=d->pool->active_slots[i];
        d->current=d->entities[slot];
        entry=ssa_entity_dispatch_handler(d->current->type);
        if (entry && ops->handler(ops->context,d,entry,slot)<0) goto done;
        if (!known(d,d->current) || d->pool->active_count>100) goto done;
        if (d->current->type) {
            uint16_t next=i==99?d->active_tail:d->pool->active_slots[i+1];
            d->auxiliary[d->auxiliary_count++]=(uint16_t)(next+1u);
        }
    }
    result=0;
done:
    d->auxiliary[d->auxiliary_count]=0;
    d->current=d->entities[0];
    d->updating=0;
    return result;
}
