#include "binary_archive.h"
#include "string_archive.h"
#include "robj_common.h"
#include "platform.h"
#include "vkrenderer.h"
#include "logging.h"
#include "math/vector4.h"

using namespace nslib;

#if defined(NDEBUG)
intern const u32 VALIDATION_LAYER_COUNT = 0;
intern const char **VALIDATION_LAYERS = nullptr;
#else
intern const u32 VALIDATION_LAYER_COUNT = 1;
intern const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
#endif

intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 2;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                                                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

#if __APPLE__
intern const u32 DEVICE_EXTENSION_COUNT = 2;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
#else
intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif
struct app_data
{
    vkr_context vk;
};

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "03 Triangle";
    return err_code::PLATFORM_NO_ERROR;
}

void setup_rendering(vkr_context *vk)
{
    ilog("Setting up default rendering...");
    sizet rpass_ind = vkr_add_render_pass(&vk->inst.device, {});
    vkr_init_render_pass(vk, &vk->inst.device.render_passes[rpass_ind]);

    vkr_pipeline_cfg info{};

    const char *fnames[] = {"shaders/triangle.vert.spv","shaders/triangle.frag.spv"};
    for (int i = 0; i <= VKR_SHADER_STAGE_FRAG; ++i) {
        platform_file_err_desc err{};
        platform_read_file(fnames[i], &info.shader_stages[i].code, 0, &err);
        if (err.code != err_code::PLATFORM_NO_ERROR) {
            wlog("Error reading file %s from disk (code %d): %s", fnames[i], err.code, err.str);
            return;
        }
        info.shader_stages[i].entry_point = "main";
    }

    info.rpass = &vk->inst.device.render_passes[rpass_ind];
    sizet pipe_ind = vkr_add_pipeline(&vk->inst.device, {});
    vkr_init_pipeline(vk, &info, &vk->inst.device.pipelines[pipe_ind]);
    vkr_init_swapchain_framebuffers(&vk->inst.device, vk, info.rpass, nullptr);
}

void record_command_buffer(vkr_command_buffer *cmd_buf, vkr_framebuffer *fb, vkr_pipeline *pipeline, vkr_buffer *vert_buf)
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

    vkCmdDraw(cmd_buf->hndl, vert_buf->size, 1, 0, 0);

    vkr_cmd_end_rpass(cmd_buf);
    vkr_end_cmd_buf(cmd_buf);
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");
    version_info v{1, 0, 0};

    vkr_cfg vkii{"03 Triangle",
                 {1, 0, 0},
                 {},
                 LOG_TRACE,
                 ctxt->win_hndl,
                 ADDITIONAL_INST_EXTENSIONS,
                 ADDITIONAL_INST_EXTENSION_COUNT,
                 DEVICE_EXTENSIONS,
                 DEVICE_EXTENSION_COUNT,
                 VALIDATION_LAYERS,
                 VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, &app->vk) != err_code::VKR_NO_ERROR) {
        return err_code::PLATFORM_INIT;
    }
    setup_rendering(&app->vk);

    auto dev = &app->vk.inst.device;
    sizet ind = vkr_add_buffer(dev, {});
    vkr_init_buffer(&dev->buffers[ind], &app->vk);

    void *data{};
    vkMapMemory(dev->hndl, dev->buffers[ind].mem_hndl, 0, dev->buffers[ind].size, 0, &data);
    memcpy(data, verts, dev->buffers[ind].size);
    vkUnmapMemory(dev->hndl, dev->buffers[ind].mem_hndl);
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{
    ilog("App terminate");
    auto dev = &app->vk.inst.device;
    vkr_terminate(&app->vk);
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    auto dev = &app->vk.inst.device;

    if (platform_framebuffer_resized(ctxt->win_hndl)) {
        vkr_recreate_swapchain(&app->vk.inst, &app->vk, ctxt->win_hndl, 0);
    }

    int rframe_ind = ctxt->finished_frames % VKR_RENDER_FRAME_COUNT;
    auto cur_frame = &dev->rframes[rframe_ind];
    auto buf_ind = cur_frame->cmd_buf_ind;
    auto cmd_buf = &dev->qfams[buf_ind.qfam_ind].cmd_pools[buf_ind.pool_ind].buffers[buf_ind.buffer_ind];
    auto pipeline = &dev->pipelines[0];
    auto vert_buf = &dev->buffers[0];

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

    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is readyy
    auto fb = &dev->framebuffers[im_ind];
    record_command_buffer(cmd_buf, fb, pipeline, vert_buf);

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
        return err_code::PLATFORM_RUN_FRAME;
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

DEFINE_APPLICATION_MAIN(app_data)
