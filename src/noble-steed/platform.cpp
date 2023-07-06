#include "GLFW/glfw3.h"
#include "assert.h"
#include "platform.h"
#include "logging.h"

namespace noble_steed
{

#define platform_ptr(win) (platform_ctxt*)glfwGetWindowUserPointer(win)
intern void glfw_error_callback(i32 error, const char *description)
{
    elog("Error %d: %s", error, description);
}

intern void glfw_key_press_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    platform_ctxt *pf = platform_ptr(window);
    dlog("Key pressed fc:%d key:%d scancode:%d action:%d mods:%d", pf->finished_frames, key, scancode, action, mods);
}

intern void glfw_mouse_button_callback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    platform_ctxt *pf = platform_ptr(window);
    dlog("Mouse button pressed fc:%d button:%d action:%d mods:%d", pf->finished_frames, button, action, mods);
}

intern void glfw_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    platform_ctxt *pf = platform_ptr(window);
    dlog("Scroll fc:%d with x_offset:%f and y_offset:%f", pf->finished_frames, x_offset, y_offset);
}

intern void glfw_cursor_pos_callback(GLFWwindow *window, double x_pos, double y_pos)
{
    platform_ctxt *pf = platform_ptr(window);
    dlog("Cursor fc:%d moved to %f %f", pf->finished_frames, x_pos, y_pos);
}

intern void glfw_resize_window_callback(GLFWwindow *window, i32 width, i32 height)
{
    dlog("Resizing bgfx with framebuffer size {%d %d}", width, height);
}

intern void glfw_focus_change_callback(GLFWwindow *window, i32 focused)
{
    dlog("Focus Change");
}

intern void glfw_close_window_callback(GLFWwindow *window)
{
    dlog("Closing window...");
}

intern void glfw_iconify_window_callback(GLFWwindow *window, i32 iconified)
{
    dlog("Iconified");
}

intern void glfw_maximize_window_callback(GLFWwindow *window, i32 maximized)
{
    dlog("Maximize");
}

intern void glfw_window_position_callback(GLFWwindow *window, i32 x_pos, i32 y_pos)
{
    dlog("Window position moved to {%d %d}", x_pos, y_pos);
}

intern void glfw_framebuffer_resized_callback(GLFWwindow *window, i32 width, i32 height)
{
    dlog("Resized framebuffer to {%d %d}", width, height);
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

    glfwSetInputMode(glfw_win, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
}

void *platform_alloc(sizet byte_size)
{
    return malloc(byte_size);
}

void platform_free(void *block)
{
    free(block);
}

int platform_init(const platform_init_info *settings, platform_ctxt *ctxt)
{
    ilog("Platform init");
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        elog("GLFW init failed - closing");
        return err_code::PLATFORM_INIT;
    }

    ctxt->win_hndl = platform_create_window(&settings->wind);
    if (!ctxt->win_hndl) {
        elog("Failed to create window");
        return err_code::PLATFORM_INIT;
    }

    set_glfw_callbacks(ctxt);

    auto mon = glfwGetPrimaryMonitor();
    vec2 scale;
    glfwGetMonitorContentScale(mon, &scale.x, &scale.y);
    ilog("Monitor scale is {%f %f}", scale.x, scale.y);

    log_set_level(LOG_TRACE);

    mem_store_init(500 * MB_SIZE, MEM_ALLOC_FREE_LIST, &ctxt->mem);
    mem_store_init(20 * MB_SIZE, MEM_ALLOC_STACK, &ctxt->frame_mem);
    set_global_allocator(&ctxt->mem);

    return err_code::PLATFORM_NO_ERROR;
}

int platform_terminate(platform_ctxt *ctxt)
{
    set_global_allocator(nullptr);

    ilog("Releasing %d bytes of %d total allocated in frame mem store", ctxt->frame_mem.used, ctxt->frame_mem.total_size);
    mem_store_terminate(&ctxt->frame_mem);
    ilog("Releasing %d bytes of %d total allocated in free list mem store", ctxt->mem.used, ctxt->mem.total_size);
    mem_store_terminate(&ctxt->mem);

    ilog("Platform terminate");
    return err_code::PLATFORM_NO_ERROR;
}

void *platform_create_window(const platform_window_init_info *settings)
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

    if (check_flags(settings->win_flags, platform_window_flags::SCALE_TO_MONITOR)) {
        vec2 scale;
        glfwGetMonitorContentScale(monitor, &scale.x, &scale.y);
        sz = vec2(sz) * scale;
    }

    bool fullsreen = false;
    if (check_flags(settings->win_flags, platform_window_flags::FULLSCREEN)) {
        glfwWindowHint(GLFW_AUTO_ICONIFY, (int)check_flags(settings->win_flags, platform_window_flags::FULLSCREEN_AUTO_ICONIFTY));
        glfwWindowHint(GLFW_CENTER_CURSOR, (int)check_flags(settings->win_flags, platform_window_flags::FULLSCREEN_CENTER_CURSOR));
    }
    else {
        glfwWindowHint(GLFW_VISIBLE, (int)check_flags(settings->win_flags, platform_window_flags::VISIBLE));
        glfwWindowHint(GLFW_DECORATED, (int)check_flags(settings->win_flags, platform_window_flags::DECORATED));
        glfwWindowHint(GLFW_MAXIMIZED, (int)check_flags(settings->win_flags, platform_window_flags::MAXIMIZE));
        glfwWindowHint(GLFW_FLOATING, (int)check_flags(settings->win_flags, platform_window_flags::ALWAYS_ON_TOP));
        monitor = nullptr;
    }
    return glfwCreateWindow(sz.x, sz.y, settings->title, monitor, nullptr);
}

void platform_window_poll_input(void *window_hndl)
{
    glfwPollEvents();
}

bool platform_window_should_close(void *window_hndl)
{
    return glfwWindowShouldClose((GLFWwindow *)window_hndl);
}


#include "unistd.h"
void platform_run_frame(platform_ctxt *ctxt)
{
    ptimer_split(&ctxt->time_pts);
    dlog("Frame %d elapsed:%f", ctxt->finished_frames, NSEC_TO_SEC(ctxt->time_pts.dt_ns));
    int prod{0};
    for (int i = 0; i < 1000000000; ++i) {
        prod = i*2 + 5;
    }
    dlog("Product: %d", prod);
    platform_window_poll_input(ctxt->win_hndl);
    mem_store_reset(&ctxt->frame_mem);
    ++ctxt->finished_frames;
}

} // namespace noble_steed
