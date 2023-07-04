#pragma once
#include "mem.h"
#include "profile_timer.h"
#include "math/vector2.h"

namespace noble_steed
{

namespace err_code
{
enum platform
{
    PLATFORM_NO_ERROR,
    PLATFORM_INIT,
    PLATFORM_RUN_FRAME,
    PLATFORM_TERMINATE
};
}

struct platform_window_flags
{
    enum
    {
        VISIBLE = 1,          // Ignored for full screen windows
        INTIALLY_FOCUSED = 2, // Ignored for full screen and initially hidden windows
        DECORATED = 4,        // Ignored for full screen windows
        MAXIMIZE = 8,         // Ignored for full screen
        ALWAYS_ON_TOP = 16,   // Ignored for full screen
        FULLSCREEN = 32,
        FULLSCREEN_AUTO_ICONIFTY = 64,  // Ignored for non full screen windows
        FULLSCREEN_CENTER_CURSOR = 128, // Ignored for non full screen windows
        SCALE_TO_MONITOR = 256
    };
};

struct platform_window_init_info
{
    i16 win_flags{platform_window_flags::VISIBLE | platform_window_flags::DECORATED | platform_window_flags::INTIALLY_FOCUSED};
    ivec2 resolution;
    const char *title;
};

struct platform_init_info
{
    platform_window_init_info wind;
};

struct platform_ctxt
{
    void *win_hndl;
    profile_timepoints time_pts;
    mem_store mem{};
    mem_store frame_mem{};
    int finished_frames{0};
};

int platform_init(const platform_init_info *settings, platform_ctxt *ctxt);
int platform_terminate(platform_ctxt *ctxt);

void *platform_alloc(sizet byte_size);
void platform_free(void *block);
void platform_run_frame(platform_ctxt *ctxt);

void *platform_create_window(const platform_window_init_info *settings);
void platform_window_poll_input(void *window_hndl);
bool platform_window_should_close(void *window_hndl);

} // namespace noble_steed

#define DEFINE_APPLICATION_MAIN(client_app_data_type)                                                                                      \
    client_app_data_type client_app_data{};                                                                                                \
    noble_steed::platform_ctxt ctxt{};                                                                                                     \
    int load_platform_settings(noble_steed::platform_init_info *settings, client_app_data_type *client_app_data);                          \
    int app_init(noble_steed::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                                 \
    int app_terminate(noble_steed::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                            \
    int app_run_frame(noble_steed::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                            \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        using namespace noble_steed;                                                                                                       \
        bool run_loop{true};                                                                                                               \
        platform_init_info settings{};                                                                                                     \
        if (load_platform_settings(&settings, &client_app_data) != err_code::PLATFORM_NO_ERROR) {                                          \
            return err_code::PLATFORM_INIT;                                                                                                \
        }                                                                                                                                  \
        if (platform_init(&settings, &ctxt) != err_code::PLATFORM_NO_ERROR) {                                                              \
            return err_code::PLATFORM_INIT;                                                                                                \
        }                                                                                                                                  \
        if (app_init(&ctxt, &client_app_data) != err_code::PLATFORM_NO_ERROR) {                                                            \
            return err_code::PLATFORM_INIT;                                                                                                \
        }                                                                                                                                  \
        ptimer_restart(&ctxt.time_pts);                                                                                                    \
        while (run_loop && !platform_window_should_close(ctxt.win_hndl)) {                                                                 \
            platform_run_frame(&ctxt);                                                                                                     \
            if (app_run_frame(&ctxt, &client_app_data) != err_code::PLATFORM_NO_ERROR) {                                                   \
                run_loop = false;                                                                                                          \
            }                                                                                                                              \
        }                                                                                                                                  \
        if (app_terminate(&ctxt, &client_app_data) != err_code::PLATFORM_NO_ERROR) {                                                       \
            return err_code::PLATFORM_TERMINATE;                                                                                           \
        }                                                                                                                                  \
        if (platform_terminate(&ctxt) != err_code::PLATFORM_NO_ERROR) {                                                                    \
            return err_code::PLATFORM_TERMINATE;                                                                                           \
        }                                                                                                                                  \
        return err_code::PLATFORM_NO_ERROR;                                                                                                \
    }
