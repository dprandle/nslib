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
intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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

void record_command_buffer(vkr_command_buffer *cmd_buf, vkr_framebuffer *fb, vkr_pipeline *pipeline)
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

    vkCmdDraw(cmd_buf->hndl, 3, 1, 0, 0);
    
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
    vkr_setup_default_rendering(&app->vk);
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{
    ilog("App terminate");
    vkr_terminate(&app->vk);
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
//    record_draw_triangle_command_buffer(app);
    auto dev = &app->vk.inst.device;
    auto cmd_buf = &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].cmd_pools[0].buffers[0];
    auto pipeline = &dev->pipelines[0];
    
    u32 im_ind{};
    vkAcquireNextImageKHR(dev->hndl, dev->sw_info.swapchain, UINT64_MAX, dev->image_avail, VK_NULL_HANDLE, &im_ind);

    auto fb = &dev->framebuffers[im_ind];
    vkResetCommandBuffer(cmd_buf->hndl, 0);
    record_command_buffer(cmd_buf, fb, pipeline);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {dev->image_avail};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf->hndl;

    VkSemaphore signal_semaphores[] = {dev->render_finished};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(dev->hndl, 1, &dev->in_flight);
    if (vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[0].hndl, 1, &submit_info, dev->in_flight) != VK_SUCCESS) {
        return err_code::PLATFORM_RUN_FRAME;
    }    

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = {dev->sw_info.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps

    vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[0].hndl, &present_info);
    
    vkWaitForFences(dev->hndl, 1, &dev->in_flight, VK_TRUE, UINT64_MAX);
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data)
