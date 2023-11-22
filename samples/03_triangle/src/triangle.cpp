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

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");
    version_info v{1, 0, 0};

    vkr_init_info vkii{"03 Triangle",
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
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data)
