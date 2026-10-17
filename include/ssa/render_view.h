#ifndef SSA_RENDER_VIEW_H
#define SSA_RENDER_VIEW_H
#include "ssa/movement.h"
#include "ssa/view.h"

/* Original03004020 camera scalars, host pointers excluded. Coordinates16.16;
 * Host layout is not a GBA overlay: original sector is at+18. Sector describes
 * the camera's containing sector. sine/cosine use NEGATEDyaw
 * as written by02B88, unlike gameplay's positive-heading vectors. */
typedef struct SsaRenderCamera {
    int32_t x,y,z;
    uint16_t heading;
    int16_t sector;
    int32_t sine,cosine;
} SsaRenderCamera;
typedef struct SsaRenderViewPlayer {
    uint16_t flags; /* BC:4 active,8 automap */
    uint32_t keys;  /* A4:held1/2 zoom; scan all4 slots */
    uint32_t zoom;  /* A8:unsigned16.16 automap scale */
} SsaRenderViewPlayer;
typedef struct SsaRenderView {
    SsaRenderViewPlayer players[4];
    SsaRenderCamera camera;
    const SsaMathTables *math;
    uint16_t local_index; /*0300464E, displaylocal */
    uint16_t overlay;     /*03004180, selects02CA0 */
    uint8_t ticks;        /*GAME+7 */
    unsigned updating;
} SsaRenderView;
typedef enum SsaRenderViewEvent {
    SSA_RENDER_VIEW_CLEAR=0x08007C94,
    SSA_RENDER_VIEW_AUTOMAP=0x08008420,
    SSA_RENDER_VIEW_WORLD=0x0800B6B0,
    SSA_RENDER_VIEW_OVERLAY=0x08002CA0
} SsaRenderViewEvent;
typedef struct SsaRenderViewOps {
    /* Clear arg0; automap argselectedzoom; otherargs0 (WORLD's original map
     * pointer is represented by caller context). Scalar changes visible to
     * subsequent reads. Bindings/ops/updating and all storage stay stable. */
    int (*event)(void *,SsaRenderView *,SsaRenderViewEvent,uint32_t argument);
    void *context;
} SsaRenderViewOps;

/* Complete original02B88 orchestration with native94E8 sincos. Remaining
 * rendering stages are mandatory events when reached. Return0/-1, no nested
 * update, failures retain prefix effects and clear guard. This is camera/mode
 * selection, not a scene renderer or a framebuffer presentation frontend. */
int ssa_render_view_step(SsaRenderView *,const SsaRenderViewOps *);
/* Native PC camera adapter. Borrow pose only for this copy; a displaced chase
 * pose resolves its own containing sector in the supplied map. Cannot run
 * during rendering; invalid geometry leaves camera unchanged.
 * Copies eye/chase coordinates plus gameplay yaw; leaves cached sincos until
 * the next world step. No pitch/tilt, body rendering or gameplay mutation. */
int ssa_render_view_pose(SsaRenderView *,const SsaViewPose *,uint16_t heading,
                           const SsaMapAssets *map);
#endif
