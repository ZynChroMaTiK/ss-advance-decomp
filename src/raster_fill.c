#include "ssa/raster_fill.h"

static int fill(uint8_t *dst,size_t capacity,size_t offset,uint32_t count,
                 uint8_t index,size_t stride)
{
    uint32_t i;
    if (!dst || !count || offset>=capacity ||
        (size_t)(count-1)>(capacity-offset-1)/stride) return -1;
    for (i=0;i<count;++i) {
        dst[offset]=index;
        if (i!=count-1) offset+=stride;
    }
    return 1;
}
int ssa_raster_fill_row(uint8_t *dst,size_t capacity,size_t offset,
                         uint32_t count,uint8_t index)
{return fill(dst,capacity,offset,count,index,1);}
int ssa_raster_fill_column(uint8_t *dst,size_t capacity,size_t offset,
                            uint32_t count,uint8_t index)
{return fill(dst,capacity,offset,count,index,120);}
int ssa_raster_copy_sky(uint8_t *dst,size_t capacity,size_t offset,
                        const uint8_t *src,size_t source_size,size_t source_offset,
                        uint32_t count)
{
    uint32_t i;
    if (!dst || !src || !count || offset>capacity || count>capacity-offset ||
        source_offset>source_size || count>source_size-source_offset) return -1;
    for (i=0;i<count;++i) dst[offset+i]=src[source_offset+i];
    return 1;
}
