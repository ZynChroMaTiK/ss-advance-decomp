#ifndef SSA_AUTOMAP_H
#define SSA_AUTOMAP_H
#include "ssa/map_assets.h"
#include "ssa/movement.h"
typedef struct SsaAutomapPoint {int32_t x,y;} SsaAutomapPoint;
typedef struct SsaAutomapView {
    int32_t x,y,player_x,player_y;
    uint32_t zoom;
    uint16_t heading;
    uint8_t facing,scanline;
    const SsaAutomapPoint *entities;
    size_t entity_count;
    const uint16_t *contacts;
    size_t contact_count;
} SsaAutomapView;
typedef int (*SsaAutomapLine)(void *,int32_t,int32_t,int32_t,int32_t,unsigned);
/* Normal08420 map/interaction/player/entity geometry. All values borrowed;
 * scene emission and drawing never change the world. Original debug ray-list
 * mutation is outside this normal-gameplay projection. Return1/-1. */
int ssa_automap_scene(const SsaMapAssets *,const SsaMathTables *,const SsaAutomapView *,SsaAutomapLine,void *);
/* Original07FDC clipped240x160 indexed line, including OR pixels, endpoint
 * and degenerate-point rules. Returns1/-1; invalid data changes no pixels.
 * Original invalid post-clip infinite loop is a native error instead. */
int ssa_automap_line(uint8_t *,size_t,int32_t,int32_t,int32_t,int32_t,unsigned);
int ssa_automap_draw(uint8_t *,size_t,const SsaMapAssets *,const SsaMathTables *,const SsaAutomapView *);
#endif
