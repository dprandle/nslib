#pragma once
#include "memory.h"
#include "profile_timer.h"
#include "math/vector2.h"
#include "containers/array.h"
#include "util.h"

namespace nslib
{

namespace err_code
{
enum platform
{
    PLATFORM_NO_ERROR,
    PLATFORM_INIT_FAIL,
    PLATFORM_RUN_FRAME_FAIL,
    PLATFORM_TERMINATE_FAIL
};

enum file
{
    FILE_NO_ERROR,
    FILE_OPEN_FAIL,
    FILE_TELL_FAIL,
    FILE_SEEK_FAIL,
    FILE_READ_DIFF_SIZE,
    FILE_WRITE_DIFF_SIZE,
    FILE_GET_CWD_FAIL
};

} // namespace err_code

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

enum struct platform_input_event_type
{
    INVALID = -1,
    KEY_PRESS,
    MOUSE_BTN,
    SCROLL,
    CURSOR_POS
};

enum struct platform_window_event_type
{
    INVALID = -1,
    WIN_RESIZE,
    FB_RESIZE,
    MOVE,
    FOCUS,
    ICONIFIED,
    MAXIMIZED,
};

struct platform_input_event
{
    platform_input_event_type type{platform_input_event_type::INVALID};
    i32 key_or_button{};
    i32 scancode{};
    i32 action{};
    i32 mods{};
    vec2 offset;
    vec2 pos;
    void *win_hndl;
};

struct platform_window_event
{
    platform_window_event_type type{platform_window_event_type::INVALID};
    void *window{};
    union
    {
        // First is new size, second is prev size - for window resize these are screen coords, for fb resize they are pixels
        pair<ivec2, ivec2> resize{};
        // First is new pos, second is prev pos
        pair<ivec2, ivec2> move;
        // Lost focus is 0, gained focus is 1
        int focus;
        // Iconified is 1, restored is 0
        int iconified;
        // Maximized is 1, restored is 0
        int maximized;
    };
};

struct platform_frame_input_events
{
    static_array<platform_input_event, 255> events{};
};

struct platform_frame_window_events
{
    static_array<platform_window_event, 32> events{};

    // Screen coords
    ivec2 win_size;
    ivec2 pos;

    // Pixels
    ivec2 fb_size;
};

struct platform_memory
{
    mem_arena free_list{};
    mem_arena stack{};
    mem_arena frame_linear{};
};

struct platform_ctxt
{

    void *win_hndl{};
    profile_timepoints time_pts{};
    platform_frame_input_events finp;
    platform_frame_window_events fwind;
    platform_memory arenas{};
    int finished_frames{0};
    char **argv{};
    int argc;
};

struct platform_file_err_desc
{
    int code;
    const char *str;
};

struct platform_window_init_info
{
    i16 win_flags{platform_window_flags::VISIBLE | platform_window_flags::DECORATED | platform_window_flags::INTIALLY_FOCUSED};
    ivec2 resolution;
    const char *title;
};

struct platform_memory_init_info
{
    sizet free_list_size{1000 * MB_SIZE};
    sizet stack_size{100 * MB_SIZE};
    sizet frame_linear_size{100 * MB_SIZE};
};

using platform_user_cb = int(platform_ctxt *ctxt, void *user_data);

struct platform_user_callbacks
{
    platform_user_cb *init;
    platform_user_cb *run_frame;
    platform_user_cb *terminate;
};

struct platform_init_info
{
    int argc;
    char **argv;
    platform_user_callbacks user_cb;
    platform_window_init_info wind;
    platform_memory_init_info mem;
    int default_log_level{LOG_TRACE};
};

int init_platform(const platform_init_info *config, platform_ctxt *ctxt);
int terminate_platform(platform_ctxt *ctxt);

void *platform_alloc(sizet byte_size);
void *platform_realloc(void *ptr, sizet byte_size);
void platform_free(void *block);

void start_platform_frame(platform_ctxt *ctxt);
void end_platform_frame(platform_ctxt *ctxt);

void *create_platform_window(const platform_window_init_info *pf_config);

ivec2 get_window_size(void *window_hndl);
ivec2 get_framebuffer_size(void *window_hndl);

vec2 get_cursor_pos(void *window_hndl);
vec2 get_normalized_cursor_pos(void *window_hndl);

platform_window_event *get_latest_window_event(platform_window_event_type type, platform_frame_window_events *fwind);
bool frame_has_event_type(platform_window_event_type type, const platform_frame_window_events *fwind);
void process_platform_window_input(platform_ctxt *pf);
bool platform_framebuffer_resized(void *win_hndl);
bool platform_window_resized(void *win_hndl);
bool platform_window_should_close(void *window_hndl);

sizet file_size(const char *fname, platform_file_err_desc *err);

sizet read_file(const char *fname,
                const char *mode,
                void *data,
                sizet element_size,
                sizet nelements,
                sizet byte_offset = 0,
                platform_file_err_desc *err = nullptr);

sizet read_file(const char *fname, void *data, sizet element_size, sizet nelements, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

// If 0, vec will be resized to entire file size
sizet read_file(const char *fname, const char *mode, byte_array *buffer, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet read_file(const char *fname, byte_array *buffer, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname,
                 const char *mode,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset = 0,
                 platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset = 0,
                 platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname, const char *mode, const byte_array *data, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

sizet write_file(const char *fname, const byte_array *data, sizet byte_offset = 0, platform_file_err_desc *err = nullptr);

} // namespace nslib

// int config_platform_func(nslib::platform_cfg *config, client_app_data_type *user_data);
#define __MAIN_BLOCK__(config_platform_func)                                                                                               \
    using namespace nslib;                                                                                                                 \
    bool run_loop{true};                                                                                                                   \
    platform_init_info pf_config{argc, argv};                                                                                              \
    if (config_platform_func(&pf_config, &user_data) != err_code::PLATFORM_NO_ERROR) {                                                     \
        return err_code::PLATFORM_INIT_FAIL;                                                                                               \
    }                                                                                                                                      \
    ctxt.argc = pf_config.argc;                                                                                                            \
    ctxt.argv = pf_config.argv;                                                                                                            \
    if (init_platform(&pf_config, &ctxt) != err_code::PLATFORM_NO_ERROR) {                                                                 \
        return err_code::PLATFORM_INIT_FAIL;                                                                                               \
    }                                                                                                                                      \
    if (pf_config.user_cb.init) {                                                                                                          \
        int err = pf_config.user_cb.init(&ctxt, &user_data);                                                                               \
        if (err != err_code::PLATFORM_NO_ERROR) {                                                                                          \
            elog("User init failed with code %d", err);                                                                                    \
            return terminate_platform(&ctxt);                                                                                              \
        }                                                                                                                                  \
    }                                                                                                                                      \
    ptimer_restart(&ctxt.time_pts);                                                                                                        \
    while (run_loop && !platform_window_should_close(ctxt.win_hndl)) {                                                                     \
        start_platform_frame(&ctxt);                                                                                                       \
        if (pf_config.user_cb.run_frame && pf_config.user_cb.run_frame(&ctxt, &user_data) != err_code::PLATFORM_NO_ERROR) {                \
            run_loop = false;                                                                                                              \
        }                                                                                                                                  \
        end_platform_frame(&ctxt);                                                                                                         \
    }                                                                                                                                      \
    if (pf_config.user_cb.terminate) {                                                                                                     \
        int err = pf_config.user_cb.terminate(&ctxt, &user_data);                                                                          \
        if (err != err_code::PLATFORM_NO_ERROR) {                                                                                          \
            elog("User terminate failed with code %d", err);                                                                               \
        }                                                                                                                                  \
    }                                                                                                                                      \
    return terminate_platform(&ctxt);

#define DEFINE_APPLICATION_MAIN_STATIC(user_type, config_platform_func)                                                                    \
    user_type user_data{};                                                                                                                 \
    nslib::platform_ctxt ctxt{};                                                                                                           \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        __MAIN_BLOCK__(config_platform_func)                                                                                               \
    }

#define DEFINE_APPLICATION_MAIN(user_type, config_platform_func)                                                                           \
    int main(int argc, char **argv)                                                                                                        \
    {                                                                                                                                      \
        user_type user_data{};                                                                                                             \
        nslib::platform_ctxt ctxt{};                                                                                                       \
        __MAIN_BLOCK__(config_platform_func)                                                                                               \
    }
