#ifndef SSA_PC_HARDWARE_H
#define SSA_PC_HARDWARE_H
#include <stdint.h>
struct SsaPcRenderer;
/* Prepared near-clipped view geometry. Textures are column-major palette
 * indices, borrowed until end(). A backend never owns gameplay or HUD data.
 * Actor triangles arrive back-to-front in stable source/layer order. Test
 * them against world depth, but do not let sprites depth-test each other. */
typedef struct SsaPcVertex {double x,y,z,u,v;} SsaPcVertex;
typedef struct SsaPcMaterial {
    const uint8_t *pixels;unsigned width,height,color,transparent,actor,sky,decals;
    double height_z;
} SsaPcMaterial;
typedef struct SsaPcSample {float u,v,z,code;} SsaPcSample;
/* A row job may only write its assigned rows. Dispatch is synchronous: all
 * callbacks finish before returning, with borrowed inputs still alive. */
typedef void (*SsaPcRows)(void *,unsigned first,unsigned end);
typedef struct SsaPcHardware {
    void *context;
    /* Only [first,end) rows are consumed; preserve full viewport projection. */
    int (*begin)(void *,struct SsaPcRenderer *,const uint32_t palette[256],double focal,unsigned first,unsigned end);
    int (*triangle)(void *,const SsaPcVertex vertices[3],const SsaPcMaterial *);
    /* Samples retain full-width indexing, valid only in begin's row interval,
     * and remain borrowed until the next begin/end on this backend. */
    int (*end)(void *,struct SsaPcRenderer *,const SsaPcSample **);
    void (*rows)(void *,unsigned first,unsigned end,SsaPcRows,void *);
} SsaPcHardware;
#endif
