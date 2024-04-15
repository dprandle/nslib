#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "GLFW/glfw3.h"
#include "input_kmcodes.h"
#include "platform.h"
#include "logging.h"
#include "containers/cjson.h"

namespace nslib
{

#define platform_ptr(win) (platform_ctxt *)glfwGetWindowUserPointer(win)
intern void glfw_error_callback(i32 error, const char *description)
{
    elog("Error %d: %s", error, description);
}


platform_window_event * get_latest_window_event(platform_window_event_type type, platform_frame_window_events *fwind)
{
    for (sizet i = fwind->events.size - 1; i >= 0; --i) {
        if (fwind->events[i].type == type) {
            return &fwind->events[i];
        }
    }
    return nullptr;
}

bool frame_has_event_type(platform_window_event_type type, const platform_frame_window_events *fwind)
{
    for (int i = 0; i < fwind->events.size; ++i) {
        if (fwind->events[i].type == type) {
            return true;
        }
    }
    return false;
}

intern i32 get_cursor_scroll_mod_mask(GLFWwindow *window)
{
    i32 ret{0};
    if (glfwGetKey(window, KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        ret |= KEY_MOD_SHIFT;
    }
    if (glfwGetKey(window, KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        ret |= KEY_MOD_CONTROL;
    }
    if (glfwGetKey(window, KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, KEY_RIGHT_ALT) == GLFW_PRESS) {
        ret |= KEY_MOD_ALT;
    }
    if (glfwGetKey(window, KEY_LEFT_SUPER) == GLFW_PRESS || glfwGetKey(window, KEY_RIGHT_SUPER) == GLFW_PRESS) {
        ret |= KEY_MOD_SUPER;
    }
    if (glfwGetKey(window, KEY_CAPS_LOCK) == GLFW_PRESS) {
        ret |= KEY_MOD_CAPS_LOCK;
    }
    if (glfwGetKey(window, KEY_NUM_LOCK) == GLFW_PRESS) {
        ret |= KEY_MOD_NUM_LOCK;
    }
    if (glfwGetMouseButton(window, MOUSE_BTN_LEFT)) {
        ret |= CURSOR_SCROLL_MOD_MOUSE_LEFT;
    }
    if (glfwGetMouseButton(window, MOUSE_BTN_RIGHT)) {
        ret |= CURSOR_SCROLL_MOD_MOUSE_RIGHT;
    }
    if (glfwGetMouseButton(window, MOUSE_BTN_MIDDLE)) {
        ret |= CURSOR_SCROLL_MOD_MOUSE_MIDDLE;
    }
    return ret;
}

intern void glfw_key_press_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->finp.events.size == pf->finp.events.capacity) {
        pf->finp.events.size = 0;
    }
    arr_push_back(&pf->finp.events, {platform_input_event_type::KEY_PRESS, key, scancode, action, mods, {}, {}, window});
}

intern void glfw_mouse_button_callback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->finp.events.size == pf->finp.events.capacity) {
        pf->finp.events.size = 0;
    }
    arr_push_back(&pf->finp.events, {platform_input_event_type::MOUSE_BTN, button, {}, action, mods, {}, {}, window});
}

intern void glfw_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    vec2 foffset{(float)x_offset, (float)y_offset};
    platform_ctxt *pf = platform_ptr(window);
    if (pf->finp.events.size == pf->finp.events.capacity) {
        pf->finp.events.size = 0;
    }
    arr_push_back(
        &pf->finp.events,
        {platform_input_event_type::SCROLL, SCROLL_CHANGE, {}, {}, get_cursor_scroll_mod_mask(window), foffset, {}, window});
}

intern void glfw_cursor_pos_callback(GLFWwindow *window, double x_pos, double y_pos)
{
    vec2 fpos{(float)x_pos, (float)y_pos};
    platform_ctxt *pf = platform_ptr(window);
    if (pf->finp.events.size == pf->finp.events.capacity) {
        pf->finp.events.size = 0;
    }
    arr_push_back(
        &pf->finp.events,
        {platform_input_event_type::CURSOR_POS, CURSOR_POS_CHANGE, {}, {}, get_cursor_scroll_mod_mask(window), {}, fpos, window});
}

intern void glfw_focus_change_callback(GLFWwindow *window, i32 focused)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->fwind.events.size == pf->fwind.events.capacity) {
        pf->fwind.events.size = 0;
    }
    platform_window_event we{platform_window_event_type::FOCUS};
    we.window = window;
    we.focus = focused;
    arr_push_back(&pf->fwind.events, we);
    dlog("Focus Change");
}

intern void glfw_close_window_callback(GLFWwindow *window)
{
    dlog("Closing window...");
}

intern void glfw_iconify_window_callback(GLFWwindow *window, i32 iconified)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->fwind.events.size == pf->fwind.events.capacity) {
        pf->fwind.events.size = 0;
    }
    platform_window_event we{platform_window_event_type::ICONIFIED};
    we.window = window;
    we.iconified = iconified;
    arr_push_back(&pf->fwind.events, we);
    dlog("Window %s", (iconified)?"iconified":"restored");
}

intern void glfw_maximize_window_callback(GLFWwindow *window, i32 maximized)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->fwind.events.size == pf->fwind.events.capacity) {
        pf->fwind.events.size = 0;
    }
    platform_window_event we{platform_window_event_type::MAXIMIZED};
    we.window = window;
    we.iconified = maximized;
    arr_push_back(&pf->fwind.events, we);
    dlog("Window %s", (maximized)?"maximized":"restored");
}

intern void glfw_window_position_callback(GLFWwindow *window, i32 x_pos, i32 y_pos)
{
    platform_ctxt *pf = platform_ptr(window);
    if (pf->fwind.events.size == pf->fwind.events.capacity) {
        pf->fwind.events.size = 0;
    }
    platform_window_event we{platform_window_event_type::MOVE};
    we.window = window;
    we.move.first = {x_pos, y_pos};
    we.resize.second = pf->fwind.pos;
    pf->fwind.pos = we.resize.first;
    arr_push_back(&pf->fwind.events, we);
}

intern void glfw_resize_window_callback(GLFWwindow *window, i32 width, i32 height)
{
    platform_ctxt *pf = platform_ptr(window);
    platform_window_event *ev_ptr = get_latest_window_event(platform_window_event_type::WIN_RESIZE, &pf->fwind);
    if (!ev_ptr) {
        if (pf->fwind.events.size == pf->fwind.events.capacity) {
            pf->fwind.events.size = 0;
        }
        platform_window_event we{platform_window_event_type::WIN_RESIZE};
        we.window = window;
        ev_ptr = arr_push_back(&pf->fwind.events, we);
    }
    ev_ptr->resize.first = {width, height};
    ev_ptr->resize.second = pf->fwind.win_size;
    pf->fwind.win_size = ev_ptr->resize.first;
    dlog("Resized window from {%d %d} to {%d %d}", ev_ptr->resize.second.w, ev_ptr->resize.second.h, width, height);
}

intern void glfw_framebuffer_resized_callback(GLFWwindow *window, i32 width, i32 height)
{
    platform_ctxt *pf = platform_ptr(window);
    platform_window_event *ev_ptr = get_latest_window_event(platform_window_event_type::FB_RESIZE, &pf->fwind);
    if (!ev_ptr) {
        if (pf->fwind.events.size == pf->fwind.events.capacity) {
            pf->fwind.events.size = 0;
        }
        platform_window_event we{platform_window_event_type::FB_RESIZE};
        we.window = window;
        ev_ptr = arr_push_back(&pf->fwind.events, we);
    }
    ev_ptr->resize.first = {width, height};
    ev_ptr->resize.second = pf->fwind.fb_size;
    pf->fwind.fb_size = ev_ptr->resize.first;
    dlog("Resized framebuffer from {%d %d} to {%d %d}", ev_ptr->resize.second.w, ev_ptr->resize.second.h, width, height);
}

intern void set_glfw_callbacks(platform_ctxt *ctxt)
{
    auto glfw_win = (GLFWwindow *)ctxt->win_hndl;
    glfwSetWindowUserPointer(glfw_win, ctxt);

    glfwSetWindowSizeCallback(glfw_win, glfw_resize_window_callback);
    glfwSetWindowCloseCallback(glfw_win, glfw_close_window_callback);
    glfwSetWindowMaximizeCallback(glfw_win, glfw_maximize_window_callback);
    glfwSetWindowIconifyCallback(glfw_win, glfw_iconify_window_callback);
    glfwSetWindowPosCallback(glfw_win, glfw_window_position_callback);
    glfwSetWindowFocusCallback(glfw_win, glfw_focus_change_callback);
    glfwSetFramebufferSizeCallback(glfw_win, glfw_framebuffer_resized_callback);
    glfwSetKeyCallback(glfw_win, glfw_key_press_callback);
    glfwSetMouseButtonCallback(glfw_win, glfw_mouse_button_callback);
    glfwSetScrollCallback(glfw_win, glfw_scroll_callback);
    glfwSetCursorPosCallback(glfw_win, glfw_cursor_pos_callback);

    glfwSetInputMode(glfw_win, GLFW_LOCK_KEY_MODS, GLFW_FALSE);
}

intern void init_mem_arenas(const platform_memory_init_info *info, platform_memory *mem)
{
    // Null to indicate these get platform_alloc'd
    mem_init_fl_arena(&mem->free_list, info->free_list_size, nullptr);
    mem_init_stack_arena(&mem->stack, info->stack_size, nullptr);
    mem_init_lin_arena(&mem->frame_linear, info->frame_linear_size, nullptr);

    // Then these become our global mem arenas
    mem_set_global_arena(&mem->free_list);
    mem_set_global_stack_arena(&mem->stack);
    mem_set_global_frame_lin_arena(&mem->frame_linear);

    // Set up our json alloc and free funcs
    json_hooks hooks;
    hooks.malloc_fn = mem_alloc;
    hooks.free_fn = mem_free;
    json_init_hooks(&hooks);
}

intern void terminate_mem_arenas(platform_memory *mem)
{
    mem_terminate_arena(&mem->stack);
    mem_terminate_arena(&mem->frame_linear);
    mem_terminate_arena(&mem->free_list);
    mem_set_global_arena(nullptr);
    mem_set_global_stack_arena(nullptr);
    mem_set_global_frame_lin_arena(nullptr);
}

void *platform_alloc(sizet byte_size)
{
    return malloc(byte_size);
}

void platform_free(void *block)
{
    free(block);
}

void *platform_realloc(void *ptr, sizet byte_size)
{
    return realloc(ptr, byte_size);
}

int init_platform(const platform_init_info *settings, platform_ctxt *ctxt)
{
    ilog("Platform init version %d.%d.%d", NSLIB_VERSION_MAJOR, NSLIB_VERSION_MINOR, NSLIB_VERSION_PATCH);
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        elog("GLFW init failed - closing");
        return err_code::PLATFORM_INIT_FAIL;
    }

    ctxt->win_hndl = create_platform_window(&settings->wind);
    if (!ctxt->win_hndl) {
        elog("Failed to create window");
        return err_code::PLATFORM_INIT_FAIL;
    }

    glfwGetFramebufferSize((GLFWwindow*)ctxt->win_hndl, &ctxt->fwind.fb_size.x, &ctxt->fwind.fb_size.y);
    glfwGetWindowSize((GLFWwindow*)ctxt->win_hndl, &ctxt->fwind.win_size.x, &ctxt->fwind.win_size.y);
    glfwGetWindowPos((GLFWwindow*)ctxt->win_hndl, &ctxt->fwind.pos.x, &ctxt->fwind.pos.y);

    set_glfw_callbacks(ctxt);

    // Seed random number generator
    srand((u32)time(NULL));

    auto mon = glfwGetPrimaryMonitor();
    vec2 scale;
    glfwGetMonitorContentScale(mon, &scale.x, &scale.y);
    ilog("Monitor scale is {%f %f}", scale.x, scale.y);

    log_set_level(settings->default_log_level);
    init_mem_arenas(&settings->mem, &ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

int terminate_platform(platform_ctxt *ctxt)
{
    ilog("Platform terminate");
    terminate_mem_arenas(&ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

void *create_platform_window(const platform_window_init_info *settings)
{
    assert(settings);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    ivec2 sz = settings->resolution;

    if (test_flags(settings->win_flags, platform_window_flags::SCALE_TO_MONITOR)) {
        vec2 scale;
        glfwGetMonitorContentScale(monitor, &scale.x, &scale.y);
        sz = vec2(sz) * scale;
    }

    bool fullsreen = false;
    if (test_flags(settings->win_flags, platform_window_flags::FULLSCREEN)) {
        glfwWindowHint(GLFW_AUTO_ICONIFY, (int)test_flags(settings->win_flags, platform_window_flags::FULLSCREEN_AUTO_ICONIFTY));
        glfwWindowHint(GLFW_CENTER_CURSOR, (int)test_flags(settings->win_flags, platform_window_flags::FULLSCREEN_CENTER_CURSOR));
    }
    else {
        glfwWindowHint(GLFW_VISIBLE, (int)test_flags(settings->win_flags, platform_window_flags::VISIBLE));
        glfwWindowHint(GLFW_DECORATED, (int)test_flags(settings->win_flags, platform_window_flags::DECORATED));
        glfwWindowHint(GLFW_MAXIMIZED, (int)test_flags(settings->win_flags, platform_window_flags::MAXIMIZE));
        glfwWindowHint(GLFW_FLOATING, (int)test_flags(settings->win_flags, platform_window_flags::ALWAYS_ON_TOP));
        monitor = nullptr;
    }
    return glfwCreateWindow(sz.x, sz.y, settings->title, monitor, nullptr);
}

void process_platform_window_input(platform_ctxt *pf)
{
    arr_clear(&pf->finp.events);
    arr_clear(&pf->fwind.events);
    ilog("before: %lu %lu", pf->finp.events.size, pf->fwind.events.size);
    glfwPollEvents();
    wlog("after: %lu %lu", pf->finp.events.size, pf->fwind.events.size);
}

bool platform_window_should_close(void *window_hndl)
{
    return glfwWindowShouldClose((GLFWwindow *)window_hndl);
}

ivec2 get_window_size(void *win)
{
    platform_ctxt *pf = platform_ptr((GLFWwindow*)win);
    return pf->fwind.win_size;
}

ivec2 get_framebuffer_size(void *win)
{
    platform_ctxt *pf = platform_ptr((GLFWwindow*)win);
    return pf->fwind.fb_size;
}

vec2 get_cursor_pos(void *window_hndl)
{
    GLFWwindow *glfw_win = (GLFWwindow *)(window_hndl);
    dvec2 ret;
    glfwGetCursorPos(glfw_win, &ret.x, &ret.y);
    return ret;
}

vec2 get_normalized_cursor_pos(void *window_hndl)
{
    return get_cursor_pos(window_hndl) / vec2(get_framebuffer_size(window_hndl));
}

bool platform_framebuffer_resized(void *win_hndl)
{
    platform_ctxt *pf = platform_ptr((GLFWwindow*)win_hndl);
    return frame_has_event_type(platform_window_event_type::FB_RESIZE, &pf->fwind);
}

bool platform_window_resized(void *win_hndl)
{
    platform_ctxt *pf = platform_ptr((GLFWwindow*)win_hndl);
    return frame_has_event_type(platform_window_event_type::WIN_RESIZE, &pf->fwind);
}

void start_platform_frame(platform_ctxt *ctxt)
{
    ptimer_split(&ctxt->time_pts);
    process_platform_window_input(ctxt);
    mem_reset_arena(&ctxt->arenas.frame_linear);
}

void end_platform_frame(platform_ctxt *ctxt)
{
    ++ctxt->finished_frames;
}

intern sizet platform_file_size(FILE *f, platform_file_err_desc *err)
{
    sizet ret{0};

    sizet cur_p = ftell(f);
    if (cur_p == -1L)
        goto ftell_fail;

    if (fseek(f, 0, SEEK_END) != 0)
        goto fseek_fail;

    ret = ftell(f);
    if (ret == -1L)
        goto ftell_fail;

    if (fseek(f, (long)cur_p, SEEK_SET) != 0)
        goto fseek_fail;

    return ret;

fseek_fail:
    if (err) {
        err->code = err_code::FILE_SEEK_FAIL;
        err->str = strerror(errno);
    }
    return ret;

ftell_fail:
    if (err) {
        err->code = err_code::FILE_TELL_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet file_size(const char *fname, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, "r");
    if (f) {
        ret = platform_file_size(f, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

intern sizet platform_read_file(FILE *f, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet nelems{0};
    if (byte_offset != 0 && fseek(f, (long)byte_offset, SEEK_SET) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
    }
    else {
        nelems = fread(data, element_size, nelements, f);
    }
    return nelems;
}

sizet read_file(const char *fname,
                         const char *mode,
                         void *data,
                         sizet element_size,
                         sizet nelements,
                         sizet byte_offset,
                         platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        elems = platform_read_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", data, element_size, nelements, byte_offset, err);
}

sizet read_file(const char *fname, const char *mode, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        sizet fsize = platform_file_size(f, err);
        if (fsize > 0) {
            arr_resize(buffer, fsize);
            elems = platform_read_file(f, buffer->data, 1, buffer->size, byte_offset, err);
        }
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", buffer, byte_offset, err);
}

intern sizet platform_write_file(FILE *f, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    int seek = SEEK_SET;
    if (byte_offset == -1) {
        byte_offset = 0;
        seek = SEEK_END;
    }

    if ((byte_offset != 0 || seek != SEEK_SET) && fseek(f, (long)byte_offset, seek) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
        return ret;
    }

    ret = fwrite(data, element_size, nelements, f);
    if (ret != nelements && err) {
        err->code = err_code::FILE_WRITE_DIFF_SIZE;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname,
                          const char *mode,
                          const void *data,
                          sizet element_size,
                          sizet nelements,
                          sizet byte_offset,
                          platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, element_size, nelements, byte_offset, err);
}

sizet write_file(const char *fname, const char *mode, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, 1, data->size, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, byte_offset, err);
}

} // namespace nslib
