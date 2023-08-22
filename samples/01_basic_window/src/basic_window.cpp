#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{};

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {800,600};
    settings->wind.title = "01 Basic Window";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");    
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{
    ilog("App terminate");
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data)
