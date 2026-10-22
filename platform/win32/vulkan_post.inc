/* GPU scene-only FXAA/SSAO/camera motion blur. Created lazily; all-Off pays no allocation,
 * transfer or dispatch cost. Shares the already-idle command pool/fence. */
typedef struct PostPush {
    uint32_t width,height,flags,pass;
    float focal,previous_focal,exposure,padding;
    float previous[3][4];
} PostPush;
static int post_pipeline_new(SsaVulkanRenderer *v)
{
    VkDescriptorSetLayoutBinding bindings[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL}};
    VkDescriptorSetLayoutCreateInfo di={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize size={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};VkDescriptorPoolCreateInfo pool={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorSetAllocateInfo alloc={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkPushConstantRange push={VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(PostPush)};VkPipelineLayoutCreateInfo layout={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo shader={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};VkShaderModule module;
    VkComputePipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};VkResult result;
    di.bindingCount=2;di.pBindings=bindings;OK(V(CreateDescriptorSetLayout)(v->device,&di,NULL,&v->post_descriptors));
    pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;OK(V(CreateDescriptorPool)(v->device,&pool,NULL,&v->post_pool));
    alloc.descriptorPool=v->post_pool;alloc.descriptorSetCount=1;alloc.pSetLayouts=&v->post_descriptors;
    OK(V(AllocateDescriptorSets)(v->device,&alloc,&v->post_set));
    layout.setLayoutCount=1;layout.pSetLayouts=&v->post_descriptors;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    OK(V(CreatePipelineLayout)(v->device,&layout,NULL,&v->post_layout));
    shader.codeSize=sizeof ssa_post_comp;shader.pCode=ssa_post_comp;OK(V(CreateShaderModule)(v->device,&shader,NULL,&module));
    pipeline.layout=v->post_layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=module;pipeline.stage.pName="main";
    result=V(CreateComputePipelines)(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&v->post_pipeline);
    V(DestroyShaderModule)(v->device,module,NULL);return result==VK_SUCCESS;
}
/* Reproject the current camera into the previous view. Only velocity is
 * retained: colors always come from this frame, so menus cannot ghost. */
static void motion_push(SsaVulkanRenderer *v,const SsaPcRenderer *r,PostPush *push,double now)
{
    const SsaPcMotionView *old=&v->motion_previous,*cur=&r->motion;double dt=now-v->motion_time;
    float distance=0,alignment=0;
    if(v->motion_target!=r || !cur->valid || cur->scene!=old->scene || cur->cut!=old->cut || cur->epoch!=old->epoch ||
       cur->frame<old->frame || r->width!=v->motion_width || r->height!=v->motion_height ||
       push->focal!=v->motion_focal || dt<=0 || dt>0.2)return;
    for(unsigned i=0;i<3;++i){float d=cur->origin[i]-old->origin[i];distance+=d*d;alignment+=cur->axes[6+i]*old->axes[6+i];}
    if(distance>96*96 || alignment<0.707f)return;
    /* A fixed camera has no velocity, even if the ROM's quantized basis is
     * not perfectly orthonormal. Avoid a whole-frame identity dispatch. */
    if(!memcmp(cur->origin,old->origin,sizeof cur->origin) &&
       !memcmp(cur->axes,old->axes,sizeof cur->axes))return;
    push->previous_focal=v->motion_focal;push->exposure=(float)((1.0/120.0)/dt);
    if(push->exposure>1)push->exposure=1;
    for(unsigned row=0;row<3;++row){
        for(unsigned col=0;col<3;++col)for(unsigned k=0;k<3;++k)
            push->previous[row][col]+=old->axes[row*3+k]*cur->axes[col*3+k];
        for(unsigned k=0;k<3;++k)push->previous[row][3]+=old->axes[row*3+k]*(cur->origin[k]-old->origin[k]);
    }
}
static void post_complete(SsaVulkanRenderer *v,SsaPcRenderer *r,float focal,double timestamp)
{
    /* Advance history even when blur was skipped, so the next moving frame
     * measures velocity from this frame instead of an old camera sample. */
    v->post_target=NULL;
    v->motion_target=(r->hardware_features&SSA_PC_MOTION) && r->motion.valid?r:NULL;
    v->motion_previous=r->motion;v->motion_time=timestamp;v->motion_focal=focal;
    v->motion_width=r->width;v->motion_height=r->height;
}
static int post_submit(SsaVulkanRenderer *v,SsaPcRenderer *r,double focal)
{
    size_t n=(size_t)r->width*r->height;VkDeviceSize bytes=n*4;
    const unsigned ao=(r->hardware_features&SSA_PC_AO)!=0,aa=(r->hardware_features&SSA_PC_AA)!=0;
    double timestamp=wall_time();
    PostPush push={r->width,r->height,r->hardware_features,0,(float)focal,0,0,0,{{0}}};
    if(v->recording || !n || focal<=0)return 0;
    if(r->hardware_features&SSA_PC_MOTION)motion_push(v,r,&push,timestamp);
    const unsigned motion=push.exposure>0,needs_samples=ao || motion,passes=ao+aa+motion;
    if(!passes){post_complete(v,r,(float)focal,timestamp);return 1;}
    /* At most two image regions: each pass reads the previous result and
     * writes the other region. AO -> AA -> blur can reuse the AO region. */
    VkDeviceSize samples_offset=(bytes+15)&~(VkDeviceSize)15;
    VkDeviceSize input_bytes=needs_samples?samples_offset+n*sizeof(SsaPcSample):bytes;
    VkDeviceSize work_bytes=bytes*(passes>1?2:1);
    /* Host samples are an explicit standalone/test override. Normal scene
     * samples were copied directly from the main-view attachment on the GPU. */
    const unsigned host_samples=needs_samples && r->effect_samples!=NULL;
    VkDeviceSize upload_bytes=host_samples?input_bytes:bytes;
    VkDescriptorBufferInfo buffers[2];VkWriteDescriptorSet writes[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    VkCommandBufferBeginInfo begin_info={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};VkMemoryBarrier barrier={VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    VkBufferCopy copy={0,0,upload_bytes};VkSubmitInfo submit_info={VK_STRUCTURE_TYPE_SUBMIT_INFO};
    unsigned gx=(r->width+7)/8,gy=(r->height+7)/8;
    if(input_bytes>v->properties.limits.maxStorageBufferRange || work_bytes>v->properties.limits.maxStorageBufferRange ||
       gx>v->properties.limits.maxComputeWorkGroupCount[0] || gy>v->properties.limits.maxComputeWorkGroupCount[1] ||
       (host_samples && r->effect_capacity<n) ||
       (needs_samples && !host_samples && (v->post_target!=r || v->post_width!=r->width || v->post_height!=r->height)))return 0;
    if(!v->post_pipeline && !post_pipeline_new(v))return 0;
    if(!buffer_size(v,&v->upload,upload_bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,BUFFER_UPLOAD) ||
       !buffer_size(v,&v->readback,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,BUFFER_READBACK) ||
       !buffer_size(v,&v->post_input,input_bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,BUFFER_DEVICE) ||
       !buffer_size(v,&v->post_work,work_bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,BUFFER_DEVICE))return 0;
    memcpy(v->upload.mapped,r->pixels,(size_t)bytes);
    if(host_samples)memcpy((uint8_t *)v->upload.mapped+samples_offset,r->effect_samples,n*sizeof(*r->effect_samples));
    buffers[0]=(VkDescriptorBufferInfo){v->post_input.buffer,0,input_bytes};buffers[1]=(VkDescriptorBufferInfo){v->post_work.buffer,0,work_bytes};
    for(unsigned i=0;i<2;++i){writes[i].dstSet=v->post_set;writes[i].dstBinding=i;writes[i].descriptorCount=1;
        writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&buffers[i];}
    V(UpdateDescriptorSets)(v->device,2,writes,0,NULL);OK(V(ResetCommandPool)(v->device,v->pool,0));
    begin_info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;OK(V(BeginCommandBuffer)(v->commands,&begin_info));
    V(CmdCopyBuffer)(v->commands,v->upload.buffer,v->post_input.buffer,1,&copy);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
    V(CmdBindPipeline)(v->commands,VK_PIPELINE_BIND_POINT_COMPUTE,v->post_pipeline);
    V(CmdBindDescriptorSets)(v->commands,VK_PIPELINE_BIND_POINT_COMPUTE,v->post_layout,0,1,&v->post_set,0,NULL);
    const unsigned enabled[3]={ao,aa,motion};unsigned dispatched=0;
    for(unsigned pass=0;pass<3;++pass)if(enabled[pass]){
        if(dispatched){
            /* Include reads and writes: the third pass overwrites the first
             * region after AA has finished reading it (WAR as well as RAW). */
            barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
            V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
        }
        push.pass=pass;V(CmdPushConstants)(v->commands,v->post_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof push,&push);
        V(CmdDispatch)(v->commands,gx,gy,1);++dispatched;
    }
    barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,NULL,0,NULL);
    copy=(VkBufferCopy){bytes*((passes-1)&1),0,bytes};
    V(CmdCopyBuffer)(v->commands,v->post_work.buffer,v->readback.buffer,1,&copy);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    V(CmdPipelineBarrier)(v->commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,NULL,0,NULL);
    OK(V(EndCommandBuffer)(v->commands));OK(V(ResetFences)(v->device,1,&v->fence));
    submit_info.commandBufferCount=1;submit_info.pCommandBuffers=&v->commands;OK(V(QueueSubmit)(v->queue,1,&submit_info,v->fence));
    OK(V(WaitForFences)(v->device,1,&v->fence,VK_TRUE,UINT64_MAX));
    if(!(v->readback.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){VkMappedMemoryRange range={VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory=v->readback.memory;range.size=VK_WHOLE_SIZE;OK(V(InvalidateMappedMemoryRanges)(v->device,1,&range));}
    memcpy(r->pixels,v->readback.mapped,(size_t)bytes);
    v->timing.post_passes+=dispatched;
    post_complete(v,r,(float)focal,timestamp);
    return 1;
}
static int postprocess(void *context,SsaPcRenderer *r,double focal)
{
    SsaVulkanRenderer *v=context;double start=wall_time();int result;
    if(!(r->hardware_features&(SSA_PC_AA|SSA_PC_AO|SSA_PC_MOTION)))return 1;
    result=post_submit(v,r,focal);v->timing.effects+=wall_time()-start;
    if(!result){v->failed=1;return -1;}return 1;
}
