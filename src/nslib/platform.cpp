#include "GLFW/glfw3.h"
#include "input_kmcodes.h"
#include "platform.h"
#include "logging.h"

namespace nslib
{

#define platform_ptr(win) (platform_ctxt *)glfwGetWindowUserPointer(win)
intern void glfw_error_callback(i32 error, const char *description)
{
    elog("Error %d: %s", error, description);
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
    assert(pf->finp.count < MAX_PLATFORM_INPUT_FRAME_EVENTS);
    pf->finp.events[pf->finp.count] = {PLATFORM_INPUT_EVENT_TYPE_KEY_PRESS, key, scancode, action, mods, {}, {}, window};
    ++pf->finp.count;
}

intern void glfw_mouse_button_callback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    platform_ctxt *pf = platform_ptr(window);
    assert(pf->finp.count < MAX_PLATFORM_INPUT_FRAME_EVENTS);
    pf->finp.events[pf->finp.count] = {PLATFORM_INPUT_EVENT_TYPE_MOUSE_BTN, button, {}, action, mods, {}, {}, window};
    ++pf->finp.count;
}

intern void glfw_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    platform_ctxt *pf = platform_ptr(window);
    pf->finp.events[pf->finp.count] = {
        PLATFORM_INPUT_EVENT_TYPE_SCROLL, SCROLL_CHANGE, {}, {}, get_cursor_scroll_mod_mask(window), {x_offset, y_offset}, {}, window};
    ++pf->finp.count;
}

intern void glfw_cursor_pos_callback(GLFWwindow *window, double x_pos, double y_pos)
{
    platform_ctxt *pf = platform_ptr(window);
    pf->finp.events[pf->finp.count] = {
        PLATFORM_INPUT_EVENT_TYPE_CURSOR_POS, CURSOR_POS_CHANGE, {}, {}, get_cursor_scroll_mod_mask(window), {}, {x_pos, y_pos}, window};
    ++pf->finp.count;
}

intern void glfw_resize_window_callback(GLFWwindow *window, i32 width, i32 height)
{
    dlog("Resizing with size {%d %d}", width, height);
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
{}

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

    glfwSetInputMode(glfw_win, GLFW_LOCK_KEY_MODS, GLFW_FALSE);
}

intern void init_mem_arenas(const platform_memory_init_info * info, platform_memory *mem)
{
    mem_init_arena(info->free_list_size, MEM_ALLOC_FREE_LIST, &mem->free_list);
    mem_init_arena(info->stack_size, MEM_ALLOC_STACK, &mem->stack);
    mem_init_arena(info->frame_linear_size, MEM_ALLOC_LINEAR, &mem->frame_linear);
    mem_set_global_arena(&mem->free_list);
    mem_set_global_stack_arena(&mem->stack);
    mem_set_global_frame_lin_arena(&mem->frame_linear);
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

    // Seed random number generator
    srand(time(NULL));

    auto mon = glfwGetPrimaryMonitor();
    vec2 scale;
    glfwGetMonitorContentScale(mon, &scale.x, &scale.y);
    ilog("Monitor scale is {%f %f}", scale.x, scale.y);

    log_set_level(LOG_TRACE);
    init_mem_arenas(&settings->mem, &ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

int platform_terminate(platform_ctxt *ctxt)
{
    ilog("Platform terminate");
    terminate_mem_arenas(&ctxt->arenas);
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

void platform_window_process_input(platform_ctxt *pf)
{
    pf->finp.count = 0;
    glfwPollEvents();
}

bool platform_window_should_close(void *window_hndl)
{
    return glfwWindowShouldClose((GLFWwindow *)window_hndl);
}

dvec2 platform_window_size(void *window_hndl)
{
    GLFWwindow *glfw_win = (GLFWwindow *)(window_hndl);
    ivec2 ret;
    glfwGetWindowSize(glfw_win, &ret.x, &ret.y);
    return ret;
}

dvec2 platform_cursor_pos(void *window_hndl)
{
    GLFWwindow *glfw_win = (GLFWwindow *)(window_hndl);
    dvec2 ret;
    glfwGetCursorPos(glfw_win, &ret.x, &ret.y);
    return ret;
}

void platform_run_frame(platform_ctxt *ctxt)
{
    ptimer_split(&ctxt->time_pts);
    platform_window_process_input(ctxt);
    if (ctxt->arenas.frame_linear.used > 0) {
        dlog("Clearing %d used bytes from frame linear arena", ctxt->arenas.frame_linear.used);
    }
    mem_reset_arena(&ctxt->arenas.frame_linear);
    ++ctxt->finished_frames;
}

} // namespace nslib
