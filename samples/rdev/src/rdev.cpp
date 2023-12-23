#include "platform.h"
#include "renderer.h"

using namespace nslib;

struct app_data
{
    renderer rndr{};
};

int init(platform_ctxt *ctxt, void*user_data)
{
    auto app = (app_data*)user_data;
    return renderer_init(&app->rndr, ctxt->win_hndl, &ctxt->arenas.free_list);
}

int run_frame(platform_ctxt *ctxt, void*user_data)
{
    auto app = (app_data*)user_data;
    return render_frame(&app->rndr, ctxt->finished_frames);
}

int terminate(platform_ctxt *ctxt, void*user_data)
{
    auto app = (app_data*)user_data;
    renderer_terminate(&app->rndr);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {800,600};
    settings->wind.title = "RDEV";
    settings->user_cb.init = init;
    settings->user_cb.run_frame = run_frame;
    settings->user_cb.terminate = terminate;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform);
