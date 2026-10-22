#include "vulkan_renderer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#ifdef SSA_HAVE_VULKAN
#define WIN32_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>
#include "ssa_scene_vert.h"
#include "ssa_scene_frag.h"
#include "ssa_post_comp.h"

#define INSTANCE_FUNCTIONS(X) \
 X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties) \
 X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceQueueFamilyProperties) \
 X(GetPhysicalDeviceFormatProperties) X(CreateDevice) X(GetDeviceProcAddr)
#define DEVICE_FUNCTIONS(X) \
 X(DestroyDevice) X(DeviceWaitIdle) X(GetDeviceQueue) X(CreateBuffer) X(DestroyBuffer) \
 X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(InvalidateMappedMemoryRanges) \
 X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView) X(DestroyImageView) \
 X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
 X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) X(DestroyDescriptorPool) \
 X(AllocateDescriptorSets) X(UpdateDescriptorSets) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreateGraphicsPipelines) X(CreateComputePipelines) X(DestroyPipeline) \
 X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandPool) \
 X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdPipelineBarrier) X(CmdCopyBufferToImage) X(CmdCopyImageToBuffer) \
 X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdBindVertexBuffers) \
 X(CmdPushConstants) X(CmdSetViewport) X(CmdSetScissor) X(CmdDraw) X(CmdDispatch) X(CmdCopyBuffer) \
 X(CreateFence) X(DestroyFence) X(ResetFences) X(WaitForFences) X(QueueSubmit)
typedef enum BufferMemory {BUFFER_UPLOAD,BUFFER_READBACK,BUFFER_DEVICE} BufferMemory;
typedef struct Buffer {VkBuffer buffer;VkDeviceMemory memory;void *mapped;VkDeviceSize size;VkMemoryPropertyFlags flags;} Buffer;
typedef struct Image {VkImage image;VkDeviceMemory memory;VkImageView view;} Image;
typedef struct Vertex {float x,y,z,u,v;uint32_t material;} Vertex;
typedef struct Material {uint32_t offset,width,height,code;uint32_t mips[2][4];} Material;
enum { LOOKUP_BUCKETS=1024, TEXTURE_CACHE_TEXELS=4*1024*1024, TEXTURE_CACHE_ENTRIES=4096 };
typedef struct TextureEntry {
    uintptr_t address;uint8_t *copy;size_t count;
    unsigned width,height,offset,next,seen,dirty;
} TextureEntry;
struct SsaVulkanRenderer {
    HMODULE loader;PFN_vkGetInstanceProcAddr get;
#define DECLARE(name) PFN_vk##name name;
    INSTANCE_FUNCTIONS(DECLARE) DEVICE_FUNCTIONS(DECLARE)
#undef DECLARE
    VkInstance instance;VkPhysicalDevice physical;VkDevice device;VkQueue queue;
    VkPhysicalDeviceMemoryProperties memory;VkPhysicalDeviceProperties properties;
    VkDebugUtilsMessengerEXT debug;PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug;
    unsigned errors,width,height,family,recording,first_row,end_row;int failed;
    VkRenderPass pass;VkFramebuffer framebuffer;VkPipelineLayout layout;VkPipeline pipeline[2];
    VkDescriptorSetLayout descriptors;VkDescriptorPool descriptor_pool;VkDescriptorSet set;
    VkCommandPool pool;VkCommandBuffer commands;VkFence fence;
    Image color,aux,depth;
    VkPipeline post_pipeline;VkPipelineLayout post_layout;VkDescriptorSetLayout post_descriptors;
    VkDescriptorPool post_pool;VkDescriptorSet post_set;Buffer post_input,post_work;
    SsaPcRenderer *post_target;unsigned post_width,post_height; /* Borrowed until consumed; GPU samples survive reflections. */
    SsaPcRenderer *motion_target;SsaPcMotionView motion_previous;
    double motion_time;float motion_focal;unsigned motion_width,motion_height;
    Buffer vertices,textures,materials,upload,readback;
    SsaPcSample *cpu_samples;size_t sample_capacity;
    Vertex *cpu_vertices;size_t vertex_count,vertex_capacity;
    Material *cpu_materials;SsaPcMaterial *keys;size_t material_count,material_capacity;
    uint32_t *cpu_textures;size_t texture_count,texture_capacity;
    unsigned material_buckets[LOOKUP_BUCKETS],*material_next;
    unsigned texture_buckets[LOOKUP_BUCKETS],texture_epoch;
    TextureEntry *texture_entries;size_t entry_count,entry_capacity;
    SsaPcRenderer *target;float scale[2];SsaPcHardware backend;
    SsaVulkanTiming timing;
    PTP_POOL workers;TP_CALLBACK_ENVIRON worker_environment;unsigned worker_count;
};
#define V(name) v->name
static double wall_time(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart/f.QuadPart;}
#define OK(call) do{VkResult ssa_vk_result=(call);if(ssa_vk_result!=VK_SUCCESS){fprintf(stderr,"Vulkan %s failed (%d)\n",#call,(int)ssa_vk_result);return 0;}}while(0)
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,const VkDebugUtilsMessengerCallbackDataEXT *data,void *context)
{
    SsaVulkanRenderer *v=context;(void)type;
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){++v->errors;fprintf(stderr,"Vulkan validation: %s\n",data->pMessage);}
    return VK_FALSE;
}
static int memory_type(SsaVulkanRenderer *v,uint32_t bits,VkMemoryPropertyFlags flags,uint32_t *index)
{
    uint32_t i;for(i=0;i<v->memory.memoryTypeCount;++i)if((bits&(1u<<i)) && (v->memory.memoryTypes[i].propertyFlags&flags)==flags){*index=i;return 1;}
    return 0;
}
static void buffer_free(SsaVulkanRenderer *v,Buffer *b)
{
    if(b->mapped)V(UnmapMemory)(v->device,b->memory);
    if(b->buffer)V(DestroyBuffer)(v->device,b->buffer,NULL);
    if(b->memory)V(FreeMemory)(v->device,b->memory,NULL);
    memset(b,0,sizeof(*b));
}
static int buffer_size(SsaVulkanRenderer *v,Buffer *b,VkDeviceSize size,VkBufferUsageFlags usage,BufferMemory memory)
{
    VkBufferCreateInfo ci={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};VkMemoryRequirements req;
    VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};VkDeviceSize capacity=65536;
    if(b->size>=size)return 1;
    /* Turning exposes more materials/triangles. Grow geometrically, not once
     * per newly visible triangle, and only after the preceding fence completed. */
    while(capacity<size){if(capacity>UINT64_MAX/2)return 0;capacity*=2;}
    buffer_free(v,b);ci.size=capacity;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    OK(V(CreateBuffer)(v->device,&ci,NULL,&b->buffer));V(GetBufferMemoryRequirements)(v->device,b->buffer,&req);
    ai.allocationSize=req.size;
    /* Upload memory can be uncached/write-combined. Reading each sample from
     * that mapping stalls the CPU repeatedly; downloads need HOST_CACHED.
     * Prefer coherent cached memory, but support cached non-coherent heaps. */
    if(memory==BUFFER_DEVICE){
        if(!memory_type(v,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&ai.memoryTypeIndex))return 0;
    }else if(!(memory==BUFFER_READBACK &&
         (memory_type(v,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&ai.memoryTypeIndex) ||
          memory_type(v,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT,&ai.memoryTypeIndex))) &&
       !memory_type(v,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&ai.memoryTypeIndex))return 0;
    b->flags=v->memory.memoryTypes[ai.memoryTypeIndex].propertyFlags;
    OK(V(AllocateMemory)(v->device,&ai,NULL,&b->memory));OK(V(BindBufferMemory)(v->device,b->buffer,b->memory,0));
    if(memory!=BUFFER_DEVICE){OK(V(MapMemory)(v->device,b->memory,0,VK_WHOLE_SIZE,0,&b->mapped));}
    b->size=capacity;return 1;
}
static void image_free(SsaVulkanRenderer *v,Image *i)
{
    if(i->view)V(DestroyImageView)(v->device,i->view,NULL);
    if(i->image)V(DestroyImage)(v->device,i->image,NULL);
    if(i->memory)V(FreeMemory)(v->device,i->memory,NULL);memset(i,0,sizeof(*i));
}
static int image_new(SsaVulkanRenderer *v,Image *i,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect)
{
    VkImageCreateInfo ci={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};VkMemoryRequirements req;
    VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent=(VkExtent3D){v->width,v->height,1};
    ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
    OK(V(CreateImage)(v->device,&ci,NULL,&i->image));V(GetImageMemoryRequirements)(v->device,i->image,&req);
    ai.allocationSize=req.size;if(!memory_type(v,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&ai.memoryTypeIndex))return 0;
    OK(V(AllocateMemory)(v->device,&ai,NULL,&i->memory));OK(V(BindImageMemory)(v->device,i->image,i->memory,0));
    vi.image=i->image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange=(VkImageSubresourceRange){aspect,0,1,0,1};
    OK(V(CreateImageView)(v->device,&vi,NULL,&i->view));return 1;
}
static int target_size(SsaVulkanRenderer *v,unsigned w,unsigned h)
{
    VkFramebufferCreateInfo fi={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};VkImageView views[3];
    if(v->framebuffer && w==v->width && h==v->height)return 1;
    if(!w || !h || w>v->properties.limits.maxFramebufferWidth || h>v->properties.limits.maxFramebufferHeight)return 0;
    if(v->framebuffer)V(DestroyFramebuffer)(v->device,v->framebuffer,NULL);v->framebuffer=VK_NULL_HANDLE;
    image_free(v,&v->color);image_free(v,&v->aux);image_free(v,&v->depth);v->width=w;v->height=h;
    if(!image_new(v,&v->color,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT) ||
       !image_new(v,&v->aux,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT) ||
       !image_new(v,&v->depth,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT))return 0;
    views[0]=v->color.view;views[1]=v->aux.view;views[2]=v->depth.view;
    fi.renderPass=v->pass;fi.attachmentCount=3;fi.pAttachments=views;fi.width=w;fi.height=h;fi.layers=1;
    OK(V(CreateFramebuffer)(v->device,&fi,NULL,&v->framebuffer));return 1;
}
static int pipeline_new(SsaVulkanRenderer *v)
{
    VkAttachmentDescription attachments[3]={0};VkAttachmentReference colors[2]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference depth={2,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sub={0};
    VkSubpassDependency deps[2]={0};VkRenderPassCreateInfo rp={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    VkDescriptorSetLayoutBinding bindings[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL}};
    VkDescriptorSetLayoutCreateInfo di={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize pool_size={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};VkDescriptorPoolCreateInfo dp={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorSetAllocateInfo da={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};VkPushConstantRange push={VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,12};
    VkPipelineLayoutCreateInfo pl={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};VkShaderModule shaders[2]={0};unsigned i;
    VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    VkVertexInputBindingDescription vb={0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[3]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12},{2,0,VK_FORMAT_R32_UINT,20}};
    VkPipelineVertexInputStateCreateInfo vi={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo vp={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo ds={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState blend[2]={0};VkPipelineColorBlendStateCreateInfo cb={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkDynamicState dynamic[2]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dy={VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo pi={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};VkResult result;
    for(i=0;i<3;++i){attachments[i].samples=VK_SAMPLE_COUNT_1_BIT;attachments[i].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[i].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[i].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[i].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;}
    attachments[0].format=VK_FORMAT_B8G8R8A8_UNORM;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachments[0].initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format=VK_FORMAT_R32G32B32A32_SFLOAT;attachments[2].format=VK_FORMAT_D32_SFLOAT;
    attachments[2].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=2;sub.pColorAttachments=colors;sub.pDepthStencilAttachment=&depth;
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;deps[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    rp.attachmentCount=3;rp.pAttachments=attachments;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=2;rp.pDependencies=deps;
    OK(V(CreateRenderPass)(v->device,&rp,NULL,&v->pass));
    di.bindingCount=2;di.pBindings=bindings;OK(V(CreateDescriptorSetLayout)(v->device,&di,NULL,&v->descriptors));
    dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&pool_size;OK(V(CreateDescriptorPool)(v->device,&dp,NULL,&v->descriptor_pool));
    da.descriptorPool=v->descriptor_pool;da.descriptorSetCount=1;da.pSetLayouts=&v->descriptors;OK(V(AllocateDescriptorSets)(v->device,&da,&v->set));
    pl.setLayoutCount=1;pl.pSetLayouts=&v->descriptors;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;OK(V(CreatePipelineLayout)(v->device,&pl,NULL,&v->layout));
    for(i=0;i<2;++i){VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=i?sizeof(ssa_scene_frag):sizeof(ssa_scene_vert);sm.pCode=i?ssa_scene_frag:ssa_scene_vert;
        result=V(CreateShaderModule)(v->device,&sm,NULL,&shaders[i]);if(result!=VK_SUCCESS){if(shaders[0])V(DestroyShaderModule)(v->device,shaders[0],NULL);return 0;}
        stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=shaders[i];stages[i].pName="main";}
    vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&vb;vi.vertexAttributeDescriptionCount=3;vi.pVertexAttributeDescriptions=va;
    ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;vp.viewportCount=vp.scissorCount=1;
    rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
    ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
    blend[0].blendEnable=VK_TRUE;blend[0].srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend[0].dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    /* RGB still uses shader coverage; store zero alpha for the CPU RGB buffer
     * directly, avoiding a full-frame mask after the download. */
    blend[0].srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;blend[0].dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
    blend[0].colorWriteMask=blend[1].colorWriteMask=15;
    cb.attachmentCount=2;cb.pAttachments=blend;dy.dynamicStateCount=2;dy.pDynamicStates=dynamic;
    pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&vp;
    pi.pRasterizationState=&rs;pi.pMultisampleState=&ms;pi.pDepthStencilState=&ds;pi.pColorBlendState=&cb;pi.pDynamicState=&dy;pi.layout=v->layout;pi.renderPass=v->pass;
    for(i=0;i<2;++i){
        /* Sprite triangles already obey the GBA wall silhouettes. Testing
         * world planes again would slice death/explosion art at the floor. */
        ds.depthTestEnable=ds.depthWriteEnable=i?VK_FALSE:VK_TRUE;
        result=V(CreateGraphicsPipelines)(v->device,VK_NULL_HANDLE,1,&pi,NULL,&v->pipeline[i]);
        if(result!=VK_SUCCESS)break;
    }
    for(i=0;i<2;++i)V(DestroyShaderModule)(v->device,shaders[i],NULL);return result==VK_SUCCESS;
}
static int grow(void **owner,size_t *capacity,size_t count,size_t stride)
{
    size_t n;void *p;if(count<=*capacity)return 1;n=*capacity?*capacity:256;
    while(n<count){if(n>SIZE_MAX/2)return 0;n*=2;}if(n>SIZE_MAX/stride)return 0;
    p=realloc(*owner,n*stride);if(!p)return 0;*owner=p;*capacity=n;return 1;
}
/* Lookup tables preserve triangle submission order. Texture pointers are only
 * keys: contents are compared on first use each pass, so borrowed stack art,
 * palette-independent mip changes and allocator address reuse remain safe. */
static unsigned lookup_mix(uintptr_t value)
{
    uint64_t x=(uint64_t)value;x^=x>>33;x*=UINT64_C(0xff51afd7ed558ccd);x^=x>>33;
    return (unsigned)x;
}
static unsigned material_bucket(const SsaPcMaterial *m)
{
    unsigned h=lookup_mix((uintptr_t)m->pixels)^m->width*31u^m->height*131u;
    h^=m->color*197u^m->transparent*257u^m->actor*521u^m->sky*1031u^m->decals*2053u;
    for(unsigned i=0;i<2;++i)h^=lookup_mix((uintptr_t)m->mip_pixels[i])^m->mip_width[i]*4099u^m->mip_height[i]*8191u;
    return h&(LOOKUP_BUCKETS-1);
}
static int material_equal(const SsaPcMaterial *k,const SsaPcMaterial *m)
{
    return k->pixels==m->pixels && k->width==m->width && k->height==m->height && k->color==m->color &&
        k->transparent==m->transparent && k->actor==m->actor && k->sky==m->sky && k->decals==m->decals &&
        !memcmp(k->mip_pixels,m->mip_pixels,sizeof m->mip_pixels) &&
        !memcmp(k->mip_width,m->mip_width,sizeof m->mip_width) && !memcmp(k->mip_height,m->mip_height,sizeof m->mip_height);
}
static void texture_cache_clear(SsaVulkanRenderer *v)
{
    for(size_t i=0;i<v->entry_count;++i)free(v->texture_entries[i].copy);
    v->entry_count=0;v->texture_count=256;memset(v->texture_buckets,0,sizeof v->texture_buckets);
}
static int texture_offset(SsaVulkanRenderer *v,const uint8_t *pixels,unsigned width,unsigned height,uint32_t *offset)
{
    size_t count=(size_t)width*height;TextureEntry *e;
    unsigned bucket=(lookup_mix((uintptr_t)pixels)^width*31u^height*131u)&(LOOKUP_BUCKETS-1),link=v->texture_buckets[bucket];
    if(!pixels || !count || count/width!=height)return 0;
    while(link){e=&v->texture_entries[link-1];
        if(e->address==(uintptr_t)pixels && e->width==width && e->height==height)break;
        link=e->next;}
    if(!link){uint8_t *copy;
        if(count>UINT32_MAX-v->texture_count || v->entry_count>=UINT_MAX ||
           !grow((void **)&v->cpu_textures,&v->texture_capacity,v->texture_count+count,sizeof(uint32_t)) ||
           !grow((void **)&v->texture_entries,&v->entry_capacity,v->entry_count+1,sizeof(TextureEntry)))return 0;
        copy=malloc(count);if(!copy)return 0;
        e=&v->texture_entries[v->entry_count++];
        *e=(TextureEntry){(uintptr_t)pixels,copy,count,width,height,(unsigned)v->texture_count,v->texture_buckets[bucket],0,1};
        v->texture_buckets[bucket]=(unsigned)v->entry_count;v->texture_count+=count;
    }else e=&v->texture_entries[link-1];
    if(e->seen!=v->texture_epoch){
        if(e->dirty || memcmp(e->copy,pixels,count)){
            memcpy(e->copy,pixels,count);
            for(size_t i=0;i<count;++i)v->cpu_textures[e->offset+i]=pixels[i];
            e->dirty=1;
        }
        e->seen=v->texture_epoch;
    }
    *offset=e->offset;return 1;
}
static int begin(void *context,SsaPcRenderer *r,const uint32_t palette[256],double focal,unsigned first,unsigned row_end)
{
    SsaVulkanRenderer *v=context;
    if(v->failed || v->recording || first>=row_end || row_end>r->height)return -1;
    if(v->post_target==r)v->post_target=NULL;
    if(v->motion_target==r && !(r->hardware_features&SSA_PC_MOTION))v->motion_target=NULL;
    v->first_row=first;v->end_row=row_end;
    if(!grow((void **)&v->cpu_textures,&v->texture_capacity,256,sizeof(uint32_t)))return -1;
    /* Bound retained content across map changes; queued passes have already
     * completed before begin, so clearing never invalidates in-flight offsets. */
    if(v->texture_count>TEXTURE_CACHE_TEXELS || v->entry_count>TEXTURE_CACHE_ENTRIES)texture_cache_clear(v);
    if(!v->texture_count)v->texture_count=256;
    if(!++v->texture_epoch){for(size_t i=0;i<v->entry_count;++i)v->texture_entries[i].seen=0;v->texture_epoch=1;}
    memcpy(v->cpu_textures,palette,256*sizeof(uint32_t));v->vertex_count=v->material_count=0;
    memset(v->material_buckets,0,sizeof v->material_buckets);
    v->scale[0]=(float)(2*focal/r->width);v->scale[1]=(float)(-2*focal/r->height);v->target=r;v->recording=1;return 1;
}
static int triangle(void *context,const SsaPcVertex points[3],const SsaPcMaterial *m)
{
    SsaVulkanRenderer *v=context;size_t i,index;unsigned bucket,link;
    if(!v->recording)return -1;
    bucket=material_bucket(m);link=v->material_buckets[bucket];
    while(link && !material_equal(&v->keys[link-1],m))link=v->material_next[link-1];
    index=link?link-1:v->material_count;
    if(index==v->material_count){Material mat={0};
        if(v->material_count>=UINT32_C(0x7fffffff))return -1;
        if(v->material_count==v->material_capacity){size_t cap=v->material_capacity?v->material_capacity*2:256;
            Material *materials=realloc(v->cpu_materials,cap*sizeof(*materials));SsaPcMaterial *keys;unsigned *next;
            if(!materials)return -1;v->cpu_materials=materials;keys=realloc(v->keys,cap*sizeof(*keys));if(!keys)return -1;v->keys=keys;
            next=realloc(v->material_next,cap*sizeof(*next));if(!next)return -1;v->material_next=next;v->material_capacity=cap;}
        mat.code=m->color|(m->decals<<8)|(m->sky<<10)|(m->actor<<11)|(m->pixels?4096u:0u)|(m->transparent<<13);
        if(m->pixels){mat.width=m->width;mat.height=m->height;
            if(!texture_offset(v,m->pixels,m->width,m->height,&mat.offset))return -1;}
        if(m->mip_pixels[0])for(unsigned level=0;level<2;++level){
            mat.mips[level][1]=m->mip_width[level];mat.mips[level][2]=m->mip_height[level];
            if(!texture_offset(v,m->mip_pixels[level],m->mip_width[level],m->mip_height[level],&mat.mips[level][0]))return -1;}
        v->cpu_materials[index]=mat;v->keys[index]=*m;
        v->material_next[index]=v->material_buckets[bucket];v->material_buckets[bucket]=(unsigned)++v->material_count;
    }
    if(v->vertex_count>UINT32_MAX-3 || !grow((void **)&v->cpu_vertices,&v->vertex_capacity,v->vertex_count+3,sizeof(Vertex)))return -1;
    if(index>UINT32_C(0x7fffffff))return -1;
    for(i=0;i<3;++i){const SsaPcVertex *p=&points[i];v->cpu_vertices[v->vertex_count++]=(Vertex){(float)p->x,(float)p->y,(float)p->z,(float)p->u,(float)p->v,
        (uint32_t)index|(m->actor?UINT32_C(0x80000000):0)};}
    return 1;
}
static int submit(SsaVulkanRenderer *v,SsaPcRenderer *r,const SsaPcSample **samples)
{
    double start=wall_time(),submitted,completed;
    int new_textures=v->textures.size<v->texture_count*4;
    int retain_surface=(r->hardware_features&(SSA_PC_AO|SSA_PC_MOTION)) && v->first_row==0 && v->end_row==r->height;
    size_t n=(size_t)r->width*r->height;VkDeviceSize colors=n*4,aux_offset=(colors+15)&~(VkDeviceSize)15,bytes=aux_offset+n*sizeof(SsaPcSample),zero=0;
    size_t first_pixel=(size_t)v->first_row*r->width,row_pixels=(size_t)(v->end_row-v->first_row)*r->width;
    VkDescriptorBufferInfo buffers[2];VkWriteDescriptorSet writes[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    VkBufferImageCopy copy={0};VkRenderPassBeginInfo pass={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};VkClearValue clear[3]={0};
    VkViewport viewport={0,0,(float)r->width,(float)r->height,0,1};
    VkRect2D scissor={{0,(int32_t)v->first_row},{r->width,v->end_row-v->first_row}};
    VkMemoryBarrier host={VK_STRUCTURE_TYPE_MEMORY_BARRIER};VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO};unsigned i;
    if(!target_size(v,r->width,r->height) || v->texture_count*4>v->properties.limits.maxStorageBufferRange ||
       v->material_count*sizeof(Material)>v->properties.limits.maxStorageBufferRange)return 0;
    if(!buffer_size(v,&v->vertices,(v->vertex_count?v->vertex_count:1)*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,BUFFER_UPLOAD) ||
       !buffer_size(v,&v->textures,v->texture_count*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,BUFFER_UPLOAD) ||
       !buffer_size(v,&v->materials,(v->material_count?v->material_count:1)*sizeof(Material),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,BUFFER_UPLOAD) ||
       !buffer_size(v,&v->upload,colors,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,BUFFER_UPLOAD) ||
       !buffer_size(v,&v->readback,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,BUFFER_READBACK))return 0;
    /* Preserve the main-view chart on the device before reflected views reuse
     * the attachments. Postprocessing only needs to upload composed RGB. */
    if(retain_surface && (bytes>v->properties.limits.maxStorageBufferRange ||
       !buffer_size(v,&v->post_input,bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,BUFFER_DEVICE)))return 0;
    if(v->vertex_count)memcpy(v->vertices.mapped,v->cpu_vertices,v->vertex_count*sizeof(Vertex));
    if(new_textures)memcpy(v->textures.mapped,v->cpu_textures,v->texture_count*4);
    else memcpy(v->textures.mapped,v->cpu_textures,256*sizeof(uint32_t));
    for(size_t t=0;t<v->entry_count;++t){TextureEntry *e=&v->texture_entries[t];
        if(e->dirty && !new_textures)memcpy((uint32_t *)v->textures.mapped+e->offset,v->cpu_textures+e->offset,e->count*4);
        e->dirty=0;
    }
    if(v->material_count)memcpy(v->materials.mapped,v->cpu_materials,v->material_count*sizeof(Material));
    memcpy((uint8_t *)v->upload.mapped+first_pixel*4,r->pixels+first_pixel,row_pixels*4);
    buffers[0]=(VkDescriptorBufferInfo){v->textures.buffer,0,v->texture_count*4};
    buffers[1]=(VkDescriptorBufferInfo){v->materials.buffer,0,(v->material_count?v->material_count:1)*sizeof(Material)};
    for(i=0;i<2;++i){writes[i].dstSet=v->set;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&buffers[i];}
    V(UpdateDescriptorSets)(v->device,2,writes,0,NULL);OK(V(ResetCommandPool)(v->device,v->pool,0));
    bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;OK(V(BeginCommandBuffer)(v->commands,&bi));
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=v->color.image;
    barrier.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.bufferOffset=first_pixel*4;copy.imageOffset.y=(int32_t)v->first_row;
    copy.imageExtent=(VkExtent3D){r->width,v->end_row-v->first_row,1};
    V(CmdCopyBufferToImage)(v->commands,v->upload.buffer,v->color.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,NULL,0,NULL,1,&barrier);
    clear[1].color.float32[2]=FLT_MAX;clear[2].depthStencil.depth=0;
    pass.renderPass=v->pass;pass.framebuffer=v->framebuffer;pass.renderArea=scissor;pass.clearValueCount=3;pass.pClearValues=clear;
    V(CmdBeginRenderPass)(v->commands,&pass,VK_SUBPASS_CONTENTS_INLINE);
    V(CmdSetViewport)(v->commands,0,1,&viewport);V(CmdSetScissor)(v->commands,0,1,&scissor);
    V(CmdBindDescriptorSets)(v->commands,VK_PIPELINE_BIND_POINT_GRAPHICS,v->layout,0,1,&v->set,0,NULL);
    V(CmdBindVertexBuffers)(v->commands,0,1,&v->vertices.buffer,&zero);{
        struct {float scale[2];uint32_t anisotropic;} view={{v->scale[0],v->scale[1]},(r->hardware_features&SSA_PC_AF)!=0};
        V(CmdPushConstants)(v->commands,v->layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof view,&view);}

    /* Complete world depth first, then preserve the caller's sorted actor
     * order. Normal scene submissions are just one draw per pass. */
    for(i=0;i<2;++i){size_t first=0;
        V(CmdBindPipeline)(v->commands,VK_PIPELINE_BIND_POINT_GRAPHICS,v->pipeline[i]);
        while(first<v->vertex_count){size_t last=first+3;unsigned actor=v->cpu_vertices[first].material>>31;
            while(last<v->vertex_count && (v->cpu_vertices[last].material>>31)==actor)last+=3;
            if(actor==i)V(CmdDraw)(v->commands,(uint32_t)(last-first),1,(uint32_t)first,0);
            first=last;
        }
    }
    V(CmdEndRenderPass)(v->commands);
    V(CmdCopyImageToBuffer)(v->commands,v->color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,v->readback.buffer,1,&copy);
    copy.bufferOffset=aux_offset+first_pixel*sizeof(SsaPcSample);
    V(CmdCopyImageToBuffer)(v->commands,v->aux.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,v->readback.buffer,1,&copy);
    if(retain_surface)V(CmdCopyImageToBuffer)(v->commands,v->aux.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,v->post_input.buffer,1,&copy);
    host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,NULL,0,NULL);
    OK(V(EndCommandBuffer)(v->commands));OK(V(ResetFences)(v->device,1,&v->fence));si.commandBufferCount=1;si.pCommandBuffers=&v->commands;
    submitted=wall_time();OK(V(QueueSubmit)(v->queue,1,&si,v->fence));OK(V(WaitForFences)(v->device,1,&v->fence,VK_TRUE,UINT64_MAX));completed=wall_time();
    if(!(v->readback.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){
        VkMappedMemoryRange range={VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory=v->readback.memory;range.size=VK_WHOLE_SIZE;
        OK(V(InvalidateMappedMemoryRanges)(v->device,1,&range));
    }
    memcpy(r->pixels+first_pixel,(const uint8_t *)v->readback.mapped+first_pixel*4,row_pixels*4);
    *samples=(const SsaPcSample *)((const uint8_t *)v->readback.mapped+aux_offset);
    /* Unusual devices without cached host memory still get a bulk copy into
     * ordinary RAM before the scene performs per-component sample reads. */
    if(!(v->readback.flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)){
        if(!grow((void **)&v->cpu_samples,&v->sample_capacity,n,sizeof(SsaPcSample)))return 0;
        memcpy(v->cpu_samples+first_pixel,*samples+first_pixel,row_pixels*sizeof(SsaPcSample));*samples=v->cpu_samples;
    }
    if(retain_surface){v->post_target=r;v->post_width=r->width;v->post_height=r->height;}
    v->timing.prepare+=submitted-start;v->timing.wait+=completed-submitted;v->timing.readback+=wall_time()-completed;++v->timing.passes;
    return 1;
}
static int end(void *context,SsaPcRenderer *r,const SsaPcSample **samples)
{
    SsaVulkanRenderer *v=context;int result;
    if(!v->recording || v->target!=r)return -1;
    result=submit(v,r,samples);v->recording=0;v->target=NULL;
    if(!result){v->failed=1;return -1;}return 1;
}
typedef struct RowJob {SsaPcRows run;void *context;volatile LONG next;unsigned end;} RowJob;
static void row_job(RowJob *job)
{
    for(;;){unsigned first=(unsigned)InterlockedExchangeAdd(&job->next,16),end;
        if(first>=job->end)break;end=first+16;if(end>job->end)end=job->end;
        job->run(job->context,first,end);}
}
static void CALLBACK row_worker(PTP_CALLBACK_INSTANCE instance,void *context,PTP_WORK work)
{(void)instance;(void)work;row_job(context);}
static void rows(void *context,unsigned first,unsigned end,SsaPcRows run,void *data)
{
    SsaVulkanRenderer *v=context;RowJob job={run,data,(LONG)first,end};PTP_WORK work;double start=wall_time();
    if(!v->workers || end-first<64){run(data,first,end);v->timing.effects+=wall_time()-start;return;}
    work=CreateThreadpoolWork(row_worker,&job,&v->worker_environment);
    if(!work){run(data,first,end);v->timing.effects+=wall_time()-start;return;}
    /* Bounded workers plus the render thread. Jobs never call Vulkan,
     * gameplay or audio and cannot survive the current synchronous frame. */
    for(unsigned i=0;i<v->worker_count;++i)SubmitThreadpoolWork(work);
    row_job(&job);WaitForThreadpoolWorkCallbacks(work,FALSE);CloseThreadpoolWork(work);
    v->timing.effects+=wall_time()-start;
}
#include "vulkan_post.inc"
static int initialize(SsaVulkanRenderer *v)
{
    PFN_vkCreateInstance create;VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO};VkInstanceCreateInfo ci={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkDebugUtilsMessengerCreateInfoEXT debug={VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    const char *layer="VK_LAYER_KHRONOS_validation",*extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkPhysicalDevice *devices;uint32_t count=0,i,best=0;float priority=1;VkPhysicalDeviceFeatures features={0};
    VkDeviceQueueCreateInfo qi={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};VkDeviceCreateInfo di={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkCommandPoolCreateInfo cp={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};VkCommandBufferAllocateInfo ca={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};VkFenceCreateInfo fi={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    int validation=GetEnvironmentVariableA("SSA_VULKAN_VALIDATION",NULL,0)!=0;
    v->loader=LoadLibraryW(L"vulkan-1.dll");if(!v->loader)return 0;
    v->get=(PFN_vkGetInstanceProcAddr)(void *)GetProcAddress(v->loader,"vkGetInstanceProcAddr");if(!v->get)return 0;
    create=(PFN_vkCreateInstance)v->get(NULL,"vkCreateInstance");if(!create)return 0;
    app.pApplicationName="Serious Sam Advance PC";app.apiVersion=VK_API_VERSION_1_0;ci.pApplicationInfo=&app;
    if(validation){ci.enabledLayerCount=ci.enabledExtensionCount=1;ci.ppEnabledLayerNames=&layer;ci.ppEnabledExtensionNames=&extension;
        debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback=debug_message;debug.pUserData=v;ci.pNext=&debug;}
    OK(create(&ci,NULL,&v->instance));
#define LOAD_INSTANCE(name) v->name=(PFN_vk##name)v->get(v->instance,"vk" #name);if(!v->name)return 0;
    INSTANCE_FUNCTIONS(LOAD_INSTANCE)
#undef LOAD_INSTANCE
    if(validation){PFN_vkCreateDebugUtilsMessengerEXT fn=(PFN_vkCreateDebugUtilsMessengerEXT)v->get(v->instance,"vkCreateDebugUtilsMessengerEXT");
        v->destroy_debug=(PFN_vkDestroyDebugUtilsMessengerEXT)v->get(v->instance,"vkDestroyDebugUtilsMessengerEXT");
        if(!fn || !v->destroy_debug)return 0;OK(fn(v->instance,&debug,NULL,&v->debug));}
    OK(V(EnumeratePhysicalDevices)(v->instance,&count,NULL));if(!count)return 0;devices=calloc(count,sizeof(*devices));if(!devices)return 0;
    if(V(EnumeratePhysicalDevices)(v->instance,&count,devices)!=VK_SUCCESS){free(devices);return 0;}
    for(i=0;i<count;++i){VkPhysicalDeviceProperties p;VkPhysicalDeviceFeatures f;VkQueueFamilyProperties *queues;uint32_t n=0,j;unsigned score;
        V(GetPhysicalDeviceProperties)(devices[i],&p);V(GetPhysicalDeviceFeatures)(devices[i],&f);
        if(!f.independentBlend || p.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
        V(GetPhysicalDeviceQueueFamilyProperties)(devices[i],&n,NULL);queues=calloc(n,sizeof(*queues));if(!queues)continue;
        V(GetPhysicalDeviceQueueFamilyProperties)(devices[i],&n,queues);
        for(j=0;j<n;++j)if(queues[j].queueCount && ((queues[j].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)))break;free(queues);if(j==n)continue;
        score=p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?3:p.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?2:1;
        if(score>best){best=score;v->physical=devices[i];v->family=j;v->properties=p;}}
    free(devices);if(!v->physical)return 0;
    {const VkFormat formats[]={VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_D32_SFLOAT};
        const VkFormatFeatureFlags flags[]={VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT};
        for(i=0;i<3;++i){VkFormatProperties p;V(GetPhysicalDeviceFormatProperties)(v->physical,formats[i],&p);if((p.optimalTilingFeatures&flags[i])!=flags[i])return 0;}}
    V(GetPhysicalDeviceMemoryProperties)(v->physical,&v->memory);features.independentBlend=VK_TRUE;
    qi.queueFamilyIndex=v->family;qi.queueCount=1;qi.pQueuePriorities=&priority;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.pEnabledFeatures=&features;
    OK(V(CreateDevice)(v->physical,&di,NULL,&v->device));
#define LOAD_DEVICE(name) v->name=(PFN_vk##name)v->GetDeviceProcAddr(v->device,"vk" #name);if(!v->name)return 0;
    DEVICE_FUNCTIONS(LOAD_DEVICE)
#undef LOAD_DEVICE
    V(GetDeviceQueue)(v->device,v->family,0,&v->queue);if(!pipeline_new(v))return 0;
    cp.queueFamilyIndex=v->family;OK(V(CreateCommandPool)(v->device,&cp,NULL,&v->pool));
    ca.commandPool=v->pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;OK(V(AllocateCommandBuffers)(v->device,&ca,&v->commands));
    OK(V(CreateFence)(v->device,&fi,NULL,&v->fence));
    {SYSTEM_INFO info;GetSystemInfo(&info);
        unsigned threads=info.dwNumberOfProcessors/2;if(threads>8)threads=8;
        v->worker_count=threads>1?threads-1:0;
        if(v->worker_count)v->workers=CreateThreadpool(NULL);}
    if(v->workers){InitializeThreadpoolEnvironment(&v->worker_environment);
        SetThreadpoolThreadMaximum(v->workers,v->worker_count);SetThreadpoolCallbackPool(&v->worker_environment,v->workers);}
    v->backend=(SsaPcHardware){v,begin,triangle,end,postprocess,rows};return 1;
}
SsaVulkanRenderer *ssa_vulkan_open(void)
{
    SsaVulkanRenderer *v=calloc(1,sizeof(*v));if(!v)return NULL;
    if(!initialize(v)){ssa_vulkan_close(v);return NULL;}
    fprintf(stderr,"Vulkan hardware renderer: %s\n",v->properties.deviceName);return v;
}
void ssa_vulkan_close(SsaVulkanRenderer *v)
{
    if(!v)return;
    if(v->workers){DestroyThreadpoolEnvironment(&v->worker_environment);CloseThreadpool(v->workers);}
    if(v->device && V(DeviceWaitIdle))V(DeviceWaitIdle)(v->device);
    if(v->fence)V(DestroyFence)(v->device,v->fence,NULL);
    if(v->pool)V(DestroyCommandPool)(v->device,v->pool,NULL);
    if(v->framebuffer)V(DestroyFramebuffer)(v->device,v->framebuffer,NULL);
    image_free(v,&v->color);image_free(v,&v->aux);image_free(v,&v->depth);
    buffer_free(v,&v->vertices);buffer_free(v,&v->textures);buffer_free(v,&v->materials);buffer_free(v,&v->upload);buffer_free(v,&v->readback);
    for(unsigned i=0;i<2;++i)if(v->pipeline[i])V(DestroyPipeline)(v->device,v->pipeline[i],NULL);
    buffer_free(v,&v->post_input);buffer_free(v,&v->post_work);
    if(v->post_pipeline)V(DestroyPipeline)(v->device,v->post_pipeline,NULL);
    if(v->post_layout)V(DestroyPipelineLayout)(v->device,v->post_layout,NULL);
    if(v->post_pool)V(DestroyDescriptorPool)(v->device,v->post_pool,NULL);
    if(v->post_descriptors)V(DestroyDescriptorSetLayout)(v->device,v->post_descriptors,NULL);
    if(v->layout)V(DestroyPipelineLayout)(v->device,v->layout,NULL);
    if(v->descriptor_pool)V(DestroyDescriptorPool)(v->device,v->descriptor_pool,NULL);
    if(v->descriptors)V(DestroyDescriptorSetLayout)(v->device,v->descriptors,NULL);
    if(v->pass)V(DestroyRenderPass)(v->device,v->pass,NULL);
    if(v->device && V(DestroyDevice))V(DestroyDevice)(v->device,NULL);
    if(v->debug && v->destroy_debug)v->destroy_debug(v->instance,v->debug,NULL);
    if(v->instance && V(DestroyInstance))V(DestroyInstance)(v->instance,NULL);
    if(v->loader)FreeLibrary(v->loader);
    texture_cache_clear(v);free(v->texture_entries);free(v->material_next);
    free(v->cpu_vertices);free(v->cpu_materials);free(v->keys);free(v->cpu_textures);free(v->cpu_samples);free(v);
}
const SsaPcHardware *ssa_vulkan_backend(SsaVulkanRenderer *v){return v?&v->backend:NULL;}
const char *ssa_vulkan_device(const SsaVulkanRenderer *v){return v?v->properties.deviceName:"unavailable";}
unsigned ssa_vulkan_validation_errors(const SsaVulkanRenderer *v){return v?v->errors:0;}
SsaVulkanTiming ssa_vulkan_timing(const SsaVulkanRenderer *v){return v?v->timing:(SsaVulkanTiming){0};}
#else
SsaVulkanRenderer *ssa_vulkan_open(void){return NULL;}
void ssa_vulkan_close(SsaVulkanRenderer *v){(void)v;}
const SsaPcHardware *ssa_vulkan_backend(SsaVulkanRenderer *v){(void)v;return NULL;}
const char *ssa_vulkan_device(const SsaVulkanRenderer *v){(void)v;return "not built";}
unsigned ssa_vulkan_validation_errors(const SsaVulkanRenderer *v){(void)v;return 0;}
SsaVulkanTiming ssa_vulkan_timing(const SsaVulkanRenderer *v){(void)v;return (SsaVulkanTiming){0};}
#endif
