#ifndef SSA_VULKAN_RENDERER_H
#define SSA_VULKAN_RENDERER_H
#include "ssa/pc_renderer.h"
typedef struct SsaVulkanRenderer SsaVulkanRenderer;
/* Cumulative wall-clock stages, for the isolated performance diagnostic.
 * Wait includes queue submission/synchronization, not just GPU execution. */
typedef struct SsaVulkanTiming {double prepare,wait,readback,effects;unsigned passes;} SsaVulkanTiming;
SsaVulkanTiming ssa_vulkan_timing(const SsaVulkanRenderer *);
/* Optional runtime-loaded Vulkan1.0; failure leaves both legacy modes usable.
 * Owned by the frontend, not a level. All calls are on its render thread. */
SsaVulkanRenderer *ssa_vulkan_open(void);
void ssa_vulkan_close(SsaVulkanRenderer *);
const SsaPcHardware *ssa_vulkan_backend(SsaVulkanRenderer *);
const char *ssa_vulkan_device(const SsaVulkanRenderer *);
unsigned ssa_vulkan_validation_errors(const SsaVulkanRenderer *);
#endif
