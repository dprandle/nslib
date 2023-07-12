#pragma once
#include "mem.h"
#include "profile_timer.h"
#include "math/vector2.h"

namespace nslib
{
constexpr const u8 MAX_PLATFORM_INPUT_FRAME_EVENTS = 255;
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

enum platform_input_event_type
{
    PLATFORM_INPUT_EVENT_TYPE_KEY_PRESS,
    PLATFORM_INPUT_EVENT_TYPE_MOUSE_BTN,
    PLATFORM_INPUT_EVENT_TYPE_SCROLL,
    PLATFORM_INPUT_EVENT_TYPE_CURSOR_POS
};

struct platform_input_event
{
    int type{-1};
    i32 key_or_button{};
    i32 scancode{};
    i32 action{};
    i32 mods{};
    dvec2 offset;
    dvec2 pos;
    void *win_hndl;
};

struct platform_frame_input
{
    platform_input_event events[MAX_PLATFORM_INPUT_FRAME_EVENTS];
    u8 count{0};
};

struct platform_ctxt
{
    void *win_hndl{};
    profile_timepoints time_pts{};
    platform_frame_input finp{};
    mem_arena mem{};
    mem_arena frame_mem{};
    int finished_frames{0};
};

int platform_init(const platform_init_info *settings, platform_ctxt *ctxt);
int platform_terminate(platform_ctxt *ctxt);

void *platform_alloc(sizet byte_size);
void *platform_realloc(void *ptr, sizet byte_size);
void platform_free(void *block);
void platform_run_frame(platform_ctxt *ctxt);

void *platform_create_window(const platform_window_init_info *settings);

dvec2 platform_window_size(void *window_hndl);
dvec2 platform_cursor_pos(void *window_hndl);

void platform_window_process_input(platform_ctxt *pf);
bool platform_window_should_close(void *window_hndl);

} // namespace nslib

#define DEFINE_APPLICATION_MAIN(client_app_data_type)                                                                                      \
    client_app_data_type client_app_data{};                                                                                                \
    nslib::platform_ctxt ctxt{};                                                                                                           \
    int load_platform_settings(nslib::platform_init_info *settings, client_app_data_type *client_app_data);                                \
    int app_init(nslib::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                                       \
    int app_terminate(nslib::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                                  \
    int app_run_frame(nslib::platform_ctxt *ctxt, client_app_data_type *client_app_data);                                                  \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        using namespace nslib;                                                                                                             \
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
