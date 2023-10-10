#include "input_mapping.h"
#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{

};

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "04 Containers";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    array<int> arr1;
    arr_init(&arr1, &ctxt->arenas.free_list);
    arr_push_back(&arr1, 35);
    arr_push_back(&arr1, 22);
    arr_push_back(&arr1, 12);
    arr_push_back(&arr1, 9);
    arr_push_back(&arr1, -122);

    for (int i = 0; i < arr1.size; ++i) {
        ilog("Arr1[%d]: %d", i, arr1[i]);
        auto arr2(arr1);
        for (int i = 0; i < arr2.size; ++i) {
            ilog("Arr2[%d]: %d", i, arr2[i]);
        }
    }
    
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{

    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    return err_code::PLATFORM_TERMINATE;
}

DEFINE_APPLICATION_MAIN(app_data)
