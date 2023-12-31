#include "stb_image.h"
#include "platform.h"
#include "vk_context.h"
#include "renderer.h"

namespace nslib
{

#if defined(NDEBUG)
intern const u32 VALIDATION_LAYER_COUNT = 0;
intern const char **VALIDATION_LAYERS = nullptr;
#else
intern const u32 VALIDATION_LAYER_COUNT = 1;
intern const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
#endif

#if __APPLE__
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 2;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 2;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
#else
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = {};
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 1;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif

intern int setup_render_pass(renderer *rndr)
{
    auto vk = rndr->vk;
    rndr->render_pass_ind = vkr_add_render_pass(&vk->inst.device, {});
    vkr_rpass_cfg rp_cfg{};
    VkAttachmentDescription col_att{};
    col_att.format = vk->inst.device.swapchain.format;
    col_att.samples = VK_SAMPLE_COUNT_1_BIT;
    col_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    col_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    col_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    col_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    col_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    col_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    arr_push_back(&rp_cfg.attachments, col_att);

    vkr_rpass_cfg_subpass subpass{};
    VkAttachmentReference att_ref{};
    att_ref.attachment = 0;
    att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    arr_push_back(&subpass.color_attachments, att_ref);
    arr_push_back(&rp_cfg.subpasses, subpass);

    VkSubpassDependency sp_dep{};
    sp_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    sp_dep.dstSubpass = 0;
    sp_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    sp_dep.srcAccessMask = 0;
    sp_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    sp_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    arr_push_back(&rp_cfg.subpass_dependencies, sp_dep);

    return vkr_init_render_pass(vk, &rp_cfg, &vk->inst.device.render_passes[rndr->render_pass_ind]);
}

intern int setup_pipeline(renderer *rndr)
{
    auto vk = rndr->vk;
    vkr_pipeline_cfg info{};
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Descripitor Set Layouts - Just one layout for the moment with a binding at 0 for uniforms and a binding at 1 for
    // image sampler
    ++info.set_layouts.size;
    info.set_layouts[0].bindings.size = 2;

    // Uniform buffer
    info.set_layouts[0].bindings[0].binding = 0;
    info.set_layouts[0].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[0].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    info.set_layouts[0].bindings[0].descriptorCount = 1;

    // Image sampler
    info.set_layouts[0].bindings[1].binding = 1;
    info.set_layouts[0].bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    info.set_layouts[0].bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    info.set_layouts[0].bindings[1].descriptorCount = 1;

    // Vertex binding:
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(vertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    arr_push_back(&info.vert_binding_desc, binding_desc);

    // Attribute Descriptions - so far we just have three
    VkVertexInputAttributeDescription attrib_desc{};
    attrib_desc.binding = 0;
    attrib_desc.location = 0;
    attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, pos);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);

    attrib_desc.binding = 0;
    attrib_desc.location = 1;
    attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, color);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);
    
    attrib_desc.binding = 0;
    attrib_desc.location = 2;
    attrib_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, tex_coord);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);
    

    // Viewports and scissors
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk->inst.device.swapchain.extent.width;
    viewport.height = (float)vk->inst.device.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    arr_push_back(&info.viewports, viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vk->inst.device.swapchain.extent;
    arr_push_back(&info.scissors, scissor);

    // Input Assembly
    info.input_assembly.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.input_assembly.primitive_restart_enable = false;

    // Raster options
    info.raster.depth_clamp_enable = false;
    info.raster.rasterizer_discard_enable = false;
    info.raster.polygon_mode = VK_POLYGON_MODE_FILL;
    info.raster.line_width = 1.0f;
    info.raster.cull_mode = VK_CULL_MODE_BACK_BIT;
    info.raster.front_face = VK_FRONT_FACE_CLOCKWISE;
    info.raster.depth_bias_enable = false;
    info.raster.depth_bias_constant_factor = 0.0f;
    info.raster.depth_bias_clamp = 0.0f;
    info.raster.depth_bias_slope_factor = 0.0f;

    // Multisampling defaults are good

    // Color blending
    VkPipelineColorBlendAttachmentState col_blnd_att{};
    col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    col_blnd_att.blendEnable = VK_FALSE;
    col_blnd_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    col_blnd_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    col_blnd_att.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
    col_blnd_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    col_blnd_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    col_blnd_att.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional
    arr_push_back(&info.col_blend.attachments, col_blnd_att);

    // Our basic shaders
    const char *fnames[] = {"data/shaders/rdev.vert.spv", "data/shaders/rdev.frag.spv"};
    for (int i = 0; i <= VKR_SHADER_STAGE_FRAG; ++i) {
        platform_file_err_desc err{};
        platform_read_file(fnames[i], &info.shader_stages[i].code, 0, &err);
        if (err.code != err_code::PLATFORM_NO_ERROR) {
            wlog("Error reading file %s from disk (code %d): %s", fnames[i], err.code, err.str);
            return err_code::RENDER_LOAD_SHADERS_FAIL;
        }
        info.shader_stages[i].entry_point = "main";
    }

    info.rpass = &vk->inst.device.render_passes[rndr->render_pass_ind];
    rndr->pipeline_ind = vkr_add_pipeline(&vk->inst.device, {});
    return vkr_init_pipeline(vk, &info, &vk->inst.device.pipelines[rndr->pipeline_ind]);
}

intern int create_vertex_indice_buffers(renderer *rndr)
{
    auto vk = rndr->vk;
    auto dev = &vk->inst.device;
    
    // Create vertex buffer on GPU
    vkr_buffer_cfg b_cfg{};
    rndr->vert_buf_ind = vkr_add_buffer(dev, {});
    rndr->ind_buf_ind = vkr_add_buffer(dev, {});

    // Common to all buffer options
    b_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    b_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    b_cfg.vma_alloc = &dev->vma_alloc;

    // Vert buffer
    b_cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = sizeof(vertex) * 4;
    int err = vkr_init_buffer(&dev->buffers[rndr->vert_buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Init and copy data to staging buffer, then copy staging buf to vert buffer, then delete staging buf
    vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->vert_buf_ind], verts, b_cfg.buffer_size, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);

    // Ind buffer
    b_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = sizeof(u32) * 6;
    err = vkr_init_buffer(&dev->buffers[rndr->ind_buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Init and copy data to staging buffer, then copy staging buf to vert buffer, then delete staging buf
    vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->ind_buf_ind], indices, b_cfg.buffer_size, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);
    return err_code::VKR_NO_ERROR;
}

intern int load_default_image_and_sampler(renderer *rndr)
{
    auto vk = rndr->vk;
    auto dev = &rndr->vk->inst.device;
    
    /////////////////////////////////
    // Load the image for sampling //
    /////////////////////////////////
    int tex_channels;
    ivec2 tex_dims;
    stbi_uc *pixels = stbi_load("data/textures/texture.jpg", &tex_dims.w, &tex_dims.h, &tex_channels, STBI_rgb_alpha);
    if (!pixels) {
        return err_code::PLATFORM_INIT_FAIL;
    }

    rndr->default_image_ind = vkr_add_image(dev, {});
    auto dest_image = &dev->images[rndr->default_image_ind];

    vkr_image_cfg cfg{};
    cfg.dims = {tex_dims, 1};
    cfg.mip_levels = 1;
    cfg.type = VK_IMAGE_TYPE_2D;
    cfg.array_layers = 1;
    cfg.format = VK_FORMAT_R8G8B8A8_SRGB;
    cfg.tiling = VK_IMAGE_TILING_OPTIMAL;
    cfg.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    cfg.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    cfg.samples = VK_SAMPLE_COUNT_1_BIT;
    cfg.vma_alloc = &dev->vma_alloc;

    int err = vkr_init_image(dest_image, &cfg);
    if (err != err_code::VKR_NO_ERROR) {
        stbi_image_free(pixels);        
        return err;
    }

    sizet imsize = cfg.dims.x*cfg.dims.y*cfg.dims.z*4;
    vkr_stage_and_upload_image_data(dest_image, pixels, imsize, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);

    stbi_image_free(pixels);

    ///////////////////////////////////////
    // Create the image view and sampler //
    ///////////////////////////////////////
    vkr_image_view_cfg iview_cfg{};
    iview_cfg.view_type = VK_IMAGE_VIEW_TYPE_2D;
    iview_cfg.image = dest_image;
    iview_cfg.srange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    iview_cfg.srange.layerCount = 1;
    iview_cfg.srange.levelCount = 1;
    
    rndr->default_image_view_ind = vkr_add_image_view(dev);
    auto iview_ptr = &dev->image_views[rndr->default_image_view_ind];
    err = vkr_init_image_view(iview_ptr, &iview_cfg, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_sampler_cfg samp_cfg{};
    for (int i = 0; i < 3; ++i) {
        samp_cfg.address_mode_uvw[i] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    samp_cfg.mag_filter = VK_FILTER_LINEAR;
    samp_cfg.min_filter = VK_FILTER_LINEAR;
    samp_cfg.anisotropy_enable = true;
    samp_cfg.max_anisotropy = vk->inst.pdev_info.props.limits.maxSamplerAnisotropy;
    samp_cfg.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samp_cfg.compare_op = VK_COMPARE_OP_ALWAYS;
    samp_cfg.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    rndr->default_sampler_ind = vkr_add_sampler(dev);
    auto samp_ptr = &dev->samplers[rndr->default_sampler_ind];
    err = vkr_init_sampler(samp_ptr, &samp_cfg, vk);
    return err;
}


intern int setup_rendering(renderer *rndr)
{
    ilog("Setting up default rendering...");
    auto vk = rndr->vk;
    auto dev = &rndr->vk->inst.device;
    
    int err = setup_render_pass(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = setup_pipeline(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_init_swapchain_framebuffers(dev, vk, &dev->render_passes[rndr->render_pass_ind], nullptr);

    err = create_vertex_indice_buffers(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = load_default_image_and_sampler(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Create uniform buffers and descriptor sets pointing to them for each frame //
    ////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < dev->rframes.size; ++i) {
        // Create a uniform buffer for each frame
        vkr_buffer_cfg buf_cfg{};
        vkr_buffer uniform_buf{};
        buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        buf_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buf_cfg.buffer_size = sizeof(uniform_buffer_object);
        buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        buf_cfg.vma_alloc = &dev->vma_alloc;

        int err = vkr_init_buffer(&uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].uniform_buffer_ind = vkr_add_buffer(dev, uniform_buf);

        // Add descriptor sets for this frame - we are running a single set with two bindings at the moment
        auto desc_ind = vkr_add_descriptor_sets(&dev->rframes[i].desc_pool, vk, &dev->pipelines[rndr->pipeline_ind].descriptor_layouts[0], 1);
        if (desc_ind.err_code != err_code::VKR_NO_ERROR) {
            return desc_ind.err_code;
        }

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.offset = 0;
        buffer_info.range = buf_cfg.buffer_size;
        buffer_info.buffer = uniform_buf.hndl;

        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = dev->image_views[rndr->default_image_view_ind].hndl;
        image_info.sampler = dev->samplers[rndr->default_sampler_ind].hndl;

        VkWriteDescriptorSet desc_write[2]{};
        desc_write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write[0].dstSet = dev->rframes[i].desc_pool.desc_sets[desc_ind.begin].hndl;
        desc_write[0].dstBinding = 0;
        desc_write[0].dstArrayElement = 0;
        desc_write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_write[0].descriptorCount = 1;
        desc_write[0].pBufferInfo = &buffer_info;

        desc_write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write[1].dstSet = dev->rframes[i].desc_pool.desc_sets[desc_ind.begin].hndl;
        desc_write[1].dstBinding = 1;
        desc_write[1].dstArrayElement = 0;
        desc_write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_write[1].descriptorCount = 1;
        desc_write[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(dev->hndl, 2, desc_write, 0, nullptr);
    }
    return err_code::VKR_NO_ERROR;
}

intern void record_command_buffer(vkr_command_buffer *cmd_buf,
                                  vkr_framebuffer *fb,
                                  vkr_pipeline *pipeline,
                                  vkr_buffer *vert_buf,
                                  vkr_buffer *ind_buf,
                                  vkr_descriptor_set *desc_set)
{
    vkr_begin_cmd_buf(cmd_buf);
    vkr_cmd_begin_rpass(cmd_buf, fb);

    vkCmdBindPipeline(cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->hndl);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = fb->size.w;
    viewport.height = fb->size.h;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buf->hndl, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {fb->size.w, fb->size.h};
    vkCmdSetScissor(cmd_buf->hndl, 0, 1, &scissor);

    VkBuffer vert_bufs[] = {vert_buf->hndl};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf->hndl, 0, 1, vert_bufs, offsets);

    vkCmdBindIndexBuffer(cmd_buf->hndl, ind_buf->hndl, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, 0, 1, &desc_set->hndl, 0, nullptr);
    vkCmdDrawIndexed(cmd_buf->hndl, 6, 1, 0, 0, 0);

    vkr_cmd_end_rpass(cmd_buf);
    vkr_end_cmd_buf(cmd_buf);
}

int renderer_init(renderer *rndr, void *win_hndl, mem_arena *fl_arena)
{
    assert(fl_arena->alloc_type == mem_alloc_type::FREE_LIST);
    rndr->upstream_fl_arena = fl_arena;
    rndr->vk_free_list.upstream_allocator = fl_arena;
    rndr->vk_frame_linear.upstream_allocator = fl_arena;
    mem_init_arena(100 * MB_SIZE, mem_alloc_type::FREE_LIST, &rndr->vk_free_list);
    mem_init_arena(10 * MB_SIZE, mem_alloc_type::LINEAR, &rndr->vk_frame_linear);
    rndr->vk = (vkr_context *)mem_alloc(sizeof(vkr_context), &rndr->vk_free_list, 8);

    vkr_cfg vkii{"rdev",
                 {1, 0, 0},
                 {.persistent_arena = &rndr->vk_free_list, .command_arena = &rndr->vk_frame_linear},
                 LOG_TRACE,
                 win_hndl,
                 INST_CREATE_FLAGS,
                 {},
                 4,
                 ADDITIONAL_INST_EXTENSIONS,
                 ADDITIONAL_INST_EXTENSION_COUNT,
                 DEVICE_EXTENSIONS,
                 DEVICE_EXTENSION_COUNT,
                 VALIDATION_LAYERS,
                 VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, rndr->vk) != err_code::VKR_NO_ERROR) {
        return err_code::RENDER_INIT_FAIL;
    }

    int err = setup_rendering(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to initialize renderer with code %d", err);
        return err;
    }

    vec2 fbsz = platform_framebuffer_size(win_hndl);
    rndr->cvp.proj = (math::perspective(45.0f, fbsz.w / fbsz.h, 0.1f, 10.0f));
    rndr->cvp.view = (math::look_at(vec3{0.0f, 0.0f, -2.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));
    return err_code::RENDER_NO_ERROR;
}

int render_frame(renderer *rndr, int finished_frame_count)
{
    mem_reset_arena(&rndr->vk_frame_linear);
    auto dev = &rndr->vk->inst.device;

    if (platform_framebuffer_resized(rndr->vk->cfg.window)) {
        vkr_recreate_swapchain(&rndr->vk->inst, rndr->vk, 0);
    }
    
    int current_frame_ind = finished_frame_count % MAX_FRAMES_IN_FLIGHT;
    auto cur_frame = &dev->rframes[current_frame_ind];
    auto buf_ind = cur_frame->cmd_buf_ind;
    auto cmd_buf = &dev->qfams[buf_ind.pool_ind.qfam_ind].cmd_pools[buf_ind.pool_ind.pool_ind].buffers[buf_ind.buffer_ind];
    auto pipeline = &dev->pipelines[0];
    auto vert_buf = &dev->buffers[rndr->vert_buf_ind];
    auto ind_buf = &dev->buffers[rndr->ind_buf_ind];

    // Wait for the rendering to be done before starting on the next frame and then reset the fence
    vkWaitForFences(dev->hndl, 1, &cur_frame->in_flight, VK_TRUE, UINT64_MAX);

    // Acquire the image, signal the image_avail semaphore once the image has been acquired
    u32 im_ind{};
    int result = vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_frame->image_avail, VK_NULL_HANDLE, &im_ind);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        wlog("Failed to acquire swapchain image");
        return err_code::RENDER_NO_ERROR;
    }

    vkResetFences(dev->hndl, 1, &cur_frame->in_flight);

    // Update uniform buffer with some matrices
    int ubo_ind = cur_frame->uniform_buffer_ind;
    memcpy(dev->buffers[ubo_ind].mem_info.pMappedData, &rndr->cvp, sizeof(uniform_buffer_object));
    

    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is readyy
    auto fb = &dev->framebuffers[im_ind];
    auto desc_set = &cur_frame->desc_pool.desc_sets[0];
    record_command_buffer(cmd_buf, fb, pipeline, vert_buf, ind_buf, desc_set);

    // Get the info ready to submit our command buffer to the queue. We need to wait until the image avail semaphore has
    // signaled, and then we need to trigger the render finished signal once the the command buffer completes
    VkSubmitInfo submit_info{};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &cur_frame->image_avail;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf->hndl;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &cur_frame->render_finished;
    if (vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[0].hndl, 1, &submit_info, cur_frame->in_flight) != VK_SUCCESS) {
        return err_code::RENDER_SUBMIT_QUEUE_FAIL;
    }

    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &cur_frame->render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[0].hndl, &present_info);
    return err_code::RENDER_NO_ERROR;
}

void renderer_terminate(renderer *rndr)
{
    auto dev = &rndr->vk->inst.device;
    vkr_terminate(rndr->vk);
    mem_free(rndr->vk, &rndr->vk_free_list);
    mem_terminate_arena(&rndr->vk_free_list);
    mem_terminate_arena(&rndr->vk_frame_linear);
}

}
