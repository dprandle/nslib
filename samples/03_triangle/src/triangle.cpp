#include "binary_archive.h"
#include "string_archive.h"
#include "robj_common.h"
#include "platform.h"
#include "vkrenderer.h"
#include "logging.h"
#include "math/vector4.h"

using namespace nslib;

struct app_data
{
    vkr_context vk;
};

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920,1080};
    settings->wind.title = "03 Triangle";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");
    version_info v{1,0,0};
    vkr_init_info vkii{"03 Triangle", {1,0,0}, {}, LOG_TRACE, ctxt->win_hndl};
    if (vkr_init(&vkii, &app->vk) != err_code::VKR_NO_ERROR) {
        return err_code::PLATFORM_INIT;
    }    
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
