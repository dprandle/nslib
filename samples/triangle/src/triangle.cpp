#include "binary_archive.h"
#include "input_kmcodes.h"
#include "string_archive.h"
#include "robj_common.h"
#include "platform.h"
#include "vk_context.h"
#include "logging.h"
#include "math/matrix4.h"

using namespace nslib;

struct uniform_buffer_object
{
    mat4 model;
    mat4 view;
    mat4 proj;
};

struct vertex
{
    vec2 pos;
    vec3 color;
};

const vertex verts[] = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

const u16 indices[] = {0, 1, 2};

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
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                                                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 2;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
#else
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = {};
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 1;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif
struct app_data
{
    mem_arena vk_free_list{};
    mem_arena vk_frame_linear{};
    vkr_context vk;
    u32 vert_buf_ind;
    u32 ind_buf_ind;
    uniform_buffer_object cvp;

    int move_target{0};
    int move_right{0};
};

int setup_rendering(app_data *app)
{
    auto vk = &app->vk;
    ilog("Setting up default rendering...");
    sizet rpass_ind = vkr_add_render_pass(&app->vk.inst.device, {});

    vkr_rpass_cfg rp_cfg{};

    VkAttachmentDescription col_att{};
    col_att.format = app->vk.inst.device.swapchain.format;
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

    vkr_init_render_pass(vk, &rp_cfg, &vk->inst.device.render_passes[rpass_ind]);

    vkr_pipeline_cfg info{};

    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Descripitor Set Layouts just a single uniform buffer for now
    info.set_layouts[0].bindings[0].binding = 0;
    info.set_layouts[0].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[0].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    info.set_layouts[0].bindings[0].descriptorCount = 1;
    ++info.set_layouts[0].bindings.size;
    ++info.set_layouts.size;

    // Vertex binding:
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(vertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    arr_push_back(&info.vert_binding_desc, binding_desc);

    // Attribute Descriptions - so far we just have two
    VkVertexInputAttributeDescription attrib_desc{};
    attrib_desc.binding = 0;
    attrib_desc.location = 0;
    attrib_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, pos);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);
    attrib_desc.binding = 0;
    attrib_desc.location = 1;
    attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, color);
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
    const char *fnames[] = {"data/shaders/triangle.vert.spv", "data/shaders/triangle.frag.spv"};
    for (int i = 0; i <= VKR_SHADER_STAGE_FRAG; ++i) {
        platform_file_err_desc err{};
        platform_read_file(fnames[i], &info.shader_stages[i].code, 0, &err);
        if (err.code != err_code::PLATFORM_NO_ERROR) {
            wlog("Error reading file %s from disk (code %d): %s", fnames[i], err.code, err.str);
            return err.code;
        }
        info.shader_stages[i].entry_point = "main";
    }

    info.rpass = &vk->inst.device.render_passes[rpass_ind];
    sizet pipe_ind = vkr_add_pipeline(&vk->inst.device, {});
    int err = vkr_init_pipeline(vk, &info, &vk->inst.device.pipelines[pipe_ind]);
    if (err != err_code::PLATFORM_NO_ERROR) {
        return err;
    }
    vkr_init_swapchain_framebuffers(&vk->inst.device, vk, info.rpass, nullptr);

    auto dev = &vk->inst.device;

    // Create vertex buffer on GPU
    vkr_buffer_cfg b_cfg{};
    app->vert_buf_ind = vkr_add_buffer(dev, {});
    app->ind_buf_ind = vkr_add_buffer(dev, {});

    // Common to all buffer options
    b_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    b_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    b_cfg.vma_alloc = &dev->vma_alloc;

    // Vert buffer
    b_cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = sizeof(vertex) * 3;
    err = vkr_init_buffer(&dev->buffers[app->vert_buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Init and copy data to staging buffer, then copy staging buf to vert buffer, then delete staging buf
    vkr_stage_and_upload_buffer_data(&dev->buffers[app->vert_buf_ind], verts, b_cfg.buffer_size, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);

    // Ind buffer
    b_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = sizeof(u32) * 3;
    err = vkr_init_buffer(&dev->buffers[app->ind_buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Init and copy data to staging buffer, then copy staging buf to vert buffer, then delete staging buf
    vkr_stage_and_upload_buffer_data(&dev->buffers[app->ind_buf_ind], indices, b_cfg.buffer_size, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);

    // Create uniform buffers and descriptor sets pointing to them for each frame
    for (int i = 0; i < dev->rframes.size; ++i) {
        vkr_buffer_cfg buf_cfg{};
        vkr_buffer uniform_buf{};
        buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        buf_cfg.vma_alloc = &dev->vma_alloc;
        buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        buf_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buf_cfg.buffer_size = sizeof(uniform_buffer_object);
        buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        int err = vkr_init_buffer(&uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].uniform_buffer_ind = vkr_add_buffer(dev, uniform_buf);

        auto desc_ind = vkr_add_descriptor_sets(&dev->rframes[i].desc_pool, vk, &dev->pipelines[pipe_ind].descriptor_layouts[0]);
        if (desc_ind.err_code != err_code::VKR_NO_ERROR) {
            return desc_ind.err_code;
        }

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.offset = 0;
        buffer_info.range = buf_cfg.buffer_size;
        buffer_info.buffer = uniform_buf.hndl;

        VkWriteDescriptorSet desc_write{};
        desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write.dstSet = dev->rframes[i].desc_pool.desc_sets[desc_ind.begin].hndl;
        desc_write.dstBinding = 0;
        desc_write.dstArrayElement = 0;
        desc_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_write.descriptorCount = 1;
        desc_write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(dev->hndl, 1, &desc_write, 0, nullptr);
    }
    return err_code::VKR_NO_ERROR;
}

int record_command_buffer(vkr_command_buffer *cmd_buf,
                          vkr_framebuffer *fb,
                          vkr_pipeline *pipeline,
                          vkr_buffer *vert_buf,
                          vkr_buffer *ind_buf,
                          vkr_descriptor_set *desc_set)
{
    int err = vkr_begin_cmd_buf(cmd_buf);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }
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
    err = vkr_end_cmd_buf(cmd_buf);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }
    return err_code::VKR_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    ilog("App init");
    version_info v{1, 0, 0};

    mem_init_arena(100 * MB_SIZE, mem_alloc_type::FREE_LIST, &app->vk_free_list);
    mem_init_arena(10 * MB_SIZE, mem_alloc_type::LINEAR, &app->vk_frame_linear);

    vkr_cfg vkii{"Triangle",
                 {1, 0, 0},
                 {.persistent_arena = &app->vk_free_list, .command_arena = &app->vk_frame_linear},
                 LOG_TRACE,
                 ctxt->win_hndl,
                 INST_CREATE_FLAGS,
                 {},
                 4,
                 ADDITIONAL_INST_EXTENSIONS,
                 ADDITIONAL_INST_EXTENSION_COUNT,
                 DEVICE_EXTENSIONS,
                 DEVICE_EXTENSION_COUNT,
                 VALIDATION_LAYERS,
                 VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, &app->vk) != err_code::VKR_NO_ERROR) {
        return err_code::PLATFORM_INIT_FAIL;
    }

    int err = setup_rendering(app) != err_code::PLATFORM_NO_ERROR;
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vec2 fbsz(ctxt->fwind.fb_size);
    app->cvp.proj = (math::perspective(45.0f, fbsz.w / fbsz.h, 0.1f, 10.0f));
    app->cvp.view = (math::look_at(vec3{0.0f, 0.0f, -2.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    ilog("App terminate");
    auto dev = &app->vk.inst.device;
    vkr_terminate(&app->vk);
    return err_code::PLATFORM_NO_ERROR;
}

int render_frame(platform_ctxt *ctxt, app_data *app)
{
    mem_reset_arena(app->vk.cfg.arenas.command_arena);
    auto dev = &app->vk.inst.device;

    if (platform_framebuffer_resized(ctxt->win_hndl)) {
        vkr_recreate_swapchain(&app->vk.inst, &app->vk, 0);
    }

    int rframe_ind = ctxt->finished_frames % dev->rframes.size;
    auto cur_frame = &dev->rframes[rframe_ind];
    auto buf_ind = cur_frame->cmd_buf_ind;
    auto cmd_buf = &dev->qfams[buf_ind.pool_ind.qfam_ind].cmd_pools[buf_ind.pool_ind.pool_ind].buffers[buf_ind.buffer_ind];
    auto pipeline = &dev->pipelines[0];
    auto vert_buf = &dev->buffers[app->vert_buf_ind];
    auto ind_buf = &dev->buffers[app->ind_buf_ind];

    // Wait for the rendering to be done before starting on the next frame and then reset the fence
    vkWaitForFences(dev->hndl, 1, &cur_frame->in_flight, VK_TRUE, UINT64_MAX);

    // Acquire the image, signal the image_avail semaphore once the image has been acquired
    u32 im_ind{};
    int result = vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_frame->image_avail, VK_NULL_HANDLE, &im_ind);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        wlog("Failed to acquire swapchain image");
        return err_code::PLATFORM_NO_ERROR;
    }

    vkResetFences(dev->hndl, 1, &cur_frame->in_flight);

    // Update uniform buffer with some matrices
    int ubo_ind = cur_frame->uniform_buffer_ind;
    memcpy(dev->buffers[ubo_ind].mem_info.pMappedData, &app->cvp, sizeof(uniform_buffer_object));

    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is readyy
    auto fb = &dev->framebuffers[im_ind];
    auto desc_set = &cur_frame->desc_pool.desc_sets[0];
    int err = record_command_buffer(cmd_buf, fb, pipeline, vert_buf, ind_buf, desc_set);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

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
        return err_code::PLATFORM_RUN_FRAME_FAIL;
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

    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    vec3 dir = math::target(app->cvp.view);
    vec3 right = math::right(app->cvp.view);
    vec3 cur_pos = math::translation_component(app->cvp.view);

    for (int ie = 0; ie < ctxt->finp.events.size; ++ie) {
        auto ev = &ctxt->finp.events[ie];
        if (ev->type == platform_input_event_type::KEY_PRESS) {
            if (ev->action == INPUT_ACTION_PRESS) {
                ilog("Current pos on start:%s", to_cstr(cur_pos));
                if (ev->key_or_button == KEY_W) {
                    app->move_target = 1;
                }
                if (ev->key_or_button == KEY_S) {
                    app->move_target = -1;
                }
                if (ev->key_or_button == KEY_D) {
                    app->move_right = 1;
                }
                if (ev->key_or_button == KEY_A) {
                    app->move_right = -1;
                }
            }
            else if (ev->action == INPUT_ACTION_RELEASE) {
                ilog("Current pos on release:%s", to_cstr(cur_pos));
                if (ev->key_or_button == KEY_W || ev->key_or_button == KEY_S) {
                    app->move_target = 0;
                }
                if (ev->key_or_button == KEY_D || ev->key_or_button == KEY_A) {
                    app->move_right = 0;
                }
            }
        }
    }
    if (app->move_target != 0) {
        cur_pos -= dir * ctxt->time_pts.dt * app->move_target;
        math::set_mat_column(&app->cvp.view, VIEW_MATRIX_COL_POS, cur_pos);
    }
    if (app->move_right != 0) {
        cur_pos -= right * ctxt->time_pts.dt * app->move_right;
        math::set_mat_column(&app->cvp.view, VIEW_MATRIX_COL_POS, cur_pos);
    }

    return render_frame(ctxt, app);
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "Triangle";
    settings->user_cb.init = app_init;
    settings->user_cb.run_frame = app_run_frame;
    settings->user_cb.terminate = app_terminate;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
