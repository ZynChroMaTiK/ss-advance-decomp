#ifndef SSA_RAY_TRAVERSAL_H
#define SSA_RAY_TRAVERSAL_H
#include "ssa/slide.h"

typedef SsaTraceVector SsaRay;
typedef struct SsaRayIntercept { int32_t fraction; unsigned line; } SsaRayIntercept;
typedef struct SsaRayTraversal {
    SsaRay ray;
    SsaRayIntercept *intercepts;
    size_t count, capacity;
    int32_t blocking_fraction;
    unsigned updating;
} SsaRayTraversal;
typedef int (*SsaRayCollectCell)(void *context, SsaRayTraversal *traversal,
                                int32_t x, int32_t y);
typedef int (*SsaRayPredicate)(void *context, const SsaRayIntercept *intercept);
/* Zero-initialize the state and supply stable intercept storage/capacity.
 * Original 9C00: signed ascending fractions, stable ties, inclusive maximum.
 * Accepted records are consumed by setting fraction to INT32_MAX. A rejected
 * record remains unchanged and supplies blocking_fraction. Return 1 clear,
 * 0 blocked, -1 invalid/reentry/callback failure; earlier effects persist.
 * Predicates may inspect state but must not mutate or reenter this workspace. */
int ssa_ray_traverse_intercepts(SsaRayTraversal *traversal, int32_t maximum,
                               SsaRayPredicate predicate, void *context);
/* Original A240 flags=1 wall-cell traversal and subsequent 9C00 processing.
 * Resets count but preserves blocking_fraction until a predicate rejects.
 * Visits at most 64 cells, including out-of-map cells handled by collector.
 * Collector returns 1 continue, 0 blocked, -1 error and may append records.
 * No entity tracing. Caller resets its own per-query line-visited storage. */
int ssa_ray_trace(SsaRayTraversal *traversal, int32_t origin_x, int32_t origin_y,
                  int32_t x, int32_t y, int32_t end_x, int32_t end_y,
                  SsaRayCollectCell collect, SsaRayPredicate predicate,
                  void *context);
#endif
