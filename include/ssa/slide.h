#ifndef SSA_SLIDE_H
#define SSA_SLIDE_H
#include "ssa/geometry.h"
#include "ssa/movement.h"

typedef struct SsaTraceVector { int32_t x, y, dx, dy; } SsaTraceVector;
typedef struct SsaWallIntercept { int32_t fraction; uint16_t line; } SsaWallIntercept;
/* Independent scratch storage: tracing must not overwrite a pending gameplay
 * collision query. Keep the loaded map and its layout stable until free. */
typedef struct SsaSlide {
    const SsaMapAssets *map;
    uint8_t *visited;
    SsaWallIntercept *intercepts;
    size_t capacity, count;
} SsaSlide;
typedef int (*SsaSlideTryMove)(void *context, SsaBody *body, int32_t x, int32_t y);
int ssa_slide_init(SsaSlide *slide, const SsaMapAssets *map);
void ssa_slide_free(SsaSlide *slide);
int32_t ssa_trace_divide(int32_t numerator, int32_t denominator);
int ssa_trace_side(int32_t x, int32_t y, const SsaTraceVector *line);
int32_t ssa_trace_intercept(const SsaTraceVector *trace, const SsaTraceVector *wall);
/* Original flags=1 wall-only path of 0800A240. The result list retains blockmap
 * insertion order; equal-fraction contacts retain that order when processed.
 * Returns 1 or -1 for invalid geometry; no dynamic/entity trace support. */
int ssa_slide_trace(SsaSlide *slide, int32_t x, int32_t y, int32_t end_x, int32_t end_y);
/* Original 08004930 projection, including whole-unit velocity quantization.
 * Returns 1 or -1; on invalid input output velocity is unchanged. */
int ssa_slide_project(const SsaMapAssets *map, uint16_t line, int32_t x, int32_t y,
                      const SsaMathTables *tables, int32_t *velocity_x, int32_t *velocity_y);
/* Whole 08004A38 behavior, using caller's static or dynamic move/commit policy.
 * Returns 1 for a move accepted, 0 for blocked/no further displacement, -1 for
 * invalid input/query. An error can follow earlier committed movement.
 * This routine does not apply the horizontal timestep velocity clamp. */
int ssa_slide_move(SsaSlide *slide, SsaBody *body, int32_t radius,
                   const SsaMathTables *tables, SsaSlideTryMove try_move, void *context);
#endif
